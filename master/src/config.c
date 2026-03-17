#include "config.h"
#include <stdlib.h>
#include <string.h>

extern t_config_master* config_master;

t_config_master* inicializar_config_master(char* path) {
    t_config* config_temp = config_create(path);
    if (config_temp == NULL) {
        return NULL;
    }

    t_config_master* config = malloc(sizeof(t_config_master));
    if (config == NULL) {
        config_destroy(config_temp);
        return NULL;
    }

    config->puerto_escucha = strdup(config_get_string_value(config_temp, "PUERTO_ESCUCHA"));
    config->tiempo_aging = config_get_int_value(config_temp, "TIEMPO_AGING");
    config->algoritmo_planificacion = strdup(config_get_string_value(config_temp, "ALGORITMO_PLANIFICACION"));
    config->log_level = strdup(config_get_string_value(config_temp, "LOG_LEVEL"));

    config_destroy(config_temp);
    return config;
}

void liberar_config_master() {
    if (config_master != NULL) {
        free(config_master->puerto_escucha);
        free(config_master->algoritmo_planificacion);
        free(config_master->log_level);
        free(config_master);
        config_master = NULL;
    }
}

// ----------------- GETTERS ----------------- //
char* obtener_puerto_escucha(){
    return config_master ? config_master->puerto_escucha : NULL;
}

char* obtener_algoritmo_planificacion(){
    return config_master ? config_master->algoritmo_planificacion : NULL;
}

int obtener_tiempo_aging(){
    return config_master ? config_master->tiempo_aging : 0;
}

char* obtener_log_level(){
    return config_master ? config_master->log_level : NULL;
}
