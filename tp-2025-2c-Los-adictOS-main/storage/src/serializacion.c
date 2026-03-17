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
	agregar_a_paquete(paquete, &tamanio_bloque, sizeof(int));
	return paquete;
}

t_paquete* serializar_respuesta_handshake_desconocido(){
	t_paquete* paquete = crear_paquete(HANDSHAKE_ERROR);
	return paquete;
}

// ===================== RESPUESTAS OPERACIONES =====================
t_paquete* serializar_respuesta_create_file(){
	t_paquete *paquete = crear_paquete(RESPUESTA_CREATE_FILE);
	return paquete;
}

t_paquete* serializar_respuesta_truncate_file(){
	t_paquete *paquete = crear_paquete(RESPUESTA_TRUNCATE_FILE);
	return paquete;
}

t_paquete* serializar_respuesta_write_block(){
	t_paquete *paquete = crear_paquete(RESPUESTA_WRITE_BLOCK);
	return paquete;
}

t_paquete* serializar_respuesta_read_block(int resultado, void* contenido, int tamanio_leido){
    t_paquete *paquete = crear_paquete(RESPUESTA_READ_BLOCK);

    agregar_a_paquete(paquete, &resultado, sizeof(int));
    agregar_a_paquete(paquete, &tamanio_leido, sizeof(int));
    agregar_a_paquete(paquete, contenido, tamanio_leido);

    return paquete;
}

t_paquete* serializar_respuesta_remove_tag(){
	t_paquete *paquete = crear_paquete(RESPUESTA_REMOVE_TAG);
	return paquete;
}

t_paquete* serializar_respuesta_commit_tag(){
	t_paquete *paquete = crear_paquete(RESPUESTA_COMMIT_TAG);
	return paquete;
}

t_paquete* serializar_respuesta_tag_file(){
	t_paquete *paquete = crear_paquete(RESPUESTA_TAG_FILE);
	return paquete;
}

t_paquete* serializar_error_operacion(char* motivo){
	t_paquete* paquete = crear_paquete(ERROR_OPERACION);
	agregar_a_paquete(paquete, motivo, strlen(motivo) + 1);
	return paquete;
}