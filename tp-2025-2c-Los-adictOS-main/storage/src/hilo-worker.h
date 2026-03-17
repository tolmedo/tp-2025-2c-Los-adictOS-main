#ifndef HILO_CPU_H
#define HILO_CPU_H


void* atender_worker(void* arg);
typedef struct {
    int fd_worker;
    int worker_id;
} t_worker_conn;


#endif