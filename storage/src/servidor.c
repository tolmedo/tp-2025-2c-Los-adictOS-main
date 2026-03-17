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

#include "superblock.h"
#include "config.h"
#include "hilo-worker.h"
#include "serializacion.h"
#include "storage.h"


extern t_log* logger;
extern t_config_storage* config_storage;

static int fd_servidor_storage = -1;
static pthread_t thread;


volatile bool storage_activo = false;


static int _iniciar_servidor_storage();
static void* _esperar_workers(void* arg);
static void* _handshake(void* arg);

void inicializar_servidor(){
    fd_servidor_storage = _iniciar_servidor_storage();
    int* fd_servidor_storage_ptr = malloc(sizeof(int));
    *fd_servidor_storage_ptr = fd_servidor_storage;

    pthread_create(&thread, NULL, _esperar_workers, fd_servidor_storage_ptr);

}

static int _iniciar_servidor_storage(){
    char* puerto_escucha = obtener_puerto_escucha();
    int fd_servidor = crear_servidor(puerto_escucha);

    if(fd_servidor == -1) {
        log_error(logger, "No se ha podido inicializar el servidor Storage en el puerto %s.", puerto_escucha);
        exit(EXIT_FAILURE);
        return -1;
    }

    log_info(logger, "Servidor Storage Inicializado - Puerto: %s", puerto_escucha);

    free(puerto_escucha);

    storage_activo = true;

    return fd_servidor;
}

static void* _esperar_workers(void* arg){
    int fd_storage = *((int*) arg);
    free(arg);

    while (storage_activo) {
        int fd_worker = aceptar_cliente(fd_storage);

        if (fd_worker == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // No hay conexiones pendientes, esperar un poco
                usleep(100000); // 100ms
                continue;
            }
            
            if (storage_activo) {
                log_error(logger, "Error en accept: %s", strerror(errno));
            }
            break;
        }

        int* fd_worker_ptr = malloc(sizeof(int));
        *fd_worker_ptr = fd_worker;

        pthread_t thread_worker;
        pthread_create(&thread_worker, NULL, _handshake, fd_worker_ptr);
        pthread_detach(thread_worker);
    }

    log_info(logger, "Cerrando servidor Storage...");
    finalizar_servidor(fd_storage);
    finalizar_programa();
    
    return NULL;
}

static void* _handshake(void* arg){
    int worker_id = -1; 
    int* fd_worker_ptr = (int*)arg;

    t_header header = obtener_header(*fd_worker_ptr);
    void* payload = obtener_payload(*fd_worker_ptr);

    int offset = 0;
    worker_id = obtener_un_entero(payload, &offset);

    
    switch (header) {
        case HANDSHAKE_WORKER: {
            t_paquete* respuesta = serializar_respuesta_handshake_worker(obtener_block_size());
            enviar_paquete(respuesta, *fd_worker_ptr);
            
            t_worker_conn* datos = malloc(sizeof(*datos));
            datos->fd_worker  = *fd_worker_ptr;
            datos->worker_id  = worker_id;

            pthread_t worker;
            pthread_create(&worker, NULL, atender_worker, datos);
            pthread_detach(worker);

            free(fd_worker_ptr); // ya no se lo pasamos al hilo
            
            break;
        }
        default: {
            t_paquete* respuesta = serializar_respuesta_handshake_desconocido();
            enviar_paquete(respuesta, *fd_worker_ptr);
            
            log_error(logger, "Valor no identificado en el Handshake");
            
            close(*fd_worker_ptr);
            free(fd_worker_ptr);
 
            break;
        }
    }

    destruir_payload(payload);

    return NULL;
}

void finalizar_programa() {
    if (config_storage) {
        liberar_config_storage(config_storage);
    }

    if(fd_servidor_storage){
        log_info(logger, "Cerrando servidor Storage...");
        finalizar_servidor(fd_servidor_storage);
    }

    if (logger) {
        log_info(logger, "Storage finalizado correctamente");
        log_destroy(logger);
    }

}