#ifndef TABLA_DE_PAGINAS_H
#define TABLA_DE_PAGINAS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "commons/collections/list.h"
#include "commons/log.h"

typedef struct {
    int numero_pagina;
    int marco;
    int presente;
    int modificado;
    int uso;
    uint64_t ultimo_acceso;
} t_pagina;

typedef struct {
    char* nombre_file;
    char* tag;
    t_list* paginas;
} t_tabla_de_paginas;

typedef struct {
    void* memoria;
    int tamanio_total;
    int tamanio_bloque;
    int cantidad_marcos;
    bool* marcos_ocupados;
    char* algoritmo_reemplazo;
    int puntero_clock;
} memoria_interna_t;

// Funciones principales
memoria_interna_t* inicializar_memoria_interna(int tamanio_total, int tamanio_bloque);
t_tabla_de_paginas* buscar_tabla(memoria_interna_t* memoria, char* nombre_file, char* tag);
t_pagina* buscar_pagina(memoria_interna_t* memoria, t_tabla_de_paginas* tabla, int num_pagina, int query_id, int fd_storage, int offset_pag, char* nombre_file, char* nombre_tag);
void flush_paginas_modificadas(char* nombre_file, char* tag, int query_id, int fd_storage);

// Funciones de reemplazo
int obtener_marco_libre(memoria_interna_t* memoria);
int reemplazar_pagina_LRU(memoria_interna_t* memoria, int query_id, int fd_storage);
int reemplazar_pagina_CLOCK_M(memoria_interna_t* memoria, int query_id, int fd_storage);

#endif