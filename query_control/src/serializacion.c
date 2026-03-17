#include <string.h>

#include <utils/estructuras.h>
#include <utils/serializacion.h>

#include "serializacion.h"

t_paquete* serializar_handshake_master(char* archivo_query, int prioridad){
    t_paquete* paquete = crear_paquete(HANDSHAKE_QUERY_CONTROL);

    // Serializar el string (longitud + contenido)
    int longitud_query = strlen(archivo_query) + 1;  // +1 para el \0
    agregar_a_paquete(paquete, &longitud_query, sizeof(longitud_query));
    agregar_a_paquete(paquete, archivo_query, longitud_query);

    // Serializar la prioridad
    agregar_a_paquete(paquete, &prioridad, sizeof(prioridad));

    return paquete;
}