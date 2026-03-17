#include <string.h>
#include <commons/log.h>

#include <utils/estructuras.h>
#include <utils/serializacion.h>

#include "serializacion.h"

extern t_log* logger;

t_paquete* serializar_handshake_master(char* archivo_query, int prioridad){
    t_paquete* paquete = crear_paquete(HANDSHAKE_QUERY_CONTROL);

    agregar_a_paquete(paquete, archivo_query, strlen(archivo_query) + 1);
    agregar_a_paquete(paquete, &prioridad, sizeof(int));

    return paquete;
}
