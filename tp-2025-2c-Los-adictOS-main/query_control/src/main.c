#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <commons/log.h>
#include <signal.h>
#include <stdbool.h>

#include <utils/conexiones.h>

#include "config.h"
#include "query_control.h"
#include "hilo-master.h"

t_config_query_control* config_query_control;
t_log* logger;
extern volatile bool query_control_activo;

static void* esperar_enter(void* _) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { /* no-op */ }
    if (logger) log_debug(logger, "ENTER detectado. Cerrando Query Control...");
    qc_solicitar_cierre();   
    return NULL;
}


int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <archivo_config> <archivo_query> <prioridad>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char* archivo_query = argv[2];
    int prioridad = atoi(argv[3]);

    config_query_control = inicializar_config_query_control(argv[1]);
    if (config_query_control == NULL) {
        fprintf(stderr, "Error cargando configuración\n");
        return EXIT_FAILURE;
    }

    logger = log_create("query_control.log", "QUERY_CONTROL", true, log_level_from_string(obtener_log_level()));
    if (logger == NULL) {
        fprintf(stderr, "Error creando logger\n");
        return EXIT_FAILURE;
    }

    log_debug(logger, "=== Iniciando Query Control ===");
    log_debug(logger, "Archivo Query: %s", archivo_query);
    log_debug(logger, "Prioridad: %d", prioridad);
    pthread_t th_enter;
    pthread_create(&th_enter, NULL, esperar_enter, NULL);
    pthread_detach(th_enter);

    inicializar_query_control(archivo_query, prioridad);
    finalizar_programa();
    return EXIT_SUCCESS;
}
