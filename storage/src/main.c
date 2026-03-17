#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>

#include <commons/log.h>

#include <utils/conexiones.h>
#include <utils/serializacion.h>
#include <utils/estructuras.h>

#include "config.h"
#include "servidor.h"
#include "superblock.h"
#include "storage.h"


t_config_storage* config_storage;
t_superblock* superblock;
t_log* logger;

extern volatile bool storage_activo;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <archivo_configuracion>\n", argv[0]);
        return EXIT_FAILURE;
    }

    config_storage = inicializar_config_storage(argv[1]);
    if (config_storage == NULL) {
        fprintf(stderr, "Error al cargar configuración\n");
        return EXIT_FAILURE;
    }

    superblock = inicializar_superblock("storage/configs/superblock.config");
    if(superblock == NULL){
        fprintf(stderr, "Error al cargar superblock\n");
        return EXIT_FAILURE;
    }

    logger = log_create("storage.log", "STORAGE", true, log_level_from_string(obtener_log_level()));
    if (logger == NULL) {
        fprintf(stderr, "Error al crear logger\n");
        return EXIT_FAILURE;
    }

    inicializar_servidor();

    log_info(logger, "Storage iniciado. Presiona Enter para finalizar...");
    
    // En lugar de usar señales, simplemente esperamos un Enter del usuario
    getchar();
    
    log_info(logger, "Cerrando Storage...");
    storage_activo = false;
    
    finalizar_programa();
    return EXIT_SUCCESS;
}