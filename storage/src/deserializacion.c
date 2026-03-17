#include <stdlib.h>

#include <utils/estructuras.h>
#include <utils/deserializacion.h>

#include "deserializacion.h"

static t_deserializado* _deserializar_create_file(void* payload){
    t_deserializado* deserializado = malloc(sizeof(*deserializado));
    int offset = 0;
    deserializado->header = CREATE_FILE;
    deserializado->payload.crear_file.nombre_file = obtener_un_string(payload, &offset);
    deserializado->payload.crear_file.tag = obtener_un_string(payload, &offset);
    return deserializado;
}
static t_deserializado* _deserializar_handshake_worker(void* payload){
    t_deserializado* d = malloc(sizeof(*d));
    int offset = 0;
    d->header = HANDSHAKE_WORKER;
    d->payload.handshake_worker.worker_id = obtener_un_entero(payload, &offset);
    return d;
}


// TODO: Y así con c/u de las operaciones: TAG_FILE, READ_BLOCK ....

t_deserializado* deserializar(t_header header, void* payload){ // Faltan deserializar mas .... Si el mensaje tiene argumentos agregarlos a t_deserializado 
    t_deserializado* deserializado = NULL;

    switch (header) {
         case HANDSHAKE_WORKER: {
           deserializado = _deserializar_handshake_worker(payload);
           break;
        }
        case CREATE_FILE: {
            deserializado = _deserializar_create_file(payload);
            break;
        }

        default: {
            break;
        }
    }

    return deserializado;
}

void destruir_deserializado(t_deserializado* deserializado) { // Si los que faltan deserializar tienen internamente campos tipo char* o similar, faltaria agregar la liberacion de ese campo
    if (!deserializado) return;

    switch (deserializado->header) {
        case CREATE_FILE: {
            free(deserializado->payload.crear_file.nombre_file);
            free(deserializado->payload.crear_file.tag);
            break;
        }  

        default: {
            break;
        }  

    }
    
    free(deserializado);  
}