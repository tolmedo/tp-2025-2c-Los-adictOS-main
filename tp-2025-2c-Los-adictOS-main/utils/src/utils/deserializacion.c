#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>

#include "deserializacion.h" 
#include "estructuras.h"

t_header obtener_header(int fd){
    int header;
    int bytes_recv = recv(fd, &header, sizeof(int), MSG_WAITALL);
    
    if (bytes_recv == -1) return -1; // Error
    if (bytes_recv == 0)  return -1; // Desconexion del otro lado
    if (bytes_recv < sizeof(int)) return -1; // el buffer era mas chico
    
    return header;
}

void* obtener_payload(int fd){
    int size_payload;
    int bytes_recv = recv(fd, &size_payload, sizeof(int), 0);

    if (bytes_recv == -1) return NULL; // Error
    if (bytes_recv == 0)  return NULL; // Desconexion del otro lado
    if (bytes_recv < sizeof(int)) return NULL; // El buffer era mas chico
    if (size_payload == 0) return NULL; // No hay payload
    
    void* payload = malloc(size_payload);
    bytes_recv = recv(fd, payload, size_payload, MSG_WAITALL);

    if (bytes_recv == -1) return NULL; // Error
    if (bytes_recv == 0)  return NULL; // Desconexion del otro lado
    if (bytes_recv < size_payload) return NULL; // El buffer era mas chico
    
    return payload;
}

void destruir_payload(void* payload){
    if (!payload) {
        return;
    }

    free(payload);
}

int obtener_size(void* payload, int* ofsset){
    int size;
    
    memcpy(&size, payload + *ofsset, sizeof(int));
    *ofsset += sizeof(int);
    
    return size;
}

int obtener_un_entero(void* payload, int* offset) {
    int valor = 0;
    int size = obtener_size(payload, offset);

    if (size != sizeof(int)) {
        // tamaño inesperado => abortar de forma segura
        // (podés loguear o setear errno si lo preferís)
        return -1;
    }

    memcpy(&valor, (char*)payload + *offset, sizeof(int));
    *offset += sizeof(int);

    return valor;
}

char* obtener_un_string(void* payload, int* offset) {
    int size = obtener_size(payload, offset);
    char* s = malloc(size + 1);
    memcpy(s, (char*)payload + *offset, size);
    s[size] = '\0';
    *offset += size;
    return s;
}


void* obtener_un_void(void* payload, int* offset){
    void* datos;
    int size;

    size = obtener_size(payload, offset);

    datos = malloc(size);
    memcpy(datos, payload + *offset, size);
    *offset += size;

    return datos;
}
