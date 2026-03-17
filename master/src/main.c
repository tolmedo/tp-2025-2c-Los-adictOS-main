#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <commons/log.h>
#include "config.h"
#include "servidor.h"

t_config_master* config_master;
t_log* logger;
pthread_t thread_servidor;

extern volatile bool programa_activo;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <archivo_configuracion>\n", argv[0]);
        return EXIT_FAILURE;
    }

    config_master = inicializar_config_master(argv[1]);
    if (config_master == NULL) {
        fprintf(stderr, "Error al cargar configuración\n");
        return EXIT_FAILURE;
    }

    logger = log_create("master.log", "MASTER", true, log_level_from_string(obtener_log_level()));
    if (logger == NULL) {
        fprintf(stderr, "Error al crear logger\n");
        return EXIT_FAILURE;
    }

    inicializar_servidor();

    log_info(logger, "Master iniciado. Presiona Enter para finalizar...");
    
    // En lugar de usar señales, simplemente esperamos un Enter del usuario
    getchar();
    
    log_info(logger, "Cerrando Master...");
    programa_activo = false;
    
    finalizar_programa();
    return EXIT_SUCCESS;
}
