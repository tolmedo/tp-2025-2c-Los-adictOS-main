#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/select.h>
#include <errno.h>
#include <semaphore.h>

#include <commons/log.h>
#include <commons/string.h>
#include <commons/collections/list.h>

#include <utils/estructuras.h>
#include <utils/serializacion.h>
#include <utils/deserializacion.h>

#include "types.h"  
#include "hilo-worker.h"
#include "serializacion_master.h"
#include "config.h"
#include "globales.h"
#include "hilo-query_control.h" 
#include "deserializacion.h"

extern t_log* logger;

static int workers_conectados = 0;
static pthread_mutex_t mutex_workers = PTHREAD_MUTEX_INITIALIZER;

sem_t sem_desalojo_completado;
sem_t sem_ejecutar_query;

// ========== FUNCIONES DE GESTIÓN DE WORKERS ==========

t_worker* obtener_worker_disponible(void) {
    pthread_mutex_lock(&mutex_workers_info);
    
    for (int i = 0; i < list_size(lista_workers_info); i++) {
        t_worker* worker = list_get(lista_workers_info, i);

        pthread_mutex_lock(&worker->mutex);
        if (worker->disponible) {
            worker->disponible = false;  // Marcar como ocupado
            pthread_mutex_unlock(&worker->mutex);
            pthread_mutex_unlock(&mutex_workers_info);
            log_debug(logger, "Worker %d asignado para ejecución", worker->worker_id);
            return worker;
        }
        pthread_mutex_unlock(&worker->mutex);
    }
    
    pthread_mutex_unlock(&mutex_workers_info);
    return NULL;  // No hay workers disponibles
}

void marcar_worker_disponible(int worker_id) {
    pthread_mutex_lock(&mutex_workers_info);
    
    for (int i = 0; i < list_size(lista_workers_info); i++) {
        t_worker* worker = list_get(lista_workers_info, i);
        if (worker->worker_id == worker_id) {
            pthread_mutex_lock(&worker->mutex);
            worker->disponible = true;
            pthread_mutex_unlock(&worker->mutex);
            pthread_mutex_unlock(&mutex_workers_info);

            log_debug(logger, "Worker %d marcado como disponible", worker_id);
            return;
        }
    }
    
    // Worker no encontrado (podría pasar durante shutdown)
    pthread_mutex_unlock(&mutex_workers_info);
    log_debug(logger, "Worker %d no encontrado al intentar marcar como disponible", worker_id);
}

void marcar_worker_ocupado(int worker_id){
    pthread_mutex_lock(&mutex_workers_info);
    
    for (int i = 0; i < list_size(lista_workers_info); i++) {
        t_worker* worker = list_get(lista_workers_info, i);
        if (worker->worker_id == worker_id) {
            pthread_mutex_lock(&worker->mutex);
            worker->disponible = false;
            pthread_mutex_unlock(&worker->mutex);
            pthread_mutex_unlock(&mutex_workers_info);

            log_debug(logger, "Worker %d marcado como disponible", worker_id);
            return;
        }
    }
    
    pthread_mutex_unlock(&mutex_workers_info);
    log_debug(logger, "Worker %d no encontrado al intentar marcar como ocupado", worker_id);
}

int obtener_socket_de_worker(int worker_id) {
    pthread_mutex_lock(&mutex_workers_info);
    
    for (int i = 0; i < list_size(lista_workers_info); i++) {
        t_worker* info = list_get(lista_workers_info, i);
        if (info->worker_id == worker_id) {
            int socket = info->fd_worker;
            pthread_mutex_unlock(&mutex_workers_info);
            return socket;
        }
    }
    
    pthread_mutex_unlock(&mutex_workers_info);
    return -1;
}

void agregar_worker_info(int worker_id, int socket_worker) {
    pthread_mutex_lock(&mutex_workers_info);
    
    // Verificar si el worker YA EXISTE
    for (int i = 0; i < list_size(lista_workers_info); i++) {
        t_worker* info = list_get(lista_workers_info, i);
        if (info->worker_id == worker_id) {
            // ERROR: Worker duplicado - CERRAR la nueva conexión
            log_error(logger, "ERROR: Worker %d ya está conectado. Rechazando conexión duplicada", worker_id);
            pthread_mutex_unlock(&mutex_workers_info);
            close(socket_worker);  // Cerrar el socket duplicado
            return;
        }
    }
    
    // Si no existe, AGREGAR NUEVO WORKER
    t_worker* nuevo_worker = malloc(sizeof(t_worker));
    nuevo_worker->worker_id = worker_id;
    nuevo_worker->fd_worker = socket_worker;
    nuevo_worker->disponible = true; 
    pthread_mutex_init(&nuevo_worker->mutex, NULL);
    
    list_add(lista_workers_info, nuevo_worker);
    pthread_mutex_unlock(&mutex_workers_info);
    
    log_debug(logger, "Worker %d conectado y agregado correctamente", worker_id);
}

