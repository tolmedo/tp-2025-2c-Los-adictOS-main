#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <commons/log.h>
#include <signal.h>
#include "config.h"

#include <utils/conexiones.h>

#include "hilo-storage.h"
#include "worker.h"

t_config_worker* config_worker;
t_log* logger;
int worker_activo = 1;

void finalizar_programa() {
    worker_activo = 0;
    if (config_worker) liberar_config_worker(config_worker);
    if (logger) log_destroy(logger);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <archivo_config> <ID_Worker>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int worker_id = atoi(argv[2]);

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
    inicializar_worker(worker_id);

    log_info(logger, "Worker %d iniciado correctamente", worker_id);

    getchar();

    log_info(logger, "Worker %d finalizando", worker_id);

    finalizar_programa();

    return EXIT_SUCCESS;
}
