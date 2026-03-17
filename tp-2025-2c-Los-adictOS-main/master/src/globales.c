#include "globales.h"
#include <stddef.h>
#include <commons/collections/list.h>

t_list* queries_ready = NULL;
t_list* queries_exec = NULL;
t_list* lista_workers_info = NULL; 

pthread_mutex_t mutex_aging = PTHREAD_MUTEX_INITIALIZER;
bool aging_activo = false;
int tiempo_aging = 0;

pthread_mutex_t mutex_queries_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_queries_exec = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_workers_info = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_planificacion = PTHREAD_MUTEX_INITIALIZER;