#include <stdlib.h>
#include <string.h>

#include <utils/estructuras.h>
#include <utils/deserializacion.h>

#include "deserializacion.h"

static t_deserializado_query_control* _deserializar_qc_lectura(void* payload) {
    t_deserializado_query_control* deserializado = malloc(sizeof(*deserializado));
    int offset = 0;
    
    deserializado->header = QC_LECTURA;
    deserializado->payload.qc_lectura.nombre_file = obtener_un_string(payload, &offset);
    deserializado->payload.qc_lectura.tag = obtener_un_string(payload, &offset);
    deserializado->payload.qc_lectura.contenido = obtener_un_string(payload, &offset);
   
    return deserializado;
}

static t_deserializado_query_control* _deserializar_qc_finalizacion(void* payload) {
    t_deserializado_query_control* deserializado = malloc(sizeof(*deserializado));
    int offset = 0;
    
    deserializado->header = QC_FINALIZACION;
    deserializado->payload.qc_finalizacion.motivo = obtener_un_string(payload, &offset);
    
    return deserializado;
}

t_deserializado_query_control* deserializar_mensaje_query_control(t_header header, void* payload) {
    t_deserializado_query_control* deserializado = NULL;

    switch (header) {
        case QC_FINALIZACION:
            deserializado = _deserializar_qc_finalizacion(payload);
            break;
            
        case QC_LECTURA:
            deserializado = _deserializar_qc_lectura(payload);
            break;
            
        default:
            break;
    }

    return deserializado;
}
void destruir_deserializado_query_control(t_deserializado_query_control* deserializado) {
    if (!deserializado) return;

    switch (deserializado->header) {
        case QC_LECTURA:
            free(deserializado->payload.qc_lectura.nombre_file);
            free(deserializado->payload.qc_lectura.tag);
            free(deserializado->payload.qc_lectura.contenido);
            break;
            
        case QC_FINALIZACION:
            free(deserializado->payload.qc_finalizacion.motivo);
            break;
            
        default:
            break;
    }
    
    free(deserializado);
}