void remover_worker_info(int worker_id) {
    pthread_mutex_lock(&mutex_workers_info);
    
    for (int i = 0; i < list_size(lista_workers_info); i++) {
        t_worker* info = list_get(lista_workers_info, i);
        if (info->worker_id == worker_id) {
            pthread_mutex_destroy(&info->mutex);
            free(info);
            list_remove(lista_workers_info, i);
            break;
        }
    }
    
    pthread_mutex_unlock(&mutex_workers_info);
}

// ========== FUNCIONES DE COMUNICACIÓN ==========

void enviar_desalojo_a_worker(int worker_id, int query_id) {
    int socket_worker = obtener_socket_de_worker(worker_id);
    
    if (socket_worker == -1) {
        log_debug(logger, "No se pudo enviar desalojo: Worker %d no encontrado", worker_id);
        return;
    }
    
    t_paquete* paquete = crear_paquete(DESALOJAR_QUERY);//
    agregar_a_paquete(paquete, &query_id, sizeof(int));
    
    if (!enviar_paquete(paquete, socket_worker)) {
        log_error(logger, "Error enviando desalojo al Worker %d para Query %d", worker_id, query_id);
    } else {
        log_debug(logger, "Desalojo enviado al Worker %d para Query %d", worker_id, query_id);

    }
}

// ========== FUNCIONES DE PROCESAMIENTO ==========

static t_query* encontrar_query_por_worker(int worker_id) {
    pthread_mutex_lock(&mutex_queries_exec);
    
    for (int i = 0; i < list_size(queries_exec); i++) {
        t_query* query = list_get(queries_exec, i);

        pthread_mutex_lock(&query->mutex);
        int worker_asignado = query->worker_asignado;
        pthread_mutex_unlock(&query->mutex);

        if (worker_asignado == worker_id) {
            pthread_mutex_unlock(&mutex_queries_exec);
            return query;
        }
    }
    
    pthread_mutex_unlock(&mutex_queries_exec);
    return NULL;
}

static void procesar_lectura_realizada(int worker_id, void* payload) {
    t_query* query = encontrar_query_por_worker(worker_id);
    if (!query) {
        log_error(logger, "No se encontró query para Worker %d en LECTURA_REALIZADA", worker_id);
        return;
    }
    
    int offset = 0;
    char* nombre_file = obtener_un_string(payload, &offset);
    char* tag = obtener_un_string(payload, &offset);
    char* contenido = obtener_un_string(payload, &offset);

    log_debug(logger, "File %s - Tag %s - Contenido Leído: %s", nombre_file, tag, contenido);
    
    if (!nombre_file || !tag || !contenido) {
        log_error(logger, "Error deserializando lectura del Worker %d", worker_id);
        free(nombre_file);
        free(tag);
        free(contenido);
        return;
    }
    
    pthread_mutex_lock(&query->mutex);
    int fd_query = query->fd_query;
    int query_id = query->id;
    pthread_mutex_unlock(&query->mutex);
/*
    if (estado == QUERY_EXIT) {
        log_debug(logger, "Query %d en EXIT, descartando lectura de %s:%s", 
                    query_id, nombre_file, tag);
        free(nombre_file);
        free(tag);
        free(contenido);
        return;
    }

    if (fd_query <= 0) {
        log_debug(logger, "fd_query inválido (%d), Query Control ya desconectado", fd_query);
        free(nombre_file);
        free(tag);
        free(contenido);
        return;
    }
*/

    log_info(logger, "## Se envía mensaje de lectura de la Query %d en el Worker %d al Query Control", query_id, worker_id);
    
    t_paquete* paquete = serializar_mensaje_lectura(nombre_file, tag, contenido);
    bool enviado = enviar_paquete(paquete, fd_query);
    
    if (!enviado) {
        log_error(logger, "Error al enviar mensaje de lectura al Query Control para Query %d", query_id);
    }
    
    free(nombre_file);
    free(tag);
    free(contenido);
}

