#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <stdint.h> 
#include <stdbool.h>
#include <semaphore.h>
#include <errno.h>

#include <commons/log.h>
#include <commons/string.h>
#include <commons/collections/list.h>

#include <utils/estructuras.h>
#include <utils/serializacion.h>
#include <utils/deserializacion.h>

#include "types.h"
#include "hilo-query_control.h"
#include "serializacion_master.h"
#include "config.h"
#include "globales.h"
#include "hilo-worker.h"

extern t_log* logger;

static int contador_queries_conectadas = 0;
static int contador_id_queries = 0;
static pthread_mutex_t mutex_contadores = PTHREAD_MUTEX_INITIALIZER;

extern sem_t sem_ejecutar_query;

pthread_t thread_aging;
extern volatile bool programa_activo;
extern bool aging_activo;

// ========== FUNCIONES DE BÚSQUEDA ==========

t_query* encontrar_query_por_id(int query_id) {

    pthread_mutex_lock(&mutex_queries_exec);

    for (int i = 0; i < list_size(queries_exec); i++) {
        t_query* query = list_get(queries_exec, i);
        if (query->id == query_id) {
            pthread_mutex_unlock(&mutex_queries_exec);
            return query;
        }
    }

    pthread_mutex_unlock(&mutex_queries_exec);
    
    return NULL;
}

// ========== FUNCIONES DE AGING ==========

bool aplicar_aging_a_query(t_query* query) {
    pthread_mutex_lock(&query->mutex);
    
    if (query->estado != QUERY_READY || query->prioridad <= 0) {
        pthread_mutex_unlock(&query->mutex);
        return false;
    }
    
    struct timespec ahora;
    clock_gettime(CLOCK_MONOTONIC, &ahora);
    
    long tiempo_en_ready_ms = 
        (ahora.tv_sec - query->tiempo_ingreso_ready.tv_sec) * 1000 +
        (ahora.tv_nsec - query->tiempo_ingreso_ready.tv_nsec) / 1000000;
    
    if (tiempo_en_ready_ms >= obtener_tiempo_aging()) {
        int prioridad_anterior = query->prioridad;
        query->prioridad--;
        query->tiempo_ingreso_ready = ahora;
        
        pthread_mutex_unlock(&query->mutex);
        
        log_info(logger, "##%d Cambio de prioridad: %d -> %d", query->id, prioridad_anterior, query->prioridad);
        return true;
    }
    
    pthread_mutex_unlock(&query->mutex);
    return false;
}

// ========== FUNCIONES DE DESALOJO ==========

void desalojar_query_en_worker(int worker_id, int nueva_query_id) {
    pthread_mutex_lock(&mutex_queries_exec);
    
    t_query* victima = NULL;
    
    // Buscar la query que está en ese worker
    for (int i = 0; i < list_size(queries_exec); i++) {
        t_query* query = list_get(queries_exec, i);
        
        pthread_mutex_lock(&query->mutex);
        int worker_asignado = query->worker_asignado;
        bool desalojada = query->desalojada;
        t_estado_query estado = query->estado;
        pthread_mutex_unlock(&query->mutex);
        
        if (worker_asignado == worker_id) {
            if(desalojada || estado != QUERY_EXEC){
                pthread_mutex_unlock(&mutex_queries_exec);
                log_debug(logger,"Se intento desalojar Query %d en Worker %d pero ya estaba desalojada o no está en EXEC",query->id,worker_id);
                return;
            }

            victima = query;
            break;
        }
    }
    
    pthread_mutex_unlock(&mutex_queries_exec);
    
    if (victima) {
        pthread_mutex_lock(&victima->mutex);
        enviar_desalojo_a_worker(worker_id, victima->id);
        pthread_mutex_unlock(&victima->mutex);
    }
}

// ========== FUNCIONES DE COMUNICACIÓN CON WORKERS ==========

void manejar_fallo_envio_worker(int worker_id, t_query* query) {
    int worker_asignado = worker_id;
    
    pthread_mutex_lock(&mutex_queries_exec);
    bool estaba_en_exec = false;
    
    for (int i = 0; i < list_size(queries_exec); i++) {
        t_query* q = list_get(queries_exec, i);
        if (q->id == query->id) {
            list_remove(queries_exec, i);
            estaba_en_exec = true;
            break;
        }
    }
    pthread_mutex_unlock(&mutex_queries_exec);
    
    if (estaba_en_exec) {
        pthread_mutex_lock(&mutex_queries_ready);
        pthread_mutex_lock(&query->mutex);
        query->estado = QUERY_READY;
        query->worker_asignado = -1;
        clock_gettime(CLOCK_MONOTONIC, &query->tiempo_ingreso_ready);
        pthread_mutex_unlock(&query->mutex);
        list_add(queries_ready, query);
        pthread_mutex_unlock(&mutex_queries_ready);
    }

    // Marcar worker como disponible
    marcar_worker_disponible(worker_asignado);

    if (estaba_en_exec) {
        planificar_queries();
    }
}

