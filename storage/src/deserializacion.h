#ifndef DESERIALIZACION_H
#define DESERIALIZACION_H

#include "utils/estructuras.h"

typedef struct {
    t_header header;
    union {
        struct{ char* nombre_file; char* tag;} crear_file; // CREATE_FILE -> <NOMBRE_FILE>:<TAG>
        struct{ char* nombre_file; char* tag; int tamanio; } truncar_archivo; // TRUNCATE_FILE -> <NOMBRE_FILE>:<TAG> <TAMAÑO>
        struct{ char* nombre_file_origen; char* tag_origen; char* nombre_file_destino; char* tag_destino;} crear_copia; // TAG_FILE -> <NOMBRE_FILE_ORIGEN>:<TAG_ORIGEN> <NOMBRE_FILE_DESTINO>:<TAG_DESTINO>
        struct{ char* nombre_file; char* tag; } confirmar_tag; // COMMIT_TAG -> <NOMBRE_FILE>:<TAG>
        struct{ char* nombre_file; char* tag; int bloque_logico; void* contenido; int tamanio_contenido;} escribir_bloque; // WRITE_BLOCK -> <NOMBRE_FILE>:<TAG> <BLOQUE_LOGICO> <CONTENIDO>
        struct{ char* nombre_file; char* tag; int bloque_logico; } leer_bloque; // READ_BLOCK -> <NOMBRE_FILE>:<TAG> <BLOQUE_LOGICO>
        struct{ char* nombre_file; char* tag; } eliminar_tag; // REMOVE_TAG -> <NOMBRE_FILE>:<TAG> <BLOQUE_LOGICO>
        struct { int worker_id; } handshake_worker;
    } payload; 
} t_deserializado; 

t_deserializado* deserializar(t_header header, void* payload);
void destruir_deserializado(t_deserializado* solicitud);

#endif