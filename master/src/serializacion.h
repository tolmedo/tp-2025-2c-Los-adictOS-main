#ifndef SERIALIZACION_H
#define SERIALIZACION_H

#include <utils/serializacion.h>

t_paquete* serializar_respuesta_handshake_worker();
t_paquete* serializar_respuesta_handshake_query_control();
t_paquete* serializar_respuesta_handshake_desconocido();
t_paquete* serializar_respuesta_create_file();

#endif