#ifndef HILO_QUERY_CONTROL_H
#define HILO_QUERY_CONTROL_H

#include "types.h"

void* atender_query_control(void* arg);
void planificar_queries();
void planificar_queries_fifo();
void planificar_queries_prioridades();
void manejar_desconexion_query_control(t_query* query);
t_query* encontrar_query_por_id(int query_id);
void aplicar_aging(t_query* query);
//bool desalojar_query_por_prioridad(t_query* nueva_query);
void enviar_comando_a_worker(int worker_id, t_query* query, int program_counter); 
void manejar_fallo_envio_worker(int worker_id, t_query* query);  
//int encontrar_worker_para_desalojo(int prioridad_nueva_query);                      

#endif