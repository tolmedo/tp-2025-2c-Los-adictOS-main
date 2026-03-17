#ifndef DESERIALIZACION_QUERY_CONTROL_H
#define DESERIALIZACION_QUERY_CONTROL_H
#include "utils/estructuras.h"
typedef struct{
    t_header header;
    union{
        struct {char* motivo; } qc_finalizacion;
        struct {char* nombre_file; char* tag; int tamanio; char* contenido; } qc_lectura;
    } payload;
}t_deserializado_query_control;

t_deserializado_query_control* deserializar_mensaje_query_control(t_header header, void* payload);
void destruir_deserializado_query_control(t_deserializado_query_control* deserializado);

//const char* qc_motivo_str(t_motivo_de_finalizacion_de_query_control motivo);

#endif