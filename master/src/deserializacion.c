#include <stdlib.h>
#include <string.h>

#include <utils/estructuras.h>
#include <utils/deserializacion.h>

#include "deserializacion.h"

static t_deserializado_master* _deserializar_handshake_worker(void* payload) {
    t_deserializado_master* deserializado = malloc(sizeof(*deserializado));
    int offset = 0;
    
    deserializado->header = HANDSHAKE_WORKER;
    deserializado->payload.handshake_worker.worker_id = obtener_un_entero(payload, &offset);
    
    return deserializado;
}

static t_deserializado_master* _deserializar_handshake_query_control(void* payload) {
    t_deserializado_master* deserializado = malloc(sizeof(*deserializado));
    int offset = 0;
    
    deserializado->header = HANDSHAKE_QUERY_CONTROL;
    deserializado->payload.handshake_query_control.archivo_query = obtener_un_string(payload, &offset);
    deserializado->payload.handshake_query_control.prioridad = obtener_un_entero(payload, &offset);
    
    return deserializado;
}

t_deserializado_master* deserializar_mensaje_master(t_header header, void* payload) {
    t_deserializado_master* deserializado = NULL;

    switch (header) {
        case HANDSHAKE_WORKER:
            deserializado = _deserializar_handshake_worker(payload);
            break;
            
        case HANDSHAKE_QUERY_CONTROL:
            deserializado = _deserializar_handshake_query_control(payload);
            break;
            
        default:
            break;
    }

    return deserializado;
}

void destruir_deserializado_master(t_deserializado_master* deserializado) {
    if (!deserializado) return;

    switch (deserializado->header) {
        case HANDSHAKE_QUERY_CONTROL:
            free(deserializado->payload.handshake_query_control.archivo_query);
            break;
            
        case HANDSHAKE_WORKER:
            // No hay strings que liberar
            break;
            
        default:
            break;
    }
    
    free(deserializado);
}