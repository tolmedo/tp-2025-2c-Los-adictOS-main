#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <pthread.h>

#include <commons/log.h>
#include <commons/string.h>

#include <utils/estructuras.h>
#include <utils/serializacion.h>
#include <utils/deserializacion.h>

#include "hilo-worker.h"
#include "serializacion.h"
#include "deserializacion.h"
#include "config.h"
#include "operaciones.h"

extern t_log* logger;

// Contador global de workers conectados (thread-safe)
static int workers_conectados = 0;
static pthread_mutex_t mutex_workers = PTHREAD_MUTEX_INITIALIZER;

void* atender_worker(void* arg) {

    t_worker_conn* datos = (t_worker_conn*)arg;
    int fd_worker = datos->fd_worker;
    int worker_id = datos->worker_id;
    
    log_debug(logger, "[DEBUG] Iniciando atención al Worker %d en FD %d", worker_id, fd_worker);
    
    // Incrementar contador de workers conectados
    pthread_mutex_lock(&mutex_workers);
    workers_conectados++;
    int cantidad_actual = workers_conectados;
    pthread_mutex_unlock(&mutex_workers);

    log_info(logger, "## Se conecta el Worker %d - Cantidad de Workers: %d", worker_id, cantidad_actual);

    while (1) {
        log_debug(logger, "[DEBUG] Worker %d esperando operación...", worker_id);
        
        t_header header = obtener_header(fd_worker);
        
        log_debug(logger, "[DEBUG] Worker %d recibió header: %d", worker_id, header);
        
        if (header < 0) {
            log_debug(logger, "Worker %d desconectado", worker_id);
            break;
        }
        
        void* payload = obtener_payload(fd_worker);
        if (!payload) {
            log_debug(logger, "Payload inválido de Worker %d, cerrando conexión", worker_id);
            break;
        }
        
        log_debug(logger, "[DEBUG] Worker %d - Payload recibido, deserializando...", worker_id);
        
        t_deserializado* operacion = deserializar(header, payload);
        if (!operacion) {
            log_error(logger, "Error deserializando operación de Worker %d", worker_id);
            destruir_payload(payload);
            break;
        }

        log_debug(logger, "[DEBUG] Worker %d - Operación deserializada exitosamente", worker_id);

        switch (header) {
            case CREATE_FILE:
                {
                    int query_id = operacion->payload.crear_file.query_id;
                    char* nombre_file = operacion->payload.crear_file.nombre_file;
                    char* nombre_tag = operacion->payload.crear_file.tag;

                    create_file(fd_worker, nombre_file, nombre_tag, query_id);
                }
                break;

            case TRUNCATE_FILE:
                {
                    int query_id = operacion->payload.truncar_archivo.query_id;
                    char* nombre_file = operacion->payload.truncar_archivo.nombre_file;
                    char* nombre_tag = operacion->payload.truncar_archivo.tag;
                    int nuevo_tamanio = operacion->payload.truncar_archivo.tamanio;

                    truncate_file(fd_worker, nombre_file, nombre_tag, nuevo_tamanio, query_id);
                }
                break;

            case WRITE_BLOCK:
                {
                    int query_id = operacion->payload.escribir_bloque.query_id;
                    char* nombre_file = operacion->payload.escribir_bloque.nombre_file;
                    char* nombre_tag = operacion->payload.escribir_bloque.tag;
                    int bloque_logico = operacion->payload.escribir_bloque.bloque_logico;
                    int tamanio = operacion->payload.escribir_bloque.tamanio;
                    void* contenido = operacion->payload.escribir_bloque.contenido;

                    write_block(fd_worker, nombre_file, nombre_tag, bloque_logico, contenido, query_id, tamanio);
                }
                break;
                
            case READ_BLOCK:
                {
                    int query_id = operacion->payload.leer_bloque.query_id;
                    char* nombre_file = operacion->payload.leer_bloque.nombre_file;
                    char* nombre_tag = operacion->payload.leer_bloque.tag;
                    int bloque_logico = operacion->payload.leer_bloque.bloque_logico;
                    
                    read_block(fd_worker, nombre_file, nombre_tag, bloque_logico, query_id);
                }
                break;
                
            case REMOVE_TAG:
                {
                    int query_id = operacion->payload.eliminar_tag.query_id;
                    char* nombre_file = operacion->payload.eliminar_tag.nombre_file;
                    char* nombre_tag = operacion->payload.eliminar_tag.tag;
                    
                    remove_tag(fd_worker, nombre_file, nombre_tag, query_id);
                }
                break;

            case COMMIT_TAG:
                {
                    int query_id = operacion->payload.confirmar_tag.query_id;
                    char* nombre_file = operacion->payload.confirmar_tag.nombre_file;
                    char* nombre_tag = operacion->payload.confirmar_tag.tag;
                    
                    commit_tag(fd_worker, nombre_file, nombre_tag, query_id);
                }
                break;
                
            case TAG_FILE:
                {
                    int query_id = operacion->payload.crear_copia.query_id;
                    char* nombre_file_origen = operacion->payload.crear_copia.nombre_file_origen;
                    char* tag_origen = operacion->payload.crear_copia.tag_origen;
                    char* nombre_file_destino = operacion->payload.crear_copia.nombre_file_destino;
                    char* tag_destino = operacion->payload.crear_copia.tag_destino;

                    tag_file(fd_worker, nombre_file_origen, tag_origen, nombre_file_destino, tag_destino, query_id);
                }
                break;
            
            default:
                log_debug(logger, "Worker %d envió header desconocido: %d", worker_id, header);
                break;
        }
        
        log_debug(logger, "[DEBUG] Worker %d - Operación procesada exitosamente", worker_id);
           
        destruir_payload(payload);
        destruir_deserializado(operacion);
    }

    pthread_mutex_lock(&mutex_workers);
    workers_conectados--;
    cantidad_actual = workers_conectados;
    pthread_mutex_unlock(&mutex_workers);
    
    log_info(logger, "## Se desconecta el Worker %d - Cantidad de Workers: %d", worker_id, cantidad_actual);

    close(fd_worker);
    free(datos);

    return NULL;
}