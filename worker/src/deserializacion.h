#ifndef DESERIALIZACION_H
#define DESERIALIZACION_H

#include "utils/estructuras.h"

typedef struct {
    t_tipo_instruccion tipo;
    union {
        struct {char* nombre_file; char* tag; } create; //  CREATE <NOMBRE_FILE>:<TAG> 
        struct { char* nombre_file; char* tag; int tamanio; } truncate; //  TRUNCATE <NOMBRE_FILE>:<TAG> <TAMAÑO> 
        struct { } write; //  WRITE <NOMBRE_FILE>:<TAG> <DIRECCIÓN BASE> <CONTENIDO>
        struct { } read; //  READ <NOMBRE_FILE>:<TAG> <DIRECCIÓN BASE> <TAMAÑO>
        struct { } tag; //  TAG <NOMBRE_FILE_ORIGEN>:<TAG_ORIGEN> <NOMBRE_FILE_DESTINO>:<TAG_DESTINO> 
        struct { } commit; //  COMMIT <NOMBRE_FILE>:<TAG> 
        struct{ } flush; //  FLUSH <NOMBRE_FILE>:<TAG> 
        struct { } end; // SIN ARGUMENTOS
    } argumentos;
} t_instruccion;

// Para mensajes de red del Master
typedef struct {
    t_header header;
    union {
        struct { int query_id; char* path_query; int program_counter; } ejecutar_query;        // Cuando el Master envía una Query para ejecutar
        struct { int query_id; } desalojar_query;        // Cuando el Master solicita desalojo
    } payload;
} t_mensaje_master;

// Funciones para parsear instrucciones desde archivo
t_instruccion* parsear_instruccion(char* linea);
void destruir_instruccion(t_instruccion* instruccion);

// Funciones para mensajes de red
t_mensaje_master* deserializar_mensaje_master(t_header header, void* payload);
void destruir_mensaje_master(t_mensaje_master* mensaje);

#endif