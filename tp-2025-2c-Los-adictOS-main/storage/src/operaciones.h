#ifndef OPERACIONES_H
#define OPERACIONES_H

//FUNCIONES DE RETARDO
void aplicar_retardo_operacion();
void aplicar_retardo_acceso_bloque();

//FUNCIONES DE OPERACIONES
void create_file(int socket_worker, char* nombre_file, char* nombre_tag, int query_id);
void truncate_file(int socket_worker, char* nombre_file, char* nombre_tag, int nuevo_tamanio, int query_id);
void write_block(int socket_worker, char* nombre_file, char* nombre_tag, int bloque_logico, void* contenido, int query_id, int tamanio);
void read_block(int socket_worker, char* nombre_file, char* nombre_tag, int bloque_logico, int query_id);
void remove_tag(int socket_worker, char* nombre_file, char* nombre_tag, int query_id);
void commit_tag(int socket_worker, char* nombre_file, char* nombre_tag, int query_id);
void tag_file(int socket_worker, char* nombre_file_origen, char* tag_origen, char* nombre_file_destino, char* tag_destino, int query_id);

#endif