#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <commons/config.h>
#include <commons/log.h>

extern t_config_storage* config_storage;

t_config_storage* inicializar_config_storage(char* path) {
    t_config* config_temp = config_create(path);
    if (config_temp == NULL) {
        return NULL;
    }

    t_config_storage* config = malloc(sizeof(t_config_storage));
    if (config == NULL) {
        config_destroy(config_temp);
        return NULL;
    }

    config->puerto_escucha = strdup(config_get_string_value(config_temp, "PUERTO_ESCUCHA"));
    config->punto_montaje = strdup(config_get_string_value(config_temp, "PUNTO_MONTAJE"));
    config->retardo_operacion = config_get_int_value(config_temp, "RETARDO_OPERACION");
    config->retardo_acceso_bloque = config_get_int_value(config_temp, "RETARDO_ACCESO_BLOQUE");

    // Convertir string a bool para FRESH_START
    char* fresh_start_str = config_get_string_value(config_temp, "FRESH_START");
    config->fresh_start = (strcmp(fresh_start_str, "TRUE") == 0 || strcmp(fresh_start_str, "true") == 0);
    config->log_level = strdup(config_get_string_value(config_temp, "LOG_LEVEL"));
    
    config_destroy(config_temp);
    return config;
}

void liberar_config_storage(t_config_storage* config) {
    if (config != NULL) {
        free(config->puerto_escucha);
        free(config->punto_montaje);
        free(config->log_level);
        free(config);
    }
}

// ---------------- GETTERS ----------------------- //
char* obtener_puerto_escucha(){
    return config_storage ? config_storage->puerto_escucha : NULL;
}

bool obtener_fresh_start(){
    return config_storage ? config_storage->fresh_start : false;
}

char* obtener_punto_montaje(){
    return config_storage ? config_storage->punto_montaje : NULL;
}

int obtener_retardo_operacion(){
    return config_storage ? config_storage->retardo_operacion : 0;
}

int obtener_retardo_acceso_bloque(){
    return config_storage ? config_storage->retardo_acceso_bloque : 0;
}

char* obtener_log_level(){
    return config_storage ? config_storage->log_level : NULL;
}