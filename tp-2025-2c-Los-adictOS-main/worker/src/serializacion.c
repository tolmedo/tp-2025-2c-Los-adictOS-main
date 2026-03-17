#include "serializacion.h"
#include <utils/serializacion.h>
#include <commons/log.h>
#include <string.h>

extern t_log* logger;

t_paquete* serializar_handshake(int id_worker) {
    t_paquete* paquete = crear_paquete(HANDSHAKE_WORKER);
    agregar_a_paquete(paquete, &id_worker, sizeof(int));
    return paquete;
}

t_paquete* serializar_operacion_create(char* nombre_file, char* tag, int query_id) {
    t_paquete* paquete = crear_paquete(CREATE_FILE);
    agregar_a_paquete(paquete, &query_id, sizeof(int));
    agregar_a_paquete(paquete, nombre_file, strlen(nombre_file) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    return paquete;
}

t_paquete* serializar_operacion_truncate(char* nombre_file, char* tag, int tamano, int query_id) {
    t_paquete* paquete = crear_paquete(TRUNCATE_FILE);
    agregar_a_paquete(paquete, &query_id, sizeof(int));
    agregar_a_paquete(paquete, nombre_file, strlen(nombre_file) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, &tamano, sizeof(int));
    return paquete;
}

t_paquete* serializar_operacion_write_block(char* nombre_file, char* tag, int num_bloque, void* datos, int tamano, int query_id) {
    t_paquete* paquete = crear_paquete(WRITE_BLOCK);
    agregar_a_paquete(paquete, &query_id, sizeof(int));
    agregar_a_paquete(paquete, nombre_file, strlen(nombre_file) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, &num_bloque, sizeof(int));
    agregar_a_paquete(paquete, &tamano, sizeof(int));

    agregar_a_paquete(paquete, datos, tamano);

    return paquete;
}

t_paquete* serializar_operacion_read_block(char* nombre_file, char* tag, int num_bloque, int query_id) {
    t_paquete* paquete = crear_paquete(READ_BLOCK);
    agregar_a_paquete(paquete, &query_id, sizeof(int));
    agregar_a_paquete(paquete, nombre_file, strlen(nombre_file) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, &num_bloque, sizeof(int));
    return paquete;
}

t_paquete* serializar_operacion_tag(char* nombre_file_origen, char* tag_origen, char* nombre_file_destino, char* tag_destino, int query_id) {
    t_paquete* paquete = crear_paquete(TAG_FILE);
    agregar_a_paquete(paquete, &query_id, sizeof(int));
    agregar_a_paquete(paquete, nombre_file_origen, strlen(nombre_file_origen) + 1);
    agregar_a_paquete(paquete, tag_origen, strlen(tag_origen) + 1);
    agregar_a_paquete(paquete, nombre_file_destino, strlen(nombre_file_destino) + 1);
    agregar_a_paquete(paquete, tag_destino, strlen(tag_destino) + 1);
    return paquete;
}

t_paquete* serializar_operacion_commit(char* nombre_file, char* tag, int query_id) {
    t_paquete* paquete = crear_paquete(COMMIT_TAG);
    agregar_a_paquete(paquete, &query_id, sizeof(int));
    agregar_a_paquete(paquete, nombre_file, strlen(nombre_file) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    return paquete;
}

t_paquete* serializar_operacion_delete(char* nombre_file, char* tag, int query_id) {
    t_paquete* paquete = crear_paquete(REMOVE_TAG);
    agregar_a_paquete(paquete, &query_id, sizeof(int));
    agregar_a_paquete(paquete, nombre_file, strlen(nombre_file) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    return paquete;
}

t_paquete* serializar_fin_query(int query_id, char* motivo){
    t_paquete* paquete = crear_paquete(QUERY_FINALIZADA);
    agregar_a_paquete(paquete, &query_id, sizeof(int));
    agregar_a_paquete(paquete, motivo, strlen(motivo) + 1);

    return paquete;
}