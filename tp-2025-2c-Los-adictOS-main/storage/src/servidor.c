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
#include <utils/estructuras.h>

#include "superblock.h"
#include "config.h"
#include "hilo-worker.h"
#include "serializacion.h"
#include "storage.h"

extern t_log* logger;
extern t_config_storage* config_storage;

static int fd_servidor_storage = -1;
static pthread_t thread;
static int worker_id_counter = 1;

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

    log_debug(logger, "Servidor Storage Inicializado - Puerto: %s", puerto_escucha);
    storage_activo = true;

    return fd_servidor;
}

static void* _esperar_workers(void* arg){
    int fd_storage = *((int*) arg);
    free(arg);
    
    log_debug(logger, "Esperando conexiones de Workers...");
    
    while (storage_activo) {
        int fd_worker = aceptar_cliente(fd_storage);
        
        if (fd_worker == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                usleep(100000);
                continue;
            }
            
            if (storage_activo) {
                log_error(logger, "Error en accept: %s", strerror(errno));
            }
            break;
        }

        log_debug(logger, "Nueva conexión aceptada - FD: %d", fd_worker);

        int* fd_worker_ptr = malloc(sizeof(int));
        *fd_worker_ptr = fd_worker;

        pthread_t thread_worker;
        if (pthread_create(&thread_worker, NULL, _handshake, fd_worker_ptr) != 0) {
            log_error(logger, "Error creando hilo de handshake");
            close(fd_worker);
            free(fd_worker_ptr);
            continue;
        }
        pthread_detach(thread_worker);
    }

    log_debug(logger, "Cerrando servidor Storage...");
    finalizar_servidor(fd_storage);
    finalizar_programa();
    
    return NULL;
}

static void* _handshake(void* arg){
    int* fd_worker_ptr = (int*)arg;
    int fd_worker = *fd_worker_ptr;

    log_debug(logger, "Iniciando handshake con cliente FD: %d", fd_worker);

    t_header header = obtener_header(fd_worker);
    
    log_debug(logger, "Header recibido en handshake: %d (esperaba HANDSHAKE_WORKER=%d)", header, HANDSHAKE_WORKER);
    
    if (header != HANDSHAKE_WORKER) {
        log_error(logger, "Header inválido en handshake: %d (esperaba HANDSHAKE_WORKER=%d)", header, HANDSHAKE_WORKER);
        
        // Intentar consumir payload si existe
        void* payload = obtener_payload(fd_worker);
        if (payload) destruir_payload(payload);
        
        t_paquete* respuesta = serializar_respuesta_handshake_desconocido();
        enviar_paquete(respuesta, fd_worker);
        
        close(fd_worker);
        free(fd_worker_ptr);
        return NULL;
    }
    
    // Obtener payload SOLO si el header es correcto
    void* payload = obtener_payload(fd_worker);
    
    int worker_id = -1;
    
    if (payload) {
        int offset = 0;
        worker_id = obtener_un_entero(payload, &offset);
        destruir_payload(payload);
        
        log_debug(logger, "Worker ID extraído del payload: %d", worker_id);
    }
    
    if (worker_id <= 0) {
        worker_id = worker_id_counter++;
        log_debug(logger, "Worker sin ID válido, asignando ID automático: %d", worker_id);
    }
    
    log_debug(logger, "Handshake recibido de Worker %d", worker_id);
    
    // Enviar respuesta con block_size
    t_paquete* respuesta = serializar_respuesta_handshake_worker(obtener_block_size());
    if (!respuesta) {
        log_error(logger, "Error creando respuesta handshake para Worker %d", worker_id);
        close(fd_worker);
        free(fd_worker_ptr);
        return NULL;
    }
    
    if (!enviar_paquete(respuesta, fd_worker)) {
        log_error(logger, "Error enviando handshake a Worker %d", worker_id);
        close(fd_worker);
        free(fd_worker_ptr);
        return NULL;
    }
    
    log_debug(logger, "Handshake enviado a Worker %d con block_size: %d", 
             worker_id, obtener_block_size());
    
    // Crear estructura de datos para el worker
    t_worker_conn* datos = malloc(sizeof(*datos));
    if (!datos) {
        log_error(logger, "Error de memoria al crear estructura para Worker %d", worker_id);
        close(fd_worker);
        free(fd_worker_ptr);
        return NULL;
    }
    
    datos->fd_worker = fd_worker;
    datos->worker_id = worker_id;

    // Crear hilo para atender al worker
    pthread_t worker;
    if (pthread_create(&worker, NULL, atender_worker, datos) != 0) {
        log_error(logger, "Error creando hilo para Worker %d", worker_id);
        free(datos);
        close(fd_worker);
        free(fd_worker_ptr);
        return NULL;
    }
    
    log_debug(logger, "Hilo creado exitosamente para atender Worker %d", worker_id);
    
    pthread_detach(worker);

    free(fd_worker_ptr);
    
    return NULL;
}

void finalizar_programa() {
    if (config_storage) {
        liberar_config_storage(config_storage);
    }

    if(fd_servidor_storage){
        log_debug(logger, "Cerrando servidor Storage...");
        finalizar_servidor(fd_servidor_storage);
    }

    if (logger) {
        log_debug(logger, "Storage finalizado correctamente");
        log_destroy(logger);
    }
}