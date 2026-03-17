#ifndef TYPES_H
#define TYPES_H

#include <commons/collections/list.h>
#include <pthread.h>
#include <stdint.h>  
#include <time.h>
#include <stdbool.h>

typedef enum {
    QUERY_READY,
    QUERY_EXEC,
    QUERY_EXIT
} t_estado_query;

typedef struct t_worker {
    int worker_id;
    int fd_worker;
    bool disponible;        
    pthread_mutex_t mutex;
} t_worker;

typedef struct t_query {
    int id;
    int fd_query;
    char* path;
    int prioridad;
    t_estado_query estado;
    int worker_asignado;
    pthread_mutex_t mutex;
    int program_counter;   
    struct timespec tiempo_ingreso_ready;
    bool desalojada;
} t_query;

#endif