static void procesar_query_finalizada(int worker_id, void* payload) {
    t_query* query = encontrar_query_por_worker(worker_id);
    if (!query) {
        log_error(logger, "No se encontró query para Worker %d en QUERY_FINALIZADA", worker_id);
        return;
    }

    int offset = 0;
    int query_id_recibido = obtener_un_entero(payload, &offset);

    pthread_mutex_lock(&query->mutex);
    int query_id = query->id;
    int fd_query = query->fd_query;
    int prioridad = query->prioridad;
    pthread_mutex_unlock(&query->mutex);

    if (query_id_recibido != query_id) {
        log_debug(logger, "Query ID recibido (%d) no coincide con esperado (%d)", query_id_recibido, query_id);
    }
    
    // Enviar finalización al Query Control
    char* motivo = obtener_un_string(payload, &offset);

    t_paquete* paquete_finalizacion = crear_paquete(QC_FINALIZACION);
    agregar_a_paquete(paquete_finalizacion, motivo, strlen(motivo) + 1);
    if (enviar_paquete(paquete_finalizacion, fd_query)) log_debug(logger, "se manda paquete");

    pthread_mutex_lock(&mutex_workers);
    pthread_mutex_lock(&mutex_queries_exec);
    
    pthread_mutex_lock(&query->mutex);
    if (query->worker_asignado != -1) {
        marcar_worker_disponible(worker_id);
        list_remove_element(queries_exec, query);
    }
    query->estado = QUERY_EXIT;
    pthread_mutex_unlock(&query->mutex);

    pthread_mutex_unlock(&mutex_queries_exec);
    pthread_mutex_unlock(&mutex_workers);

    if (strcmp(motivo, "COMPLETADA") == 0){
        log_info(logger, "## Se terminó la Query %d en el Worker %d", query_id, worker_id);
    
        pthread_mutex_lock(&query->mutex);
        if (query->fd_query > 0) {
            close(query->fd_query);
            query->fd_query = -1;
        }
        pthread_mutex_unlock(&query->mutex);
    
        usleep(50000);
    
        pthread_mutex_destroy(&query->mutex);
        free(query->path);
        free(query);
    } else {
        log_info(logger, "## Se desaloja la Query %d (%d) del Worker %d - Motivo: DESCONEXION", 
             query_id, prioridad, worker_id);
    
        pthread_mutex_lock(&query->mutex);
        if (query->fd_query > 0) {
            close(query->fd_query);
            query->fd_query = -1;
        }
        pthread_mutex_unlock(&query->mutex);
    
        usleep(50000);
    
        pthread_mutex_destroy(&query->mutex);
        free(query->path);
        free(query);
    }

    log_info(logger, "## Se desconecta un Query Control. Se finaliza la Query %d con prioridad %d. Nivel multiprocesamiento %d", query_id, prioridad, workers_conectados);
    free(motivo);
    planificar_queries();
}

static void procesar_desalojo_completado(int worker_id, int query_id, int program_counter) {
    t_query* query = encontrar_query_por_id(query_id);

    if (query) {
        pthread_mutex_lock(&query->mutex);
        query->program_counter = program_counter;
        query->desalojada = true;
            
        pthread_mutex_lock(&mutex_queries_exec);
        list_remove_element(queries_exec, query);
        pthread_mutex_unlock(&mutex_queries_exec);
            
        pthread_mutex_lock(&mutex_queries_ready);
        query->estado = QUERY_READY;
        query->worker_asignado = -1;
        clock_gettime(CLOCK_MONOTONIC, &query->tiempo_ingreso_ready);
        log_info(logger, "## Se desaloja la Query %d (%d) del Worker %d - Motivo: PRIORIDAD", query->id, query->prioridad, worker_id);
        pthread_mutex_unlock(&query->mutex);
        list_add(queries_ready, query);
        pthread_mutex_unlock(&mutex_queries_ready);
        
    }
    
    marcar_worker_disponible(worker_id);
    sem_post(&sem_ejecutar_query);
}

