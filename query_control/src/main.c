#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <commons/log.h>
#include <signal.h>

#include <utils/conexiones.h>

#include "config.h"
#include "query_control.h"

t_config_query_control* config_query_control;
t_log* logger;

void terminar_query_control(int signal){
    log_info(logger, "Señal recibida (%d). Finalizando Query Control", signal);
    finalizar_programa();
    exit(EXIT_SUCCESS);
}


int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <archivo_config> <archivo_query> <prioridad>\n", argv[0]);
        return EXIT_FAILURE;
    }

    signal(SIGINT, terminar_query_control);

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

    log_info(logger, "=== Iniciando Query Control ===");
    log_info(logger, "Archivo Query: %s", archivo_query);
    log_info(logger, "Prioridad: %d", prioridad);

    inicializar_query_control(archivo_query, prioridad);

    log_info(logger, "## Query Finalizada - COMPLETADA");
    
    finalizar_programa();
    return EXIT_SUCCESS;
}
