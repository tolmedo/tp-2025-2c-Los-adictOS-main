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

#include "hilo-query_control.h"
#include "serializacion.h"
#include "config.h"

extern t_log* logger;

// Contador global de queries conectados (thread-safe)
static int queries_conectados = 0;
static pthread_mutex_t mutex_queries = PTHREAD_MUTEX_INITIALIZER;
static int query_id_counter = 0;  // Contador para asignar IDs únicos
static pthread_mutex_t mutex_query_id = PTHREAD_MUTEX_INITIALIZER;

void* atender_query_control(void* arg) {
    t_query* datos = (t_query*)arg;
    int fd_query = datos->fd_query;

    // Generar ID único para la query
    pthread_mutex_lock(&mutex_query_id);
    int query_id = query_id_counter++;
    datos->id = query_id;
    pthread_mutex_unlock(&mutex_query_id);

    // Incrementar contador de queries conectados
    pthread_mutex_lock(&mutex_queries);
    queries_conectados++;
    int cantidad_actual = queries_conectados;
    pthread_mutex_unlock(&mutex_queries);


    log_info(logger, "## Se conecta un Query Control para ejecutar la Query %s con prioridad %d - Id asignado: %d. Nivel multiprogramación %d", datos->path, datos->prioridad, datos->id, cantidad_actual);
    

    for(int i=0; i<3; i++){
        sleep(10);
        log_info(logger, "simulando query");
    }

    // Query Control se desconectó - decrementar contador
    pthread_mutex_lock(&mutex_queries);
    queries_conectados--;
    cantidad_actual = queries_conectados;
    pthread_mutex_unlock(&mutex_queries);

    // Log obligatorio: Desconexión de Query Control
    log_info(logger, "## Se desconecta un Query Control. Se finaliza la Query %d con prioridad %d. Nivel multiprogramación %d", datos->id, datos->prioridad, cantidad_actual);

    // Limpiar memoria
    close(fd_query);

    return NULL;
}