void manejar_desconexion_worker(int worker_id) {
    pthread_mutex_lock(&mutex_workers);
    
    remover_worker_info(worker_id);
    
    t_query* query_afectada = NULL;
    pthread_mutex_lock(&mutex_queries_exec);
    for (int i = 0; i < list_size(queries_exec); i++) {
        t_query* query = list_get(queries_exec, i);

        pthread_mutex_lock(&query->mutex);
        int worker_asignado = query->worker_asignado;
        pthread_mutex_unlock(&query->mutex);

        if (worker_asignado == worker_id) {
            query_afectada = query;
            list_remove(queries_exec, i);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_queries_exec);
    
    if (query_afectada) {
        t_paquete* paquete_error = crear_paquete(QC_FINALIZACION);
        char* motivo = "ERROR_DESCONEXION_WORKER";
        agregar_a_paquete(paquete_error, motivo, strlen(motivo) + 1);

        pthread_mutex_lock(&query_afectada->mutex);
        int fd_query = query_afectada->fd_query;
        int query_id = query_afectada->id;
        pthread_mutex_unlock(&query_afectada->mutex);

        if(fd_query > 0) {
            enviar_paquete(paquete_error, fd_query);
        } else {
            eliminar_paquete(paquete_error);
        }
        
        log_info(logger, "## Se desconecta el Worker %d - Se finaliza la Query %d - Cantidad total de Workers: %d",
                 worker_id, query_id, list_size(lista_workers_info));
        
        pthread_mutex_lock(&query_afectada->mutex);
        if (query_afectada->fd_query > 0) {
            close(query_afectada->fd_query);
            query_afectada->fd_query = -1;
        }
        pthread_mutex_unlock(&query_afectada->mutex);

        usleep(50000);
    
        pthread_mutex_destroy(&query_afectada->mutex);
        free(query_afectada->path);
        free(query_afectada);
    } else {
        log_info(logger, "## Se desconecta el Worker %d - Cantidad total de Workers: %d",
                 worker_id, list_size(lista_workers_info));
    }
    
    workers_conectados--;
    pthread_mutex_unlock(&mutex_workers);
    
    planificar_queries();
}

void* atender_worker(void* arg) {
    t_worker* datos = (t_worker*)arg;
    int worker_id = datos->worker_id;

    if (worker_id < 0) {
        log_error(logger, "ID de Worker inválido: %d", worker_id);
        free(datos);
        return NULL;
    }

    agregar_worker_info(worker_id, datos->fd_worker);
    
    pthread_mutex_lock(&mutex_workers);
    workers_conectados++;
    int cantidad_actual = workers_conectados;
    pthread_mutex_unlock(&mutex_workers);

    log_info(logger, "## Se conecta el Worker %d - Cantidad total de Workers: %d", worker_id, cantidad_actual);

    sem_init(&sem_desalojo_completado, 0, 0);
    sem_init(&sem_ejecutar_query, 0, 0);

    planificar_queries();

    while (1) {
        fd_set readfds;
        struct timeval timeout;
        
        FD_ZERO(&readfds);
        FD_SET(datos->fd_worker, &readfds);
        
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        
        int result = select(datos->fd_worker + 1, &readfds, NULL, NULL, &timeout);
        
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            log_error(logger, "Error en select para Worker %d: %s", worker_id, strerror(errno));
            break;
        } else if (result == 0) {
            continue;
        }

        t_header header = obtener_header(datos->fd_worker);
        if (header == -1) {
            log_debug(logger, "Worker %d desconectado del Master", worker_id);
            break;
        }

        void* payload = obtener_payload(datos->fd_worker);
        
        switch (header) {
            case LECTURA_REALIZADA:
                procesar_lectura_realizada(worker_id, payload);
                break;
            case QUERY_FINALIZADA:
                procesar_query_finalizada(worker_id, payload);
                break;
            case DESALOJO_QUERY:
                t_deserializado_master* deserializado = deserializar_mensaje_master(header, payload);
                int query_id = deserializado->payload.desalojo_query.query_id;
                int program_counter = deserializado->payload.desalojo_query.program_counter;
                procesar_desalojo_completado(worker_id, query_id, program_counter);
                destruir_deserializado_master(deserializado);
                break;
            default:
                log_debug(logger, "Header desconocido %d recibido del Worker %d", header, worker_id);
                break;
        }
        
        destruir_payload(payload);
    }

    manejar_desconexion_worker(worker_id);
    
    free(datos);
    return NULL;
}