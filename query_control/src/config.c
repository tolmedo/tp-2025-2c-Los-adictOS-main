#include "config.h"
#include <stdlib.h>
#include <string.h>

extern t_config_query_control* config_query_control;

t_config_query_control* inicializar_config_query_control(char* path) {
    t_config* config_temp = config_create(path);
    if (config_temp == NULL) {
        return NULL;
    }

    t_config_query_control* config = malloc(sizeof(t_config_query_control));
    if (config == NULL) {
        config_destroy(config_temp);
        return NULL;
    }

    config->ip_master = strdup(config_get_string_value(config_temp, "IP_MASTER"));
    config->puerto_master = strdup(config_get_string_value(config_temp, "PUERTO_MASTER"));
    config->log_level = strdup(config_get_string_value(config_temp, "LOG_LEVEL"));

    config_destroy(config_temp);
    return config;
}

void liberar_config_query_control() {
    if (config_query_control != NULL) {
        free(config_query_control->ip_master);
        free(config_query_control->puerto_master);
        free(config_query_control->log_level);
        free(config_query_control);
    }
}

// ---------------- GETTERS ----------------------- //

char* obtener_ip_master(){
    return config_query_control ? config_query_control->ip_master : NULL;
}

char* obtener_puerto_master(){
    return config_query_control ? config_query_control->puerto_master : 0;
}

char* obtener_log_level(){
    return config_query_control? config_query_control->log_level : NULL;
}
