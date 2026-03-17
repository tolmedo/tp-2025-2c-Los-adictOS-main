#include <stdlib.h>
#include <string.h>

#include "commons/log.h"
#include "commons/string.h"

#include <utils/estructuras.h>
#include <utils/serializacion.h>

#include "serializacion.h"

extern t_log* logger;

t_paquete* serializar_respuesta_handshake_worker(int tamanio_bloque){
	t_paquete* paquete = crear_paquete(HANDSHAKE_OK);
	agregar_a_paquete(paquete, &tamanio_bloque, sizeof(tamanio_bloque));
	return paquete;
}

t_paquete* serializar_respuesta_handshake_desconocido(){
	t_paquete* paquete = crear_paquete(HANDSHAKE_ERROR);
	return paquete;
}

t_paquete* serializar_respuesta_create_file(){
	t_paquete *paquete = crear_paquete(RESPUESTA_CREATE_FILE);
	return paquete;
}