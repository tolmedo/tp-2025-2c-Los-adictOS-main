#ifndef STORAGE_CONFIG_H_
#define STORAGE_CONFIG_H_

#include <stdbool.h>

typedef struct {
    char* puerto_escucha;
    bool fresh_start;
    char* punto_montaje;
    int retardo_operacion;
    int retardo_acceso_bloque;
    char* log_level;
} t_config_storage;

t_config_storage* inicializar_config_storage(char* path);
void liberar_config_storage(t_config_storage* config);

// Getters
char* obtener_puerto_escucha();
bool obtener_fresh_start();
char* obtener_punto_montaje();
int obtener_retardo_operacion();
int obtener_retardo_acceso_bloque();
char* obtener_log_level();

#endif