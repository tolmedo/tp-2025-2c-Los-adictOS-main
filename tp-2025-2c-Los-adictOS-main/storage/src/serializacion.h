#ifndef SERIALIZACION_H
#define SERIALIZACION_H

#include <utils/serializacion.h>

// SERIALIZACION DE HANDSHAKES
t_paquete* serializar_respuesta_handshake_worker(int tamanio_bloque);
t_paquete* serializar_respuesta_handshake_desconocido();

// SERIALIZACION DE OPERACIONES
t_paquete* serializar_respuesta_create_file();
t_paquete* serializar_respuesta_truncate_file(); // Solo avisa éxito/error (bool)
t_paquete* serializar_respuesta_write_block(); // Solo avisa éxito/error (bool)
t_paquete* serializar_respuesta_read_block(int resultado, void* contenido, int tamanio_leido); // Avisa éxito/error (bool) - envía contenido del bloque - tamaño del contenido
t_paquete* serializar_respuesta_remove_tag();
t_paquete* serializar_respuesta_commit_tag();
t_paquete* serializar_respuesta_tag_file();

t_paquete* serializar_error_operacion(char* motivo);

#endif