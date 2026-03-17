#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include "commons/log.h"

#include <utils/conexiones.h>
#include <utils/serializacion.h>
#include <utils/deserializacion.h>
#include <utils/estructuras.h>

#include "hilo-master.h"
#include "config.h"
#include "serializacion.h"
#include "deserializacion.h"

static int fd_master = -1;
extern t_log* logger;
volatile bool query_control_activo = true;
static volatile bool cierre_local_solicitado = false;

static int conectar_a_master();
static void handshake_con_master(char* archivo_query, int prioridad);
static void* atender_master(void* arg);

void qc_solicitar_cierre(void) {
    cierre_local_solicitado = true;   // <- importante para el log del motivo
    query_control_activo = false;
    if (fd_master >= 0) {
        shutdown(fd_master, SHUT_RDWR);  // despierta el recv() bloqueante
    }
}

void inicializar_master(char* archivo_query, int prioridad){
    fd_master = conectar_a_master();
    char* query = archivo_query;
    int priori = prioridad;
    
    handshake_con_master(query, priori);
    pthread_t hilo_master;
    pthread_create(&hilo_master, NULL, atender_master, NULL);
    
    pthread_join(hilo_master, NULL);
    
}

static int conectar_a_master(){
    char* ip_master = obtener_ip_master();
    char* puerto_master = obtener_puerto_master();
    int fd = conectar_a_servidor(ip_master, puerto_master);

    if(fd < 0){
        log_debug(logger, "Error al intentar conectar con Master en %s:%s", ip_master, puerto_master);
        exit(EXIT_FAILURE);
        return -1;
    }

    log_info(logger, "## Conexión al Master exitosa. IP: %s, Puerto: %s", ip_master, puerto_master);

    return fd;
}

static void handshake_con_master(char* archivo_query, int prioridad){
    log_info(logger, "## Solicitud de ejecución de Query: %s, prioridad: %d", archivo_query, prioridad);

    t_paquete* handshake = serializar_handshake_master(archivo_query, prioridad);
    enviar_paquete(handshake, fd_master);
    
    t_header header = obtener_header(fd_master);
    log_debug(logger, "recibido: %d - esperado: %d", header, HANDSHAKE_OK);

    if (header != HANDSHAKE_OK){
        log_debug(logger, "Handshake fallido: Master rechazó la conexión");
        close(fd_master);
        exit(EXIT_FAILURE);
    }

    log_debug(logger, "Handshake con Master establecido");

}

static void* atender_master(void* arg){
    (void)arg;

    log_debug(logger, "Iniciando loop de recepción desde Master...");
    bool finalizacion_recibida = false;

    while (query_control_activo) {

        t_header header = obtener_header(fd_master);
        
        log_debug(logger, "Recibo header %d, esperaba %d", header, QC_FINALIZACION);
        if(header == 0) continue;
        if (header < 0) {
            if (!query_control_activo) {
                log_debug(logger, "Conexión cerrada normalmente");
            } else {
                log_debug(logger, "Error al recibir header del Master: %s", strerror(errno));
            }
            break;
        }

        void* payload = obtener_payload(fd_master);
        if (payload == NULL) {  
            log_debug(logger, "Error al recibir payload");
            break;
        }

        t_deserializado_query_control* mensaje = deserializar_mensaje_query_control(header, payload);

        if (mensaje == NULL) {
            log_debug(logger, "Deserialización fallida para header %d. Headers esperados: QC_LECTURA=%d, QC_FINALIZACION=%d", header, QC_LECTURA, QC_FINALIZACION);
            log_debug(logger, "Deserialización de mensaje de Query Control fallida. Cortando conexión.");
            destruir_payload(payload);
            break;
        }

        switch (header) {
            case QC_LECTURA: {
                char* nombre_file = mensaje->payload.qc_lectura.nombre_file;
                char* tag = mensaje->payload.qc_lectura.tag;
                char* contenido = mensaje->payload.qc_lectura.contenido;

                log_info(logger, "## Lectura realizada: Archivo <%s:%s>, contenido: <%s>.", nombre_file, tag, contenido);
                break;
            }

            case QC_FINALIZACION: {
                char* motivo = mensaje->payload.qc_finalizacion.motivo;

                log_info(logger, "## Query Finalizada - %s", motivo ? : "DESCONOCIDO");

                finalizacion_recibida = true;
                query_control_activo = false;

                break;
            }

            default: {
                log_debug(logger, "Header desconocido desde Master: %d. Cortando conexión.", mensaje->header);
                query_control_activo = false;
                break;
            }
        }

        destruir_payload(payload);
        destruir_deserializado_query_control(mensaje);
    }

    if (fd_master >= 0) {
        shutdown(fd_master, SHUT_RDWR);
        close(fd_master);
        fd_master = -1;
    }

    if (!finalizacion_recibida) {
        if (cierre_local_solicitado) {
            log_info(logger, "## Query Finalizada - DESCONEXION DEL QUERY CONTROL");
        } else {
            log_info(logger, "## Query Finalizada - DESCONEXION DEL MASTER");
        }
    }

    log_debug(logger, "Loop de recepción finalizado. Conexión con Master cerrada.");

    return NULL;
}