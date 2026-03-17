#include <string.h>

#include <utils/estructuras.h>
#include <utils/serializacion.h>

#include "serializacion.h"

t_paquete* serializar_handshake_storage(int id_worker){
	t_paquete* paquete = crear_paquete(HANDSHAKE_WORKER);
	agregar_a_paquete(paquete, &id_worker, sizeof(id_worker));
	return paquete;
}

t_paquete* serializar_handshake_master(int id_worker){
	t_paquete* paquete = crear_paquete(HANDSHAKE_WORKER);
	agregar_a_paquete(paquete, &id_worker, sizeof(id_worker));
	return paquete;
}
