#include <stdlib.h>

#include <utils/estructuras.h>
#include <utils/deserializacion.h>

#include "deserializacion.h"


//======================== DESERIALIZACION DE OPERACIONES ========================
static t_deserializado* deserializar_create_file(void* payload){
    t_deserializado* deserializado = malloc(sizeof(*deserializado));
    int offset = 0;

    deserializado->header = CREATE_FILE;
    deserializado->payload.crear_file.query_id = obtener_un_entero(payload, &offset);
    deserializado->payload.crear_file.nombre_file = obtener_un_string(payload, &offset);
    deserializado->payload.crear_file.tag = obtener_un_string(payload, &offset);

    return deserializado;
}

static t_deserializado* deserializar_truncate_file(void* payload){
    t_deserializado* deserializado = malloc(sizeof(*deserializado));
    int offset = 0;

    deserializado->header = TRUNCATE_FILE;
    deserializado->payload.truncar_archivo.query_id = obtener_un_entero(payload, &offset);
    deserializado->payload.truncar_archivo.nombre_file = obtener_un_string(payload, &offset);
    deserializado->payload.truncar_archivo.tag = obtener_un_string(payload, &offset);
    deserializado->payload.truncar_archivo.tamanio = obtener_un_entero(payload, &offset);
    

    return deserializado;
}

static t_deserializado* deserializar_write_block(void* payload){
    t_deserializado* deserializado = malloc(sizeof(*deserializado));
    int offset = 0;
    
    deserializado->header = WRITE_BLOCK;
    deserializado->payload.escribir_bloque.query_id = obtener_un_entero(payload, &offset);
    deserializado->payload.escribir_bloque.nombre_file = obtener_un_string(payload, &offset);
    deserializado->payload.escribir_bloque.tag = obtener_un_string(payload, &offset);
    deserializado->payload.escribir_bloque.bloque_logico = obtener_un_entero(payload, &offset);
    deserializado->payload.escribir_bloque.tamanio = obtener_un_entero(payload, &offset);
    deserializado->payload.escribir_bloque.contenido = obtener_un_void(payload, &offset);

    return deserializado;
}

static t_deserializado* deserializar_read_block(void* payload){
    t_deserializado* deserializado = malloc(sizeof(*deserializado));
    int offset = 0;
    
    deserializado->header = READ_BLOCK;
    deserializado->payload.leer_bloque.query_id = obtener_un_entero(payload, &offset);
    deserializado->payload.leer_bloque.nombre_file = obtener_un_string(payload, &offset);
    deserializado->payload.leer_bloque.tag = obtener_un_string(payload, &offset);
    deserializado->payload.leer_bloque.bloque_logico = obtener_un_entero(payload, &offset);
    
    return deserializado;
}
static t_deserializado* deserializar_remove_tag(void* payload){
    t_deserializado* deserializado = malloc(sizeof(*deserializado));
    int offset = 0;
    
    deserializado->header = REMOVE_TAG;
    deserializado->payload.eliminar_tag.query_id = obtener_un_entero(payload, &offset);
    deserializado->payload.eliminar_tag.nombre_file = obtener_un_string(payload, &offset);
    deserializado->payload.eliminar_tag.tag = obtener_un_string(payload, &offset);
    
    return deserializado;
}
static t_deserializado* deserializar_commit_tag(void* payload){
    t_deserializado* deserializado = malloc(sizeof(*deserializado));
    int offset = 0;

    deserializado->header = COMMIT_TAG;
    deserializado->payload.confirmar_tag.query_id = obtener_un_entero(payload, &offset);
    deserializado->payload.confirmar_tag.nombre_file = obtener_un_string(payload, &offset);
    deserializado->payload.confirmar_tag.tag = obtener_un_string(payload, &offset);

    return deserializado;
}
static t_deserializado* deserializar_tag_file(void* payload){
    t_deserializado* deserializado = malloc(sizeof(*deserializado));
    int offset = 0;
    
    deserializado->header = TAG_FILE;
    deserializado->payload.crear_copia.query_id = obtener_un_entero(payload, &offset);
    deserializado->payload.crear_copia.nombre_file_origen = obtener_un_string(payload, &offset);
    deserializado->payload.crear_copia.tag_origen = obtener_un_string(payload, &offset);
    deserializado->payload.crear_copia.nombre_file_destino = obtener_un_string(payload, &offset);
    deserializado->payload.crear_copia.tag_destino = obtener_un_string(payload, &offset);
    
    return deserializado;
}


//======================== DESERIALIZACION DE HANDSHAKE ========================
static t_deserializado* _deserializar_handshake_worker(void* payload){
    t_deserializado* d = malloc(sizeof(*d));
    int offset = 0;
    d->header = HANDSHAKE_WORKER;
    d->payload.handshake_worker.worker_id = obtener_un_entero(payload, &offset);
    return d;
}


t_deserializado* deserializar(t_header header, void* payload){ // Faltan deserializar mas .... Si el mensaje tiene argumentos agregarlos a t_deserializado 
    t_deserializado* deserializado = NULL;

    switch (header) {
         case HANDSHAKE_WORKER: {
           deserializado = _deserializar_handshake_worker(payload);
           break;
        }

        case CREATE_FILE: {
            deserializado = deserializar_create_file(payload);
            break;
        }

        case TRUNCATE_FILE: {
            deserializado = deserializar_truncate_file(payload);
            break;
        }

        case WRITE_BLOCK: {
            deserializado = deserializar_write_block(payload);
            break;
        }

        case READ_BLOCK: {
            deserializado = deserializar_read_block(payload);
            break;
        }
        
        case REMOVE_TAG: {
            deserializado = deserializar_remove_tag(payload);
            break;
        }
        
        case COMMIT_TAG: {
            deserializado = deserializar_commit_tag(payload);
            break;
        }
        
        case TAG_FILE: {
            deserializado = deserializar_tag_file(payload);
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

        case TRUNCATE_FILE: {
            free(deserializado->payload.truncar_archivo.nombre_file);
            free(deserializado->payload.truncar_archivo.tag);
            break;
        }

        case WRITE_BLOCK: {
            free(deserializado->payload.escribir_bloque.nombre_file);
            free(deserializado->payload.escribir_bloque.tag);
            free(deserializado->payload.escribir_bloque.contenido);
            break;
        }

        case READ_BLOCK: {
            free(deserializado->payload.leer_bloque.nombre_file);
            free(deserializado->payload.leer_bloque.tag);
            break;
        }

        case COMMIT_TAG: {
            free(deserializado->payload.confirmar_tag.nombre_file);
            free(deserializado->payload.confirmar_tag.tag);
            break;
        }

        case REMOVE_TAG: {
            free(deserializado->payload.eliminar_tag.nombre_file);
            free(deserializado->payload.eliminar_tag.tag);
            break;
        }

        case TAG_FILE: {
            free(deserializado->payload.crear_copia.nombre_file_origen);
            free(deserializado->payload.crear_copia.tag_origen);
            free(deserializado->payload.crear_copia.nombre_file_destino);
            free(deserializado->payload.crear_copia.tag_destino);
            break;
        }

        default: {


            break;
        }  

    }
    
    free(deserializado);  
}