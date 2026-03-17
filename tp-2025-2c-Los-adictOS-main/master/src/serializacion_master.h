#ifndef SERIALIZACION_H
#define SERIALIZACION_H

#include <stdint.h> 
#include <utils/serializacion.h>

t_paquete* serializar_respuesta_handshake_worker();
t_paquete* serializar_respuesta_handshake_query_control();
t_paquete* serializar_respuesta_handshake_desconocido();
t_paquete* serializar_respuesta_create_file();
t_paquete* serializar_comando_ejecutar_query(int query_id, char* path_query, int program_counter);
t_paquete* serializar_mensaje_lectura(char* nombre_file, char* tag, char* contenido);
t_paquete* serializar_mensaje_finalizacion(char* motivo);

#endif