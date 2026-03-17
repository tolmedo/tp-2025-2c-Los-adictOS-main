#ifndef CONFIG_WORKER_H
#define CONFIG_WORKER_H

#include <commons/config.h>

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

// Funciones de inicialización y destrucción
t_config_worker* inicializar_config_worker(char* path);
void liberar_config_worker(t_config_worker* config);

// Getters
char* obtener_ip_master(void);
char* obtener_puerto_master(void);
char* obtener_ip_storage(void);
char* obtener_puerto_storage(void);
int obtener_tam_memoria(void);
int obtener_retardo_memoria(void);
char* obtener_algoritmo_reemplazo(void);
char* obtener_path_queries(void);
char* obtener_log_level_worker(void);
int obtener_id_worker(void);

#endif