#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <commons/config.h>

t_config_master* config_master = NULL;

t_config_master* inicializar_config_master(char* path) {
    t_config* config = config_create(path);
    if (!config) return NULL;
    
    config_master = malloc(sizeof(t_config_master));
    config_master->puerto_escucha = strdup(config_get_string_value(config, "PUERTO_ESCUCHA"));
    config_master->algoritmo_planificacion = strdup(config_get_string_value(config, "ALGORITMO_PLANIFICACION"));
    config_master->tiempo_aging = config_get_int_value(config, "TIEMPO_AGING");
    config_master->log_level = strdup(config_get_string_value(config, "LOG_LEVEL"));
    
    config_destroy(config);
    return config_master;
}

void liberar_config_master() {
    if (!config_master) return;
    
    free(config_master->puerto_escucha);
    free(config_master->algoritmo_planificacion);
    free(config_master->log_level);
    free(config_master);
    config_master = NULL;
}

char* obtener_puerto_escucha() {
    return config_master ? config_master->puerto_escucha : NULL;
}

char* obtener_algoritmo_planificacion() {
    return config_master ? config_master->algoritmo_planificacion : NULL;
}

int obtener_tiempo_aging() {
    return config_master ? config_master->tiempo_aging : 0;
}

char* obtener_log_level() {
    return config_master ? config_master->log_level : "INFO";
}

bool usar_algoritmo_prioridades() {
    return config_master && 
           strcmp(config_master->algoritmo_planificacion, "PRIORIDADES") == 0;
}