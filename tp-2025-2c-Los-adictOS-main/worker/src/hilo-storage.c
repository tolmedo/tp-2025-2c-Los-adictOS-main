#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <semaphore.h>

#include "commons/log.h"

#include <utils/conexiones.h>
#include <utils/serializacion.h>
#include <utils/deserializacion.h>

#include "hilo-storage.h"
#include "config.h"
#include "serializacion.h"
#include "query_interpreter.h"

int fd_storage;
extern int tamanio_bloque;
extern int error_operacion;
extern t_log* logger;

static int conectar_a_storage();
static void handshake_con_storage();

sem_t sem_respuesta_storage;
sem_t sem_query_finalizada;
extern bool query_en_ejecucion;

void inicializar_storage() {
    fd_storage = conectar_a_storage();
    sem_init(&sem_respuesta_storage, 0, 0);
    sem_init(&sem_query_finalizada, 0, 0);
    handshake_con_storage();
}

static int conectar_a_storage() {
    char* ip_storage = obtener_ip_storage();
    char* puerto_storage = obtener_puerto_storage();

    int fd = conectar_a_servidor(ip_storage, puerto_storage);

    if (fd < 0) {
        log_error(logger, "Error al intentar conectar con Storage en %s:%s", ip_storage, puerto_storage);
        exit(EXIT_FAILURE);
        return -1;
    }

    log_debug(logger, "Conexión establecida con Storage en %s:%s", ip_storage, puerto_storage);

    return fd;
}

static void handshake_con_storage() {
    int worker_id = obtener_id_worker(); 
    t_paquete* handshake = serializar_handshake(worker_id);
    enviar_paquete(handshake, fd_storage);

    t_header header = obtener_header(fd_storage);

    if (header != HANDSHAKE_OK) {
        log_error(logger, "Handshake fallido: Storage rechazó la conexión");
        close(fd_storage);
        exit(EXIT_FAILURE);
    }

    log_debug(logger, "Handshake con Storage establecido");

    // Recibir tamaño de bloque
    int offset = 0;
    void* payload = obtener_payload(fd_storage);
    tamanio_bloque = obtener_un_entero(payload, &offset);
    
    log_debug(logger, "Tamaño de bloque recibido de Storage: %d bytes", tamanio_bloque);
    
    destruir_payload(payload);
}

void esperar_respuesta_storage(int fd, int query_id, int fd_master) {
    t_header header = obtener_header(fd);
    if (header < 0) {
        log_error(logger, "Error recibiendo respuesta de Storage (conexión cerrada)");
        return;
    }
    
    if (header == 0) {
        //log_error(logger, "Header 0 recibido de Storage - posible desincronización");
        header = obtener_header(fd);
    }
    
    log_debug(logger, "[DEBUG] Header recibido de Storage: %d", header);
    
    int offset = 0;
    void* payload = NULL;
    
    switch(header) {
        case RESPUESTA_CREATE_FILE:
            log_debug(logger, "Storage confirmó CREATE_FILE");

            payload = obtener_payload(fd);
            destruir_payload(payload);
            sem_post(&sem_respuesta_storage);
            break;
            
        case RESPUESTA_TRUNCATE_FILE:
            log_debug(logger, "Storage confirmó TRUNCATE_FILE");

            payload = obtener_payload(fd);
            destruir_payload(payload);
            sem_post(&sem_respuesta_storage);
            break;
            
        case RESPUESTA_WRITE_BLOCK:
            log_debug(logger, "Storage confirmó WRITE_BLOCK");

            payload = obtener_payload(fd);
            destruir_payload(payload);
            sem_post(&sem_respuesta_storage);
            break;
            
        case RESPUESTA_READ_BLOCK:
            log_debug(logger, "Storage confirmó READ_BLOCK");
            sem_post(&sem_respuesta_storage);
            
            break;
            
        case RESPUESTA_TAG_FILE:
            log_debug(logger, "Storage confirmó TAG_FILE");
            payload = obtener_payload(fd);
            destruir_payload(payload);
            sem_post(&sem_respuesta_storage);
            break;
            
        case RESPUESTA_COMMIT_TAG:
            log_debug(logger, "Storage confirmó COMMIT_TAG");

            payload = obtener_payload(fd);
            destruir_payload(payload);
            sem_post(&sem_respuesta_storage);
            break;
            
        case RESPUESTA_REMOVE_TAG:
            log_debug(logger, "Storage confirmó REMOVE_TAG");

            payload = obtener_payload(fd);
            destruir_payload(payload);
            sem_post(&sem_respuesta_storage);
            break;
            
        case ERROR_OPERACION:
            payload = obtener_payload(fd);

            if (!payload) {
                log_error(logger, "Error: payload NULL en ERROR_OPERACION");
                return;
            }
            
            char* motivo = obtener_un_string(payload, &offset);
            log_error(logger, "Error en operación de Storage: %s", motivo);
            query_en_ejecucion = false;
            
            sem_post(&sem_respuesta_storage);
            //sem_wait(&sem_query_finalizada);
            t_paquete* paquete = serializar_fin_query(query_id, motivo);
            enviar_paquete(paquete, fd_master);
            
            free(motivo);
            destruir_payload(payload);
            break;
            
        default:
            log_error(logger, "Respuesta inesperada de Storage: %d", header);

            payload = obtener_payload(fd);
            destruir_payload(payload);
            break;
    }
}