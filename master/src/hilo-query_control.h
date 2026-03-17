#ifndef HILO_QUERY_CONTROL_H
#define HILO_QUERY_CONTROL_H

void* atender_query_control(void* arg);

typedef struct{
    int id;
    int fd_query;
    char* path;
    int prioridad;
}t_query;

#endif