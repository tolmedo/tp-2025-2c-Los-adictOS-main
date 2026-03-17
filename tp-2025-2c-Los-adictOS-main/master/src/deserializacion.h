#ifndef DESERIALIZACION_MASTER_H
#define DESERIALIZACION_MASTER_H

#include <stdint.h>
#include "utils/estructuras.h"

typedef struct {
    t_header header;
    union {
        struct { int worker_id; } handshake_worker;
        struct { char* archivo_query; int prioridad; } handshake_query_control;
        struct { int query_id; int program_counter; } desalojo_query;
    } payload;
} t_deserializado_master;

t_deserializado_master* deserializar_mensaje_master(t_header header, void* payload);
void destruir_deserializado_master(t_deserializado_master* deserializado);

#endif