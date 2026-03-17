#ifndef DESERIALIZACION_WORKER_H
#define DESERIALIZACION_WORKER_H

#include <stdint.h>
#include "utils/estructuras.h"

typedef struct {
    t_tipo_instruccion tipo;
    union {
        struct { char* nombre_file; char* tag; } create;
        struct { char* nombre_file; char* tag; int tamanio; } truncate;
        struct { char* nombre_file; char* tag; uint32_t direccion_base; char* contenido; } write;
        struct { char* nombre_file; char* tag; uint32_t direccion_base; uint32_t tamanio; } read;
        struct { char* nombre_file_origen; char* tag_origen; char* nombre_file_destino; char* tag_destino; } tag;
        struct { char* nombre_file; char* tag; } commit;
        struct { char* nombre_file; char* tag; } flush;
        struct { char* nombre_file; char* tag; } delete;
        struct { } end;
    } argumentos;
} t_instruccion;

// Para mensajes de red del Master
typedef struct {
    t_header header;
    union {
        struct { int query_id; char* path_query; int program_counter; } ejecutar_query;
        struct { int query_id; } desalojar_query;
    } payload;
} t_mensaje_master;

// Funciones para parsear instrucciones desde archivo
t_instruccion* parsear_instruccion(char* linea);
void destruir_instruccion(t_instruccion* instruccion);

// Funciones para mensajes de red
t_mensaje_master* deserializar_mensaje_master(t_header header, void* payload);
void destruir_mensaje_master(t_mensaje_master* mensaje);

#endif