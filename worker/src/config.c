#include "config.h"
#include <stdlib.h>
#include <string.h>

extern t_config_worker* config_worker;

t_config_worker* inicializar_config_worker(char* path) {
    t_config* config_temp = config_create(path);
    if (config_temp == NULL) {
        return NULL;
    }

    t_config_worker* config = malloc(sizeof(t_config_worker));
    if (config == NULL) {
        config_destroy(config_temp);
        return NULL;
    }

    config->ip_master = strdup(config_get_string_value(config_temp, "IP_MASTER"));
    config->puerto_master = strdup(config_get_string_value(config_temp, "PUERTO_MASTER"));
    config->ip_storage = strdup(config_get_string_value(config_temp, "IP_STORAGE"));
    config->puerto_storage = strdup(config_get_string_value(config_temp, "PUERTO_STORAGE"));
    config->tam_memoria = config_get_int_value(config_temp, "TAM_MEMORIA");
    config->retardo_memoria = config_get_int_value(config_temp, "RETARDO_MEMORIA");
    config->algoritmo_reemplazo = strdup(config_get_string_value(config_temp, "ALGORITMO_REEMPLAZO"));
    config->path_queries = strdup(config_get_string_value(config_temp, "PATH_QUERIES"));
    config->log_level = strdup(config_get_string_value(config_temp, "LOG_LEVEL"));

    config_destroy(config_temp);
    return config;
}

void liberar_config_worker(t_config_worker* config) {
    if (config != NULL) {
        free(config->ip_master);
        free(config->puerto_master);
        free(config->puerto_storage);
        free(config->ip_storage);
        free(config->algoritmo_reemplazo);
        free(config->path_queries);
        free(config->log_level);
        free(config);
    }
}


// -------------------- GETTERS -------------------- //

char* obtener_ip_master() {
    return config_worker ? config_worker->ip_master : NULL;
}

char* obtener_puerto_master() {
    return config_worker ? config_worker->puerto_master : NULL;
}

char* obtener_ip_storage() {
    return config_worker ? config_worker->ip_storage : NULL;
}

char* obtener_puerto_storage() {
    return config_worker ? config_worker->puerto_storage : NULL;
}

int obtener_tam_memoria() {
    return config_worker ? config_worker->tam_memoria : 0;
}

int obtener_retardo_memoria() {
    return config_worker ? config_worker->retardo_memoria : 0;
}

char* obtener_algoritmo_reemplazo() {
    return config_worker ? config_worker->algoritmo_reemplazo : NULL;
}

char* obtener_path_queries() {
    return config_worker ? config_worker->path_queries : NULL;
}

char* obtener_log_level_worker() {
    return config_worker ? config_worker->log_level : NULL;
}
