#ifndef QUERY_INTERPRETER_H
#define QUERY_INTERPRETER_H

#include <pthread.h>
#include <stdbool.h>
#include "deserializacion.h"

extern pthread_mutex_t mutex_control_query;

void ejecutarQuery(char* path_query, int query_id, int pc_inicial);
void marcar_query_desalojada();
int obtener_pc_actual();
void ejecutar_instruccion(t_instruccion* inst, int query_id, int pc);

void flush_por_desalojo(int query_id);
void liberar_marcos_query(int query_id);

#endif