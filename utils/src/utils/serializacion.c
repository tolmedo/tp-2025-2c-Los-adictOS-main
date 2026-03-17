#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "commons/log.h"
#include "commons/string.h"

#include "serializacion.h"
#include "estructuras.h"


extern t_log* logger;


bool enviar_header(t_header header, int socket_cliente){
	int bytes_enviados = send(socket_cliente, &header, sizeof(int), 0);
	if(bytes_enviados != sizeof(int)){
		log_error(logger, "Error al enviar el paquete: %s", strerror(errno));
		return false;
	}
	return true;
}

static void _crear_buffer(t_paquete* paquete){
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete* crear_paquete(t_header header) {
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->header = header;
	_crear_buffer(paquete);
	return paquete;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio){
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);
	paquete->buffer->size += tamanio + sizeof(int);
}

void eliminar_paquete(t_paquete* paquete){
    if (paquete == NULL) return;
    
    if (paquete->buffer != NULL) {
        free(paquete->buffer->stream); 
        free(paquete->buffer);          
    }
    free(paquete);                     
}

static void* _serializar_paquete(t_paquete* paquete, int bytes) {
	void* nuevo_buffer = malloc(bytes);
	int desplazamiento = 0;

	memcpy(nuevo_buffer + desplazamiento, &(paquete->header), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(nuevo_buffer + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(nuevo_buffer + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return nuevo_buffer;
}

bool enviar_paquete(t_paquete* paquete, int socket_cliente){
	int bytes_a_enviar = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = _serializar_paquete(paquete, bytes_a_enviar);
	int bytes_enviados = send(socket_cliente, a_enviar, bytes_a_enviar, 0);
	free(a_enviar);
    eliminar_paquete(paquete);

	if(bytes_enviados != bytes_a_enviar){
		log_error(logger, "Error al enviar el paquete: %s", strerror(errno));
		return false;
	}

	return true;
}

void enviar_paquete_sin_eliminar_paquete(t_paquete* paquete, int socket_cliente){
	int bytes_a_enviar = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = _serializar_paquete(paquete, bytes_a_enviar);
	send(socket_cliente, a_enviar, bytes_a_enviar, 0);
	free(a_enviar);
}




