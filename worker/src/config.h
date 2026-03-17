#ifndef WORKER_CONFIG_H_
#define WORKER_CONFIG_H_

#include <commons/config.h>
#include <commons/log.h>

typedef struct {
    char* ip_master;
    char* puerto_master;
    char* ip_storage;
    char* puerto_storage;
    int tam_memoria;
    int retardo_memoria;
    char* algoritmo_reemplazo;
    char* path_queries;
    char* log_level;
} t_config_worker;

t_config_worker* inicializar_config_worker(char* path);
void liberar_config_worker(t_config_worker* config);


// Getters
char* obtener_ip_master();
char* obtener_puerto_master();
char* obtener_ip_storage();
char* obtener_puerto_storage();
int obtener_tam_memoria();
int obtener_retardo_memoria();
char* obtener_algoritmo_reemplazo();
char* obtener_path_queries();
char* obtener_log_level_worker();

#endif