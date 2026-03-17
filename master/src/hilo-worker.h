#ifndef HILO_WORKER_H
#define HILO_WORKER_H

void* atender_worker(void* arg);
typedef struct {
    int fd_worker;
    int worker_id;
} t_datos_worker;

#endif