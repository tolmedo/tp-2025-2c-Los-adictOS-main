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
#include <semaphore.h>

#include <commons/log.h>

#include <utils/conexiones.h>
#include <utils/serializacion.h>
#include <utils/deserializacion.h>
#include <utils/estructuras.h>

#include "config.h"
#include "hilo-worker.h"
#include "hilo-query_control.h"
#include "serializacion_master.h"
#include "deserializacion.h"

extern t_log* logger;
extern t_config_master* config_master;

static int fd_servidor_master = -1;
static pthread_t thread;

volatile bool programa_activo = false;

static int _iniciar_servidor_master();
static void* _esperar_clientes(void* arg);
static void* _handshake(void* arg);

void inicializar_servidor(void){
    fd_servidor_master = _iniciar_servidor_master();
    
    if (fd_servidor_master == -1) {
        log_error(logger, "No se pudo inicializar el servidor Master");
        exit(EXIT_FAILURE);
    }
    
    int* fd_servidor_master_ptr = malloc(sizeof(int));
    if (!fd_servidor_master_ptr) {
        log_error(logger, "Error de memoria al inicializar servidor");
        close(fd_servidor_master);
        exit(EXIT_FAILURE);
    }
    
    *fd_servidor_master_ptr = fd_servidor_master;

    if (pthread_create(&thread, NULL, _esperar_clientes, fd_servidor_master_ptr) != 0) {
        log_error(logger, "Error al crear hilo del servidor");
        free(fd_servidor_master_ptr);
        close(fd_servidor_master);
        exit(EXIT_FAILURE);
    }
    
    log_debug(logger, "Servidor Master inicializado correctamente");

}

static int _iniciar_servidor_master(){
    char* puerto_escucha = obtener_puerto_escucha();
    if (!puerto_escucha) {
        log_error(logger, "Puerto de escucha no configurado");
        return -1;
    }

    int fd_servidor = crear_servidor(puerto_escucha);

    if(fd_servidor == -1) {
        log_error(logger, "No se ha podido inicializar el servidor Master en el puerto %s", puerto_escucha);
        return -1;
    }

    log_debug(logger, "Servidor Master Inicializado - Puerto: %s", puerto_escucha);

    programa_activo = true;
    return fd_servidor;
}

static void* _esperar_clientes(void* arg){
    int fd_master = *((int*) arg);
    free(arg);

    log_debug(logger, "Hilo de aceptación de clientes iniciado");

    while (programa_activo) {
        int fd_cliente = aceptar_cliente(fd_master);

        if (fd_cliente == -1) {
            if (errno != EINTR && programa_activo) {
                log_error(logger, "Error en accept: %s", strerror(errno));
                sleep(1);
            }
            continue;
        }

        log_debug(logger, "Nuevo cliente conectado, FD: %d", fd_cliente);

        int* fd_cliente_ptr = malloc(sizeof(int));
        if (!fd_cliente_ptr) {
            log_error(logger, "Error de memoria al aceptar cliente");
            close(fd_cliente);
            continue;
        }

        *fd_cliente_ptr = fd_cliente;

        pthread_t thread_cliente;
        if (pthread_create(&thread_cliente, NULL, _handshake, fd_cliente_ptr) != 0) {
            log_error(logger, "Error al crear hilo para cliente");
            free(fd_cliente_ptr);
            close(fd_cliente);
            continue;
        }
        
        pthread_detach(thread_cliente);
    }

    log_debug(logger, "Cerrando servidor Master...");
    if (fd_servidor_master != -1) {
        finalizar_servidor(fd_servidor_master);
    }
    
    return NULL;
}

