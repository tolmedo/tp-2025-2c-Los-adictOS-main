#ifndef GLOBALES_H
#define GLOBALES_H

#include "types.h"
#include <commons/collections/list.h>
#include <pthread.h>
#include <stdbool.h>

extern t_list* queries_ready;
extern t_list* queries_exec;
extern t_list* lista_workers_info;

extern pthread_mutex_t mutex_aging;
extern bool aging_activo;
extern int tiempo_aging;

extern pthread_mutex_t mutex_queries_ready;
extern pthread_mutex_t mutex_queries_exec;
extern pthread_mutex_t mutex_workers_info; 

extern pthread_mutex_t mutex_planificacion;

#endif