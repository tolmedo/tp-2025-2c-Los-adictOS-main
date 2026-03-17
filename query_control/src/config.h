#ifndef QUERY_CONTROL_CONFIG_H_
#define QUERY_CONTROL_CONFIG_H_

#include <commons/config.h>
#include <commons/log.h>

typedef struct {
    char* ip_master;
    char* puerto_master;
    char* log_level;
} t_config_query_control;

t_config_query_control* inicializar_config_query_control(char* path);
void liberar_config_query_control();

// Getters
char* obtener_ip_master();
char* obtener_puerto_master();
char* obtener_log_level();


#endif