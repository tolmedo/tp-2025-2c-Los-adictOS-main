#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "commons/log.h"

#include "hilo-master.h"
#include "serializacion.h"
//#include "deserializacion.h"
#include "config.h"
#include "query_control.h"

extern t_log* logger;
extern t_config* config_query_control;

void inicializar_query_control(char* archivo_query, int prioridad){
    inicializar_master(archivo_query, prioridad);
}

void finalizar_programa() {
    if (config_query_control) liberar_config_query_control(config_query_control);
    if (logger) log_destroy(logger);
}