void enviar_comando_a_worker(int worker_id, t_query* query, int program_counter) {
    int socket_worker = obtener_socket_de_worker(worker_id);
    
    if (socket_worker == -1) {
        log_error(logger, "No se pudo encontrar socket del Worker %d para Query %d", worker_id, query->id);
        manejar_fallo_envio_worker(worker_id, query);
        return;
    }
    
    t_paquete* paquete = serializar_comando_ejecutar_query(query->id, query->path, program_counter);
    
    if (!paquete) {
        log_error(logger, "Error serializando comando para Query %d", query->id);
        manejar_fallo_envio_worker(worker_id, query);
        return;
    }
    
    if (enviar_paquete(paquete, socket_worker)) {
        log_info(logger, "## Se envía la Query %d (%d) al Worker %d (PC: %u)", 
                 query->id, query->prioridad, worker_id, program_counter);
    } else {
        log_error(logger, "Fallo al enviar Query %d al Worker %d", query->id, worker_id);
        manejar_fallo_envio_worker(worker_id, query);
    }
}

// ========== ALGORITMOS DE PLANIFICACIÓN ==========

void planificar_queries_fifo() {
    pthread_mutex_lock(&mutex_planificacion);

    pthread_mutex_lock(&mutex_queries_ready);
    
    if (list_is_empty(queries_ready)) {
        pthread_mutex_unlock(&mutex_queries_ready);
        pthread_mutex_unlock(&mutex_planificacion);
        log_debug(logger,"lista vacía de queries");
        return;
    }

    log_debug(logger,"Hay queries en ready");

    t_worker* worker = obtener_worker_disponible();
    if (!worker) {
        pthread_mutex_unlock(&mutex_planificacion);
        pthread_mutex_unlock(&mutex_queries_ready);
        return;
    }

    t_query* query = list_remove(queries_ready, 0);
    pthread_mutex_unlock(&mutex_queries_ready);

    pthread_mutex_lock(&mutex_queries_exec);
    pthread_mutex_lock(&query->mutex);
    query->worker_asignado = worker->worker_id;
    query->estado = QUERY_EXEC;
    query->desalojada = false;
    pthread_mutex_unlock(&query->mutex);
    list_add(queries_exec, query);
    pthread_mutex_unlock(&mutex_queries_exec);

    enviar_comando_a_worker(worker->worker_id, query, query->program_counter);
    pthread_mutex_unlock(&mutex_planificacion);
}

void planificar_queries_prioridades() {
    pthread_mutex_lock(&mutex_planificacion);

    pthread_mutex_lock(&mutex_workers_info);
    int grado_multiprogramacion = list_size(lista_workers_info);
    pthread_mutex_unlock(&mutex_workers_info);

    for(int i = 0; i < grado_multiprogramacion; i++){

    pthread_mutex_lock(&mutex_queries_ready);

    if (list_is_empty(queries_ready)) {
        pthread_mutex_unlock(&mutex_queries_ready);
        pthread_mutex_unlock(&mutex_planificacion);
        return;
    }
    
    t_worker* worker = obtener_worker_disponible();

    t_query* mejor_query = NULL;
    int mejor_prioridad = -1;
    int indice_mejor = -1;

    for (int i = 0; i < list_size(queries_ready); i++) {
        t_query* q = list_get(queries_ready, i);

        pthread_mutex_lock(&q->mutex);
        int prioridad_actual = q->prioridad;
        pthread_mutex_unlock(&q->mutex);

        if (mejor_prioridad == -1 || prioridad_actual < mejor_prioridad) {
            mejor_prioridad = q->prioridad;
            mejor_query = q;
            indice_mejor = i;
        }
    }

    if (worker && mejor_query) {
        t_query* query = list_remove(queries_ready, indice_mejor);
        pthread_mutex_unlock(&mutex_queries_ready);

        pthread_mutex_lock(&mutex_queries_exec);
        pthread_mutex_lock(&query->mutex);
        query->estado = QUERY_EXEC;
        query->worker_asignado = worker->worker_id;
        query->desalojada = false;
        pthread_mutex_unlock(&query->mutex);
        list_add(queries_exec, query);
        pthread_mutex_unlock(&mutex_queries_exec);

        enviar_comando_a_worker(worker->worker_id, query, query->program_counter);
        
        pthread_mutex_unlock(&mutex_planificacion);
        continue;
    }
    
    // CASO 2: Todo ocupado -> buscar mejor query para posible desalojo
    // Encontrar la MEJOR query en READY
    pthread_mutex_lock(&mutex_queries_exec);
    bool puede_desalojar = false;
    int worker_victima = -1;
    int query_victima_id = -1;
    int peor_prioridad_exec = -1;

    for (int i = 0; i < list_size(queries_exec); i++) {
        t_query* query_exec = list_get(queries_exec, i);

        pthread_mutex_lock(&query_exec->mutex);
        int prioridad_exec = query_exec->prioridad;
        int worker_id_exec = query_exec->worker_asignado;
        int query_id_exec = query_exec->id;
        pthread_mutex_unlock(&query_exec->mutex);

        if (mejor_query && mejor_prioridad < prioridad_exec) {
            if (peor_prioridad_exec == -1 || prioridad_exec > peor_prioridad_exec) {
                puede_desalojar = true;
                worker_victima = worker_id_exec;
                query_victima_id = query_id_exec;
                peor_prioridad_exec = prioridad_exec;
            }
        }
    }

    pthread_mutex_unlock(&mutex_queries_exec);

    if (puede_desalojar) {
        t_query* query = list_remove(queries_ready, indice_mejor);
        pthread_mutex_unlock(&mutex_queries_ready);

        pthread_mutex_lock(&query->mutex);
        query->estado = QUERY_EXEC;
        query->worker_asignado = worker_victima;
        query->desalojada = false;
        pthread_mutex_unlock(&query->mutex);

        desalojar_query_en_worker(worker_victima, query_victima_id);
        sem_wait(&sem_ejecutar_query);

        pthread_mutex_lock(&mutex_queries_exec);
        list_add(queries_exec, query);
        pthread_mutex_unlock(&mutex_queries_exec);

        marcar_worker_ocupado(worker_victima);
        enviar_comando_a_worker(query->worker_asignado, query, query->program_counter);
        continue;
    }else {
        pthread_mutex_unlock(&mutex_queries_ready);
        continue;
    }
}   
    pthread_mutex_unlock(&mutex_planificacion);
}

