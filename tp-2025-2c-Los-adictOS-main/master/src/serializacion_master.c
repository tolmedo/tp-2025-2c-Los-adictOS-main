#include <stdlib.h>
#include <string.h>
#include <stdint.h>  

#include "commons/log.h"
#include "commons/string.h"

#include <utils/estructuras.h>
#include <utils/serializacion.h>

#include "serializacion_master.h"

extern t_log* logger;

t_paquete* serializar_respuesta_handshake_worker(){
	t_paquete* paquete = crear_paquete(HANDSHAKE_OK);
	return paquete;
}

t_paquete* serializar_respuesta_handshake_query_control(){
	t_paquete* paquete = crear_paquete(HANDSHAKE_OK);
    log_debug(logger, "MANDO HEADER %d", HANDSHAKE_OK);
	return paquete;
}

t_paquete* serializar_respuesta_handshake_desconocido(){
	t_paquete* paquete = crear_paquete(HANDSHAKE_ERROR);
	return paquete;
}

t_paquete* serializar_comando_ejecutar_query(int query_id, char* path_query, int program_counter) {
    t_paquete* paquete = crear_paquete(EJECUTAR_QUERY);

    agregar_a_paquete(paquete, path_query, strlen(path_query) + 1);
    agregar_a_paquete(paquete, &query_id, sizeof(int));
    agregar_a_paquete(paquete, &program_counter, sizeof(int));
    
    return paquete;
}

t_paquete* serializar_respuesta_create_file() {
    t_paquete* paquete = crear_paquete(RESPUESTA_CREATE_FILE);
    return paquete;
}

t_paquete* serializar_mensaje_lectura(char* nombre_file, char* tag, char* contenido) {
    t_paquete* paquete = crear_paquete(QC_LECTURA);
    agregar_a_paquete(paquete, nombre_file, strlen(nombre_file) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    agregar_a_paquete(paquete, contenido, strlen(contenido) + 1);
    
    return paquete;
}

t_paquete* serializar_mensaje_finalizacion(char* motivo) {
    t_paquete* paquete = crear_paquete(QC_FINALIZACION);
    agregar_a_paquete(paquete, motivo, strlen(motivo) + 1);
    
    return paquete;
}