#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

#include "commons/log.h"

#include <utils/conexiones.h>
#include <utils/serializacion.h>
#include <utils/deserializacion.h>
#include <utils/conexiones.h>

#include "hilo-master.h"
#include "config.h"
#include "serializacion.h"

static int fd_master;
extern t_log* logger;
extern int query_control_activo;

static int conectar_a_master();
static void handshake_con_master(char* archivo_query, int prioridad);
static void atender_master();

void inicializar_master(char* archivo_query, int prioridad){
    fd_master = conectar_a_master();
    char* query = archivo_query;
    int priori = prioridad;

    handshake_con_master(query, priori);
    atender_master();
    
}

static int conectar_a_master(){
    char* ip_master = obtener_ip_master();
    char* puerto_master = obtener_puerto_master();
    int fd = conectar_a_servidor(ip_master, puerto_master);

    if(fd < 0){
        log_error(logger, "Error al intentar conectar con Master en %s:%s", ip_master, puerto_master);
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
    //void* payload = obtener_payload(fd_master);
    log_info(logger, "recibido: %d - esperado: %d", header, HANDSHAKE_OK);
    //destruir_payload(payload);

    if (header != HANDSHAKE_OK){
        log_error(logger, "Handshake fallido: Master rechazó la conexión");
        close(fd_master);
        exit(EXIT_FAILURE);
    }

    log_debug(logger, "Handshake con Kernel establecido");

}

static void atender_master(){

    for(int i =0; i<5; i++){
        sleep(10);
        log_info(logger, "simulando ejecución");
    }

    close(fd_master);
}