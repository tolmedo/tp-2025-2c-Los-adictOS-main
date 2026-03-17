#include "servidor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

#include <commons/log.h>

#include <utils/conexiones.h>
#include <utils/serializacion.h>
#include <utils/deserializacion.h>

#include "config.h"
#include "hilo-worker.h"
#include "hilo-query_control.h"
#include "serializacion.h"
#include "deserializacion.h"

extern t_log* logger;
extern t_config_master* config_master;

static int fd_servidor_master = -1;
static pthread_t thread;


volatile bool programa_activo = false;

static int _iniciar_servidor_master();
static void* _esperar_clientes(void* arg);
static void* _handshake(void* arg);

void inicializar_servidor(){
    fd_servidor_master = _iniciar_servidor_master();
    int* fd_servidor_master_ptr = malloc(sizeof(int));
    *fd_servidor_master_ptr = fd_servidor_master;

    pthread_create(&thread, NULL, _esperar_clientes, fd_servidor_master_ptr);
}

static int _iniciar_servidor_master(){
    char* puerto_escucha = obtener_puerto_escucha();
    int fd_servidor = crear_servidor(puerto_escucha);

    if(fd_servidor == -1) {
        log_error(logger, "No se ha podido inicializar el servidor Master en el puerto %s.", puerto_escucha);
        exit(EXIT_FAILURE);
        return -1;
    }

    log_info(logger, "Servidor Master Inicializado - Puerto: %s", puerto_escucha);
    
    programa_activo = true;
    return fd_servidor;
}

static void* _esperar_clientes(void* arg){
    int fd_master = *((int*) arg);
    free(arg);

    while (programa_activo) {
        int fd_cliente = aceptar_cliente(fd_master);

        if (fd_cliente == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                usleep(100000); // 100ms
                continue;
            }
            
            if (programa_activo) {
                log_error(logger, "Error en accept: %s", strerror(errno));
            }
            break;
        }

        int* fd_cliente_ptr = malloc(sizeof(int));
        *fd_cliente_ptr = fd_cliente;

        pthread_t thread_cliente;
        pthread_create(&thread_cliente, NULL, _handshake, fd_cliente_ptr);
        pthread_detach(thread_cliente);
    }

    log_info(logger, "Cerrando servidor Master...");
    if (fd_servidor_master != -1) {
        finalizar_servidor(fd_master);
    }
    
    return NULL;
}

static void* _handshake(void* arg){ 
    int* fd_master_ptr = (int*)arg;
    int fd_cliente = *fd_master_ptr;

    t_header header = obtener_header(fd_cliente);
    
    // Debug: Log del header recibido
    log_debug(logger, "Header recibido: %d", header);
    
    if (header == -1) {
        log_error(logger, "Error al recibir header del cliente");
        close(fd_cliente);
        free(fd_master_ptr);
        return NULL;
    }
    
    switch (header) {
        case HANDSHAKE_WORKER: {
            log_info(logger, "Procesando handshake de Worker");

            void* payload = obtener_payload(fd_cliente);
            if (payload == NULL) {
                log_error(logger, "Handshake Worker sin payload");
                close(fd_cliente);
                free(fd_master_ptr);
                return NULL;
            }

            t_deserializado_master* deserializado = deserializar_mensaje_master(HANDSHAKE_WORKER, payload);
            if (deserializado == NULL) {
                log_error(logger, "No se pudo deserializar handshake Worker");
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_master_ptr);
                return NULL;
            }

            int worker_id = deserializado->payload.handshake_worker.worker_id;

            t_paquete* respuesta = serializar_respuesta_handshake_worker();
            enviar_paquete(respuesta, fd_cliente);

            t_datos_worker* datos = malloc(sizeof(*datos));
            datos->fd_worker = fd_cliente;
            datos->worker_id = worker_id;

            pthread_t worker;
            pthread_create(&worker, NULL, atender_worker, datos);
            pthread_detach(worker);

            destruir_deserializado_master(deserializado);
            destruir_payload(payload);
            free(fd_master_ptr);  

            break;
        }
        case HANDSHAKE_QUERY_CONTROL: {
            log_info(logger, "Procesando handshake de Query Control");

            void* payload = obtener_payload(fd_cliente);

            if (payload == NULL) {
                log_error(logger, "Handshake Worker sin payload");
                close(fd_cliente);
                free(fd_master_ptr);
                return NULL;
            }

            t_deserializado_master* deserializado = deserializar_mensaje_master(HANDSHAKE_QUERY_CONTROL, payload);
            if (deserializado == NULL) {
                log_error(logger, "No se pudo deserializar handshake Query Control");
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_master_ptr);
                return NULL;
            }
            
            enviar_header(HANDSHAKE_OK, fd_cliente);

            t_query* datos = malloc(sizeof(*datos));
            datos->fd_query = fd_cliente;
            datos->path = deserializado->payload.handshake_query_control.archivo_query;
            datos->prioridad = deserializado->payload.handshake_query_control.prioridad;
            
            pthread_t query_control;
            pthread_create(&query_control, NULL, atender_query_control, datos);
            pthread_detach(query_control);

            destruir_deserializado_master(deserializado);
            destruir_payload(payload);
            free(fd_master_ptr);  
            
            break;
        }
        default: {
            log_error(logger, "Valor no identificado en el Handshake: %d", header);
            
            t_paquete* respuesta = serializar_respuesta_handshake_desconocido();
            enviar_paquete(respuesta, fd_cliente);
            
            close(fd_cliente);
            free(fd_master_ptr);
            break;
        }
    }

    return NULL;
}

void finalizar_programa() {
    programa_activo = false;
    
    if (fd_servidor_master != -1) {
        log_info(logger, "Cerrando servidor Master...");
        finalizar_servidor(fd_servidor_master);
        fd_servidor_master = -1;
    }
    
    if (config_master) {
        liberar_config_master();
        config_master = NULL;
    }
    
    if (logger) {
        log_info(logger, "Master finalizado correctamente");
        log_destroy(logger);
        logger = NULL;
    }
}
