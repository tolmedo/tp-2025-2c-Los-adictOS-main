#include "worker.h"
#include "hilo-master.h"
#include "hilo-storage.h"
#include "config.h"

#include <pthread.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include <stdlib.h>
#include <semaphore.h>

extern t_log* logger;
extern t_config_worker* config_worker;
extern t_list* tablas_de_paginas;

pthread_mutex_t mutex_ejecucion_query = PTHREAD_MUTEX_INITIALIZER;
bool query_en_ejecucion = false;
pthread_mutex_t mutex_tablas_paginas = PTHREAD_MUTEX_INITIALIZER;

sem_t sem_desalojo;

void inicializar_worker(int id_worker) {
    // Inicializar lista de tablas de páginas
    tablas_de_paginas = list_create();
    log_debug(logger, "Lista de tablas de páginas inicializada");

    sem_init(&sem_desalojo, 0, 0);

    // Conectar con Storage primero (para obtener tamaño de bloque)
    inicializar_storage();

    // Conectar con Master
    inicializar_master(id_worker);

    // Iniciar hilo para escuchar al Master
    pthread_t hilo_master;
    int* id_ptr = malloc(sizeof(int));
    *id_ptr = id_worker;
    pthread_create(&hilo_master, NULL, escuchar_master, id_ptr);
    pthread_detach(hilo_master);

    log_debug(logger, "Worker %d inicializado y escuchando al Master", id_worker);
}