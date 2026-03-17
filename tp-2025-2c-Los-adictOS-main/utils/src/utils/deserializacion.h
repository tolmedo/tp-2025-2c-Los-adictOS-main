#ifndef DESERIALIZACION_UTILS
#define DESERIALIZACION_UTILS

#include "estructuras.h" 

t_header obtener_header(int fd);
void* obtener_payload(int fd);
void destruir_payload(void* payload);
int obtener_size(void* payload, int* ofsset);
int obtener_un_entero(void* payload, int* offset);
char* obtener_un_string(void *payload, int* offset);
void* obtener_un_void(void* payload, int* offset);

#endif