#include <stdlib.h>
#include <string.h>

#include "commons/log.h"
#include "commons/string.h"

#include <utils/estructuras.h>
#include <utils/serializacion.h>

#include "serializacion.h"

extern t_log* logger;

t_paquete* serializar_respuesta_handshake_worker(){
	t_paquete* paquete = crear_paquete(HANDSHAKE_OK);
	return paquete;
}

t_paquete* serializar_respuesta_handshake_query_control(){
	t_paquete* paquete = crear_paquete(HANDSHAKE_OK);
	return paquete;
}

t_paquete* serializar_respuesta_handshake_desconocido(){
	t_paquete* paquete = crear_paquete(HANDSHAKE_ERROR);
	return paquete;
}