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
#include "filesystem.h"


t_config_storage* config_storage;
t_superblock* superblock;
t_filesystem* filesystem;
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

    superblock = inicializar_superblock("/home/utnso/storage/superblock.config");
    if(superblock == NULL){
        fprintf(stderr, "Error al cargar superblock\n");
        return EXIT_FAILURE;
    }

    logger = log_create("storage.log", "STORAGE", true, log_level_from_string(obtener_log_level()));
    if (logger == NULL) {
        fprintf(stderr, "Error al crear logger\n");
        return EXIT_FAILURE;
    }

    // Inicializar filesystem
    log_debug(logger, "Inicializando filesystem...");
    filesystem = inicializar_filesystem();
    if (filesystem == NULL) {
        log_error(logger, "Error al inicializar filesystem");
        finalizar_programa();
        return EXIT_FAILURE;
    }
    
    log_debug(logger, "Filesystem inicializado correctamente");

    inicializar_servidor();

    log_debug(logger, "Storage iniciado. Presiona Enter para finalizar...");
    
    getchar();
    
    log_debug(logger, "Cerrando Storage...");
    storage_activo = false;
    
    finalizar_programa();
    return EXIT_SUCCESS;
}