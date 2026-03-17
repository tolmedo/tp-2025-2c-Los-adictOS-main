#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <commons/log.h>
#include <signal.h>
#include <semaphore.h>

#include "config.h"
#include "worker.h"
#include "hilo-storage.h"
#include "query_interpreter.h"
#include "tabla_de_paginas.h"

#include <utils/conexiones.h>

t_config_worker* config_worker;
t_log* logger;
int worker_activo = 1;
int worker_id_global = -1;
memoria_interna_t* memoria_interna;
bool memoria_creada = false;
int tamanio_bloque;
t_list* tablas_de_paginas;

void finalizar_programa() {
    worker_activo = 0;
    if (config_worker) liberar_config_worker(config_worker);
    if (logger) log_destroy(logger);
    if (memoria_interna) {
        free(memoria_interna->marcos_ocupados);
        free(memoria_interna->algoritmo_reemplazo);
        free(memoria_interna->memoria);
        free(memoria_interna);
        memoria_interna = NULL;
    }
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <archivo_config> <ID_Worker>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int worker_id = atoi(argv[2]);
    worker_id_global = worker_id;

    config_worker = inicializar_config_worker(argv[1]);
    if (config_worker == NULL) {
        fprintf(stderr, "Error cargando configuración\n");
        return EXIT_FAILURE;
    }

    // Crear logger con ID del worker
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "worker_%d.log", worker_id);

    logger = log_create(log_filename, "WORKER", true, log_level_from_string(obtener_log_level_worker()));
    if (logger == NULL) {
        fprintf(stderr, "Error creando logger\n");
        return EXIT_FAILURE;
    }

    log_debug(logger, "Worker %d iniciado correctamente", worker_id);

    // Inicializar Worker (conecta con Storage y Master)
    inicializar_worker(worker_id);

    // Crear memoria interna después de obtener tamaño de bloque
    if (!memoria_creada) {
        memoria_interna = inicializar_memoria_interna(config_worker->tam_memoria, tamanio_bloque);
        memoria_creada = true;
    }

    log_debug(logger, "Worker %d esperando queries del Master...", worker_id);

    getchar();

    log_debug(logger, "Worker %d finalizando", worker_id);

    finalizar_programa();

    return EXIT_SUCCESS;
}