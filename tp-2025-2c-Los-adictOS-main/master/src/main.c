#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <signal.h>
#include "types.h"
#include "config.h"
#include "servidor.h"
#include "globales.h"
#include "hilo-query_control.h"

t_log* logger = NULL;
pthread_t thread_servidor;

extern volatile bool programa_activo;
// Destructor para t_query usado al finalizar el programa
static void destruir_query_shutdown(void* elem) {
    t_query* query = (t_query*) elem;

    if (!query) return;

    // Destruir el mutex si fue inicializado
    pthread_mutex_destroy(&query->mutex);

    // Liberar el path duplicado en _handshake
    free(query->path);

    // Finalmente liberar la estructura
    free(query);
}


void liberar_recursos_globales() {
    log_debug(logger, "Liberando recursos globales...");
    
    pthread_mutex_destroy(&mutex_aging);
    pthread_mutex_destroy(&mutex_workers_info);
    pthread_mutex_destroy(&mutex_queries_exec);
    pthread_mutex_destroy(&mutex_queries_ready);
    
    if (queries_ready) {
        list_destroy_and_destroy_elements(queries_ready, destruir_query_shutdown);
        queries_ready = NULL;
    }
    if (queries_exec) {
        list_destroy_and_destroy_elements(queries_exec, destruir_query_shutdown);
        queries_exec = NULL;
    }
    
    if (lista_workers_info) {
        for (int i = 0; i < list_size(lista_workers_info); i++) {
            t_worker* info = list_get(lista_workers_info, i);
            pthread_mutex_destroy(&info->mutex);
            free(info);
        }
        list_destroy(lista_workers_info);
        lista_workers_info = NULL;
    }
    
    log_debug(logger, "Recursos globales liberados");
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <archivo_configuracion>\n", argv[0]);
        return EXIT_FAILURE;
    }

    queries_ready = list_create();
    queries_exec = list_create();
    lista_workers_info = list_create();  // ← Única lista de workers

    config_master = inicializar_config_master(argv[1]);
    if (config_master == NULL) {
        fprintf(stderr, "Error al cargar configuración\n");
        liberar_recursos_globales();
        return EXIT_FAILURE;
    }

    logger = log_create("master.log", "MASTER", true, 
                       log_level_from_string(obtener_log_level()));
    if (logger == NULL) {
        fprintf(stderr, "Error al crear logger\n");
        liberar_config_master();
        liberar_recursos_globales();
        return EXIT_FAILURE;
    }

    aging_activo = (usar_algoritmo_prioridades() && obtener_tiempo_aging() > 0);

    log_debug(logger, "=== MASTER INICIADO ===");
    log_debug(logger, "Algoritmo: %s", obtener_algoritmo_planificacion());
    log_debug(logger, "Aging: %d ms", obtener_tiempo_aging());
    log_debug(logger, "Log Level: %s", obtener_log_level());
    log_debug(logger, "Puerto: %s", obtener_puerto_escucha());

    inicializar_servidor();

    log_debug(logger, "Master iniciado correctamente. Presiona Enter para finalizar...");
    
    getchar();

    log_debug(logger, "Iniciando cierre del Master...");
    programa_activo = false;
    
    finalizar_programa();
    liberar_recursos_globales();
    liberar_config_master();
    
    if (logger) {
        log_destroy(logger);
    }
    
    return EXIT_SUCCESS;
}