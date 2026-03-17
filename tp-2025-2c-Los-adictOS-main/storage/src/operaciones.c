#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <commons/log.h>
#include <commons/string.h>

#include <utils/conexiones.h>
#include <utils/serializacion.h>
#include <utils/estructuras.h>

#include "filesystem.h"
#include "config.h"
#include "serializacion.h"
#include "operaciones.h"
#include "superblock.h"

extern t_log* logger;
extern t_config_storage* config_storage;
extern t_filesystem* filesystem;
extern int error_read;

void aplicar_retardo_operacion(){
    int retardo = obtener_retardo_operacion();
    usleep(retardo*1000);
}

void aplicar_retardo_acceso_bloque(){
    int retardo = obtener_retardo_acceso_bloque();
    usleep(retardo*1000);
}

// ========================== OPERACIÓN CREATE FILE ==========================

void create_file(int socket_worker, char* nombre_file, char* nombre_tag, int query_id){
    log_debug(logger, "CREATE FILE - File: %s, Tag: %s", nombre_file, nombre_tag);

    aplicar_retardo_operacion();

    bool resultado = crear_file(filesystem, nombre_file, nombre_tag);

    if (resultado){
        log_info(logger, "##%d - File Creado %s:%s", query_id, nombre_file, nombre_tag);
        t_paquete* respuesta = serializar_respuesta_create_file();
        enviar_paquete(respuesta, socket_worker);
    }
    else{
        char* motivo = "File:Tag preexistente";
        t_paquete* paquete = serializar_error_operacion(motivo);
        enviar_paquete(paquete, socket_worker);

    }
}

// ========================== OPERACIÓN TRUNCATE FILE ==========================

void truncate_file(int socket_worker, char* nombre_file, char* nombre_tag, int nuevo_tamanio, int query_id){
    log_debug(logger,"TRUNCATE FILE - File: %s, Tag: %s, Tamaño: %d", nombre_file, nombre_tag, nuevo_tamanio);

    aplicar_retardo_operacion();

    int resultado = truncar_file(filesystem, nombre_file, nombre_tag, nuevo_tamanio, query_id);

    if (resultado == 1){
        log_info(logger, "##%d - File Truncado %s:%s - Tamaño: %d", query_id, nombre_file, nombre_tag, nuevo_tamanio);
        t_paquete* respuesta = serializar_respuesta_truncate_file();
        //enviar_paquete(respuesta, socket_worker);
        if (!enviar_paquete(respuesta, socket_worker)) {
            log_error(logger, "Error enviando respuesta TRUNCATE a Worker");
        }
    } else if(resultado == 2){
        char* motivo = "Escritura no permitida";
        t_paquete* paquete = serializar_error_operacion(motivo);
        enviar_paquete(paquete, socket_worker);
    }else if (resultado == 3){
        char* motivo = "Espacio insuficiente";
        t_paquete* paquete = serializar_error_operacion(motivo);
        enviar_paquete(paquete, socket_worker);
    } else if (resultado == 4){
        char* motivo = "File:Tag inexistente";
        log_debug(logger,"%s", motivo);
        t_paquete* paquete = serializar_error_operacion(motivo);
        enviar_paquete(paquete, socket_worker);
    } else {
        char* motivo = "Ocurrió un error durante la operación TRUNCATE";
        t_paquete*paquete = serializar_error_operacion(motivo);
        enviar_paquete(paquete, socket_worker);
    }

}

// ========================== OPERACIÓN WRITE BLOCK ==========================

void write_block(int socket_worker, char* nombre_file, char* nombre_tag, int bloque_logico, void* contenido, int query_id, int tamanio){
    log_debug(logger,"WRITE BLOCK - File: %s, Tag: %s, Bloque Lógico: %d", nombre_file, nombre_tag, bloque_logico);

    aplicar_retardo_operacion();
    aplicar_retardo_acceso_bloque();

    int resultado = escribir_bloque(filesystem, nombre_file, nombre_tag, bloque_logico, contenido, tamanio);

    if(resultado == 1){
        log_info(logger, "##%d - Bloque Lógico Escrito %s:%s - Número de Bloque: %d", query_id, nombre_file, nombre_tag, bloque_logico);
        t_paquete* respuesta = serializar_respuesta_write_block();
        enviar_paquete(respuesta, socket_worker);
    } else if(resultado == 2){
        char* motivo = "Escritura fuera de límite";
        t_paquete*paquete = serializar_error_operacion(motivo);
        enviar_paquete(paquete, socket_worker);
    } else if(resultado == 3){
        char* motivo = "Escritura no permitida por COMMITED";
        t_paquete*paquete = serializar_error_operacion(motivo);
        enviar_paquete(paquete, socket_worker);
    }else if(resultado == 4){
        char* motivo = "Espacio insuficiente";
        t_paquete*paquete = serializar_error_operacion(motivo);
        enviar_paquete(paquete, socket_worker);
    } else {
        char* motivo = "Ocurrió un error durante la escritura";
        t_paquete*paquete = serializar_error_operacion(motivo);
        enviar_paquete(paquete, socket_worker);
    }
}

// ========================== OPERACIÓN READ BLOCK ==========================

