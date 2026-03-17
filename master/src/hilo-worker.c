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
#include "config.h"

extern t_log* logger;

// Contador global de workers conectados (thread-safe)
static int workers_conectados = 0;
static pthread_mutex_t mutex_workers = PTHREAD_MUTEX_INITIALIZER;

void* atender_worker(void* arg) {
    t_datos_worker* datos = (t_datos_worker*)arg;
    int fd_worker = datos->fd_worker;
    int worker_id = datos->worker_id;
    
    // Incrementar contador de workers conectados
    pthread_mutex_lock(&mutex_workers);
    workers_conectados++;
    int cantidad_actual = workers_conectados;
    pthread_mutex_unlock(&mutex_workers);

    log_info(logger, "## Se conecta el Worker %d - Cantidad total de Workers: %d", worker_id, cantidad_actual);

    // Simular comunicación con worker
    while (1) {
        t_header header = obtener_header(fd_worker);
        if (header == -1) {
            break;
        }
        // Procesar mensajes del worker según sea necesario
    }

    // Worker se desconectó - decrementar contador
    pthread_mutex_lock(&mutex_workers);
    workers_conectados--;
    cantidad_actual = workers_conectados;
    pthread_mutex_unlock(&mutex_workers);
    
    // Log obligatorio: Desconexión de Worker
    log_info(logger, "## Se desconecta el Worker %d - Cantidad total de Workers: %d", worker_id, cantidad_actual);
    
    // Limpiar memoria
    close(fd_worker);
    free(datos);

    return NULL;
}