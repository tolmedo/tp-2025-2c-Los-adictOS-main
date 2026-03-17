#ifndef SERIALIZACION_WORKER_H
#define SERIALIZACION_WORKER_H

#include <utils/estructuras.h>
#include <utils/serializacion.h>
#include <stddef.h>

t_paquete* serializar_handshake(int id_worker);

t_paquete* serializar_fin_query(int query_id, char* motivo);

t_paquete* serializar_operacion_create(char* nombre_file, char* tag, int query_id);
t_paquete* serializar_operacion_truncate(char* nombre_file, char* tag, int tamano, int query_id);
t_paquete* serializar_operacion_write_block(char* nombre_file, char* tag, int num_bloque, 
                                            void* datos, int tamano, int query_id);
t_paquete* serializar_operacion_read_block(char* nombre_file, char* tag, int num_bloque, int query_id);
t_paquete* serializar_operacion_tag(char* nombre_file_origen, char* tag_origen,
                                   char* nombre_file_destino, char* tag_destino, int query_id);
t_paquete* serializar_operacion_commit(char* nombre_file, char* tag, int query_id);
t_paquete* serializar_operacion_delete(char* nombre_file, char* tag, int query_id);

#endif