static void* _handshake(void* arg){ 
    int* fd_cliente_ptr = (int*)arg;
    int fd_cliente = *fd_cliente_ptr;
    

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    
    if (setsockopt(fd_cliente, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_debug(logger, "No se pudo configurar timeout para handshake");
    }

    t_header header = obtener_header(fd_cliente);
    
    log_debug(logger, "Header recibido: %d", header);
    
    if (header == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            log_debug(logger, "Timeout en handshake del cliente FD: %d", fd_cliente);
        } else {
            log_debug(logger, "Error al recibir header del cliente FD: %d: %s", 
                     fd_cliente, strerror(errno));
        }
        close(fd_cliente);
        free(fd_cliente_ptr);
        return NULL;
    }
    
    switch (header) {
        case HANDSHAKE_WORKER: {
            log_debug(logger, "Procesando handshake de Worker");

            void* payload = obtener_payload(fd_cliente);
            if (payload == NULL) {
                log_error(logger, "Handshake Worker sin payload - FD: %d", fd_cliente);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }

            t_deserializado_master* deserializado = deserializar_mensaje_master(HANDSHAKE_WORKER, payload);
            if (deserializado == NULL) {
                log_error(logger, "No se pudo deserializar handshake Worker - FD: %d", fd_cliente);
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }

            int worker_id = deserializado->payload.handshake_worker.worker_id;
            
            if (worker_id < 0) {
                log_error(logger, "ID de Worker inválido: %d", worker_id);
                destruir_deserializado_master(deserializado);
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }

            log_debug(logger, "Worker %d realizando handshake - FD: %d", worker_id, fd_cliente);

            t_paquete* respuesta = serializar_respuesta_handshake_worker();
            if (!respuesta) {
                log_error(logger, "Error serializando respuesta handshake Worker");
                destruir_deserializado_master(deserializado);
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }

            if (!enviar_paquete(respuesta, fd_cliente)) {
                log_error(logger, "Error enviando handshake OK a Worker %d", worker_id);
                destruir_deserializado_master(deserializado);
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }

            t_worker* datos = malloc(sizeof(*datos));
            if (!datos) {
                log_error(logger, "Error de memoria para datos Worker");
                destruir_deserializado_master(deserializado);
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }
            
            datos->fd_worker = fd_cliente;
            datos->worker_id = worker_id;

            pthread_t worker;
            if (pthread_create(&worker, NULL, atender_worker, datos) != 0) {
                log_error(logger, "Error al crear hilo para Worker %d", worker_id);
                free(datos);
                destruir_deserializado_master(deserializado);
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }
            
            pthread_detach(worker);

            destruir_deserializado_master(deserializado);
            destruir_payload(payload);
            free(fd_cliente_ptr);  

            log_debug(logger, "Handshake completado para Worker %d", worker_id);
            break;
        }
        case HANDSHAKE_QUERY_CONTROL: {
            log_debug(logger, "Procesando handshake de Query Control");

            void* payload = obtener_payload(fd_cliente);
            if (payload == NULL) {
                log_error(logger, "Handshake Query Control sin payload - FD: %d", fd_cliente);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }

            t_deserializado_master* deserializado = deserializar_mensaje_master(HANDSHAKE_QUERY_CONTROL, payload);
            if (deserializado == NULL) {
                log_error(logger, "No se pudo deserializar handshake Query Control - FD: %d", fd_cliente);
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }
            
            if (!deserializado->payload.handshake_query_control.archivo_query) {
                log_error(logger, "Archivo de query nulo en handshake - FD: %d", fd_cliente);
                destruir_deserializado_master(deserializado);
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }
            
            int prioridad = deserializado->payload.handshake_query_control.prioridad;
            if (prioridad < 0) {
                log_error(logger, "Prioridad inválida: %d - FD: %d", prioridad, fd_cliente);
                destruir_deserializado_master(deserializado);
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }
            
            log_debug(logger, "Handshake QC recibido: archivo=%s, prioridad=%d - FD: %d",
                    deserializado->payload.handshake_query_control.archivo_query,
                    prioridad,
                    fd_cliente);
                        
            t_query* datos = malloc(sizeof(*datos));
            if (!datos) {
                log_error(logger, "Error de memoria para datos Query");
                destruir_deserializado_master(deserializado);
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }
            
            datos->fd_query = fd_cliente;
            datos->path = strdup(deserializado->payload.handshake_query_control.archivo_query);
            datos->prioridad = prioridad;
            
            if (!datos->path) {
                log_error(logger, "Error duplicando path de query");
                free(datos);
                destruir_deserializado_master(deserializado);
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }

            t_paquete* respuesta = serializar_respuesta_handshake_query_control();
            if (!respuesta) {
                log_error(logger, "Error serializando respuesta handshake QC");
                free(datos->path);
                free(datos);
                destruir_deserializado_master(deserializado);
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }

            if (!enviar_paquete(respuesta, fd_cliente)) {
                log_error(logger, "Error enviando handshake OK a Query Control");
                free(datos->path);
                free(datos);
                destruir_deserializado_master(deserializado);
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }

            log_debug(logger,"se mandó paquete a queryControl");

            pthread_t query_control;
            if (pthread_create(&query_control, NULL, atender_query_control, datos) != 0) {
                log_error(logger, "Error al crear hilo para Query Control");
                free(datos->path);
                free(datos);
                destruir_deserializado_master(deserializado);
                destruir_payload(payload);
                close(fd_cliente);
                free(fd_cliente_ptr);
                return NULL;
            }
            
            pthread_detach(query_control);

            destruir_deserializado_master(deserializado);
            destruir_payload(payload);
            free(fd_cliente_ptr);  
            
            log_debug(logger, "Handshake completado para Query Control");
            break;
        }
        default: {
            log_error(logger, "Valor no identificado en el Handshake: %d - FD: %d", header, fd_cliente);
            
            t_paquete* respuesta = serializar_respuesta_handshake_desconocido();
            if (respuesta) {
                enviar_paquete(respuesta, fd_cliente);
            }
            
            close(fd_cliente);
            free(fd_cliente_ptr);
            break;
        }
    }

    return NULL;
}

void finalizar_programa() {
    log_debug(logger, "Iniciando cierre graceful del Master...");
    programa_activo = false;
    
    if (fd_servidor_master != -1) {
        log_debug(logger, "Cerrando servidor Master...");
        finalizar_servidor(fd_servidor_master);
        fd_servidor_master = -1;
    }
    
    log_debug(logger, "Servidor Master finalizado correctamente");
}