void planificar_queries() {
    if (usar_algoritmo_prioridades()) {
        planificar_queries_prioridades();
    } else {
        planificar_queries_fifo();
    }
}

// ========== MANEJO DE QUERY CONTROLS ==========

void manejar_desconexion_query_control(t_query* query) {
    if (!query) {
        log_error(logger, "manejar_desconexion_query_control() recibió query NULL");
        return;
    }

    // Guardar info ANTES de cualquier modificación
    pthread_mutex_lock(&query->mutex);
    int query_id = query->id;
    int prioridad = query->prioridad;
    int worker_asignado = query->worker_asignado;
    pthread_mutex_unlock(&query->mutex);

    //enviar_desalojo_a_worker(worker_asignado, query_id);
    log_info(logger, "## Se desaloja la Query %d (%d) del Worker %d - Motivo: DESCONEXION", query_id, prioridad, worker_asignado);

    pthread_mutex_destroy(&query->mutex);
    free(query->path);
    free(query);
}

void* hilo_aging(void* arg) {
    t_query* query = (t_query*)arg;
    log_debug(logger, "Hilo de aging iniciado para Query %d - Intervalo: %d ms", query->id, obtener_tiempo_aging());
    
    while (programa_activo && query->fd_query > 0) {
        usleep(obtener_tiempo_aging()); 
        if (aplicar_aging_a_query(query)) {
            planificar_queries();
        }
    }

    log_debug(logger, "Hilo de aging finalizado para Query %d", query->id);
    return NULL;
}


void* atender_query_control(void* arg) {
    t_query* query = (t_query*)arg;

    if (!query) {
        log_error(logger, "atender_query_control() recibió arg NULL");
        return NULL;
    }
    
    pthread_mutex_lock(&mutex_contadores);
    query->id = contador_id_queries++;
    contador_queries_conectadas++;
    pthread_mutex_unlock(&mutex_contadores);

    pthread_mutex_init(&query->mutex, NULL);
    query->desalojada = false;
    query->estado = QUERY_READY;
    query->worker_asignado = -1;
    query->program_counter = 0;
    clock_gettime(CLOCK_MONOTONIC, &query->tiempo_ingreso_ready);

    pthread_mutex_lock(&mutex_workers_info);
    int nivel_multiprocesamiento = list_size(lista_workers_info);
    pthread_mutex_unlock(&mutex_workers_info);

    log_info(logger, "## Se conecta Query Control para ejecutar %s con prioridad %d - Id: %d. Nivel multiprocesamiento: %d", query->path, query->prioridad, query->id, nivel_multiprocesamiento);

    pthread_mutex_lock(&mutex_queries_ready);
    list_add(queries_ready, query);
    pthread_mutex_unlock(&mutex_queries_ready);

    planificar_queries();

    if (aging_activo) {
        while (programa_activo && query->fd_query > 0) {
            usleep(10000); 
            if (aplicar_aging_a_query(query)) {
                planificar_queries();
            }
        }
    } else {
        while(query->fd_query > 0) {
            fd_set readfds;
            struct timeval timeout;
    
            FD_ZERO(&readfds);
            FD_SET(query->fd_query, &readfds);
    
            timeout.tv_sec = 1;  // Timeout de 1 segundo
            timeout.tv_usec = 0;
    
            int result = select(query->fd_query + 1, &readfds, NULL, NULL, &timeout);
    
            if (result < 0) {
                if (errno == EINTR) continue;
                log_debug(logger, "Error en select: %s", strerror(errno));
                break;
            } else if (result == 0) {
                // Timeout, revisar si sigue activo
                continue;
            }
    
            // Hay datos para leer
            t_header header = obtener_header(query->fd_query);
            if (header < 0) {
                //manejar_desconexion_query_control(query);
                break;
            }
    
        }
    }

    return NULL;
}
