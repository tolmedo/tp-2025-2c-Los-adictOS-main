#ifndef HILO_WORKER_H
#define HILO_WORKER_H

#include "types.h" 

void* atender_worker(void* arg);
void manejar_desconexion_worker(int worker_id);
int obtener_socket_de_worker(int worker_id);
void enviar_cancelacion_a_worker(int worker_id, int query_id);
void enviar_desalojo_a_worker(int worker_id, int query_id);
void agregar_worker_info(int worker_id, int fd_worker);
void remover_worker_info(int worker_id);
t_worker* obtener_worker_disponible(void);    
void marcar_worker_ocupado(int worker_id);      
void marcar_worker_disponible(int worker_id);           

#endif