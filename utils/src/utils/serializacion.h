#ifndef SERIALIZACION_UTILS_H
#define SERIALIZACION_UTILS_H

#include <stdbool.h>
#include "estructuras.h"

typedef struct {
	int size;
	void* stream;
} t_buffer;

typedef struct {
	t_header header;
	t_buffer* buffer;
} t_paquete;


bool enviar_header(t_header header, int socket_cliente);
bool enviar_paquete(t_paquete* paquete, int socket_cliente);
void enviar_paquete_sin_eliminar_paquete(t_paquete* paquete, int socket_cliente);
t_paquete* crear_paquete(t_header header);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void eliminar_paquete(t_paquete* paquete);


#endif