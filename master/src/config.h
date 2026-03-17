#ifndef MASTER_CONFIG_H_
#define MASTER_CONFIG_H_

#include <commons/config.h>
#include <commons/log.h>

typedef struct {
    char* puerto_escucha;
    char* algoritmo_planificacion;
    int tiempo_aging;
    char* log_level;
} t_config_master;

t_config_master* inicializar_config_master(char* path);
void liberar_config_master();

// ------ GETTERS ------ //
char* obtener_puerto_escucha();
char* obtener_algoritmo_planificacion();
int obtener_tiempo_aging();
char* obtener_log_level();

#endif /* MASTER_CONFIG_H_ */