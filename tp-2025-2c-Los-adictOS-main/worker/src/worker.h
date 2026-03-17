#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>
#include <stdbool.h>

extern pthread_mutex_t mutex_ejecucion_query;
extern bool query_en_ejecucion;

extern pthread_mutex_t mutex_tablas_paginas;

void inicializar_worker(int id_worker);

#endif