void read_block(int socket_worker, char* nombre_file, char* nombre_tag, int bloque_logico, int query_id){
    log_debug(logger,"READ FILE - File: %s, Tag: %s, Bloque: %d", nombre_file, nombre_tag, bloque_logico);

    aplicar_retardo_operacion();
    aplicar_retardo_acceso_bloque(); 

    error_read = 0;
    void* contenido = leer_bloque(filesystem, nombre_file, nombre_tag, bloque_logico);
    if (contenido == NULL){
        if(error_read == 1){
            char* motivo = "Ocurrio un error durante la lectura";
            t_paquete*paquete = serializar_error_operacion(motivo);
            enviar_paquete(paquete, socket_worker);
            return;
        }else if (error_read == 2){
            char* motivo = "FILE:TAG inexistente";
            t_paquete*paquete = serializar_error_operacion(motivo);
            enviar_paquete(paquete, socket_worker);
            return;
        }else if (error_read == 3){
            char* motivo = "Lectura fuera de limite";
            t_paquete*paquete = serializar_error_operacion(motivo);
            enviar_paquete(paquete, socket_worker);
            return;
        }
    }
    int resultado = 1;
    int tamanio_leido = obtener_block_size();
   
    t_paquete* respuesta = serializar_respuesta_read_block(resultado, contenido, tamanio_leido);
    enviar_paquete(respuesta, socket_worker);

    log_info(logger, "##%d - Bloque Lógico Leído %s:%s - Número de Bloque: %d", query_id, nombre_file, nombre_tag, bloque_logico);

    if (contenido) {
        free(contenido);
    }
}
// ========================== OPERACIÓN REMOVE TAG ==========================

void remove_tag(int socket_worker, char* nombre_file, char* nombre_tag, int query_id){
    log_debug(logger,"REMOVE TAG - File: %s, Tag: %s", nombre_file, nombre_tag);

    aplicar_retardo_operacion();

    int resultado = eliminar_file(filesystem,nombre_file, nombre_tag, query_id);

    if(resultado == 1){
        log_info(logger, "##%d - Tag Eliminado %s:%s", query_id, nombre_file, nombre_tag);
        t_paquete* respuesta = serializar_respuesta_remove_tag();
        enviar_paquete(respuesta, socket_worker);
    } else if(resultado == 2){
        char* motivo = "File:Tag inexistente";
        log_error(logger, "%s", motivo);
        t_paquete* paquete = serializar_error_operacion(motivo);
        enviar_paquete(paquete, socket_worker);
    } else{
        char* motivo = "Ocurrió un error durante la operación DELETE";
        t_paquete* paquete = serializar_error_operacion(motivo);
        enviar_paquete(paquete, socket_worker);
    }

}

// ========================== OPERACIÓN COMMIT TAG ==========================

void commit_tag(int socket_worker, char* nombre_file, char* nombre_tag, int query_id){
    log_debug(logger,"COMMIT TAG - File: %s, Tag: %s", nombre_file, nombre_tag);

    aplicar_retardo_operacion();

    bool resultado = commitear_tag(filesystem, nombre_file, nombre_tag, query_id);
    if (resultado){
        t_paquete* respuesta = serializar_respuesta_commit_tag();
        enviar_paquete(respuesta, socket_worker);
        log_info(logger, "##%d - Commit de File:Tag %s:%s", query_id, nombre_file, nombre_tag); // Log obligatorio
    } else {
        char* motivo = "Ocurrió un error durante la operación COMMIT";
        t_paquete* respuesta = serializar_error_operacion(motivo);
        enviar_paquete(respuesta, socket_worker);
    }

}

// ========================== OPERACIÓN TAG FILE ==========================

void tag_file(int socket_worker, char* nombre_file_origen, char* tag_origen, char* nombre_file_destino, char* tag_destino, int query_id){
    log_debug(logger,"TAG FILE - File Origen: %s, Tag Origen: %s, File Destino: %s, Tag Destino: %s", nombre_file_origen, tag_origen, nombre_file_destino, tag_destino);

    aplicar_retardo_operacion();

    int resultado = copiar_tag(filesystem, nombre_file_origen, tag_origen, nombre_file_destino, tag_destino, query_id);

    if(resultado == 1 ){
        log_info(logger, "##%d - Tag creado %s:%s", query_id, nombre_file_destino, tag_destino);
        t_paquete* respuesta = serializar_respuesta_tag_file();
        enviar_paquete(respuesta, socket_worker);
    } else if(resultado == 2){
        log_error(logger, "%s:%s Preexistente", nombre_file_destino, tag_destino);
        char* motivo = "File:Tag Preexistente";
        t_paquete* respuesta = serializar_error_operacion(motivo);
        enviar_paquete(respuesta, socket_worker);
    } else if (resultado == 3){
        log_error(logger, "%s:%s Inexistente",nombre_file_origen,tag_origen);
        char* motivo = "File:Tag (Origen) Inexistente";
        t_paquete* respuesta = serializar_error_operacion(motivo);
        enviar_paquete(respuesta,socket_worker);
    } else {
        char* motivo = "Ocurrió un error durante la operación TAG";
        t_paquete* respuesta = serializar_error_operacion(motivo);
        enviar_paquete(respuesta,socket_worker);
    }

}