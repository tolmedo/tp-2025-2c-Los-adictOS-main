#include "tabla_de_paginas.h"
#include "config.h"
#include "serializacion.h"
#include "worker.h"
#include "hilo-storage.h"

#include <utils/serializacion.h>
#include <utils/deserializacion.h>
#include <commons/string.h>
#include <string.h>
#include <semaphore.h>

extern t_log* logger;
extern t_list* tablas_de_paginas;
extern int tamanio_bloque;
extern int fd_master;
extern bool query_en_ejecucion;

extern pthread_mutex_t mutex_tablas_paginas;

extern sem_t sem_respuesta_storage;

static t_tabla_de_paginas* ultima_tabla_victima = NULL;
static t_pagina* ultima_pagina_victima = NULL;

static uint64_t contador_acceso_global = 0;
static pthread_mutex_t mutex_contador_lru = PTHREAD_MUTEX_INITIALIZER;

memoria_interna_t* inicializar_memoria_interna(int tamanio_total, int tamanio_bloque) {
    memoria_interna_t* memoria = malloc(sizeof(memoria_interna_t));

    memoria->memoria = malloc(tamanio_total);
    memoria->tamanio_total = tamanio_total;
    memoria->tamanio_bloque = tamanio_bloque;
    memoria->cantidad_marcos = tamanio_total / tamanio_bloque;
    memoria->marcos_ocupados = calloc(memoria->cantidad_marcos, sizeof(bool));
    memoria->algoritmo_reemplazo = obtener_algoritmo_reemplazo();
    memoria->puntero_clock = 0;

    log_debug(logger, "Memoria interna inicializada: %d bytes, %d marcos de %d bytes", tamanio_total, memoria->cantidad_marcos, tamanio_bloque);

    return memoria;
}

t_tabla_de_paginas* buscar_tabla(memoria_interna_t* memoria, char* nombre_file, char* tag) {
    pthread_mutex_lock(&mutex_tablas_paginas);
    
    if (!tablas_de_paginas) {
        log_error(logger, "tablas_de_paginas no inicializada");
        pthread_mutex_unlock(&mutex_tablas_paginas);
        return NULL;
    }

    // Buscar tabla existente
    for (int i = 0; i < list_size(tablas_de_paginas); i++) {
        t_tabla_de_paginas* tabla = list_get(tablas_de_paginas, i);
        if (strcmp(tabla->nombre_file, nombre_file) == 0 && strcmp(tabla->tag, tag) == 0) {
            pthread_mutex_unlock(&mutex_tablas_paginas);
            return tabla;
        }
    }

    // Si no existe, crear nueva tabla
    t_tabla_de_paginas* nueva_tabla = malloc(sizeof(t_tabla_de_paginas));
    nueva_tabla->nombre_file = strdup(nombre_file);
    nueva_tabla->tag = strdup(tag);
    nueva_tabla->paginas = list_create();
    list_add(tablas_de_paginas, nueva_tabla);

    pthread_mutex_unlock(&mutex_tablas_paginas);

    log_debug(logger, "Tabla de páginas creada para %s:%s", nombre_file, tag);

    return nueva_tabla;
}

static uint64_t obtener_timestamp_lru() {
    pthread_mutex_lock(&mutex_contador_lru);
    uint64_t timestamp = ++contador_acceso_global;
    pthread_mutex_unlock(&mutex_contador_lru);
    return timestamp;
}

t_pagina* buscar_pagina(memoria_interna_t* memoria, t_tabla_de_paginas* tabla, int num_pagina, int query_id, int fd_storage, int offset_pag, char* nombre_file, char* nombre_tag) {
    // Buscar si la página ya está en memoria
    for (int i = 0; i < list_size(tabla->paginas); i++) {
        t_pagina* pag = list_get(tabla->paginas, i);

        if (pag->numero_pagina == num_pagina && pag->presente) {
            pag->ultimo_acceso = obtener_timestamp_lru();
            pag->uso = 1;

            return pag;
        }
    }

    // Memoria Miss
    log_info(logger, "Query %d: - Memoria Miss - File: %s - Tag: %s - Pagina: %d", query_id, tabla->nombre_file, tabla->tag, num_pagina);
    // Cargar página desde Storage
    t_paquete* paquete_read = serializar_operacion_read_block(tabla->nombre_file, tabla->tag, num_pagina, query_id);
    enviar_paquete(paquete_read, fd_storage);
    esperar_respuesta_storage(fd_storage, query_id, fd_master);
    sem_wait(&sem_respuesta_storage);
    if (!query_en_ejecucion) {
        // La query ya fue finalizada y avisada al Master

        return NULL;
    }
    void* payload = obtener_payload(fd_storage);
    if (!payload) {
        log_debug(logger, "[DEBUG] buscar_pagina: Payload es NULL!");
        return NULL;
    }
         
    int offset = 0;
    int resultado = obtener_un_entero(payload, &offset);
    log_debug(logger, "[DEBUG] resultado es: %d",resultado);
    if (!resultado) {
        log_debug(logger, "Storage falló al leer bloque %d de %s:%s", num_pagina, tabla->nombre_file, tabla->tag);
        destruir_payload(payload);
        return NULL;
    }

    // Leer int tamanio_leido
    int tamano_leido = obtener_un_entero(payload, &offset);
    if (tamano_leido <= 0 || tamano_leido > tamanio_bloque * 2) {
        destruir_payload(payload);
        return NULL;
    }

    void* datos_bloque = obtener_un_void(payload, &offset);  
    if (!datos_bloque) {
        destruir_payload(payload);
        return NULL;
    }
    int marco_asignado = obtener_marco_libre(memoria);

    if(marco_asignado != -1) log_info(logger, "Query %d: Se asigna el Marco: %d a la Página: %d perteneciente al - File: %s - Tag: %s", query_id, marco_asignado, num_pagina, tabla->nombre_file, tabla->tag);
    
    if (marco_asignado == -1) {
        if (strcmp(memoria->algoritmo_reemplazo, "CLOCK-M") == 0) {
            marco_asignado = reemplazar_pagina_CLOCK_M(memoria, query_id, fd_storage);
        } else {
            marco_asignado = reemplazar_pagina_LRU(memoria, query_id, fd_storage);
        }
    }
    
    if (marco_asignado != -1) {
    // Si el marco vino de un reemplazo, las variables globales apuntan a la víctima
   
    if (ultima_pagina_victima != NULL && ultima_tabla_victima != NULL) {
        log_info(logger,
            "## Query %d: Se reemplaza la página %s:%s/%d por la %s:%s/%d",query_id,ultima_tabla_victima->nombre_file,ultima_tabla_victima->tag,ultima_pagina_victima->numero_pagina,
            nombre_file,
            nombre_tag,
            num_pagina
        );
    
        // Limpiamos para no reutilizar esta info en futuros accesos
        ultima_tabla_victima = NULL;
        ultima_pagina_victima = NULL;
    }
    } else {
        log_error(logger, "No se pudo obtener marco para Query %d", query_id);
        free(datos_bloque);
        destruir_payload(payload);
        return NULL;
    }
    // Copiar a memoria interna
    void* destino = memoria->memoria + (marco_asignado * tamanio_bloque) + offset_pag;
    memcpy(destino, datos_bloque, tamano_leido);

    free(datos_bloque);
    destruir_payload(payload);

    // Crear o actualizar página
    t_pagina* pagina = NULL;
    for (int i = 0; i < list_size(tabla->paginas); i++) {
        t_pagina* pag = list_get(tabla->paginas, i);
        if (pag->numero_pagina == num_pagina) {
            pagina = pag;
            break;
        }
    }

    if (!pagina) {
        pagina = malloc(sizeof(t_pagina));
        pagina->numero_pagina = num_pagina;
        list_add(tabla->paginas, pagina);
    }

    pagina->marco = marco_asignado;
    pagina->presente = 1;
    pagina->modificado = 0;
    pagina->uso = 1;
    pagina->ultimo_acceso = obtener_timestamp_lru();
    memoria->marcos_ocupados[marco_asignado] = true;

    log_info(logger, "Query %d: - Memoria Add - File: %s - Tag: %s - Pagina: %d - Marco: %d", query_id, tabla->nombre_file, tabla->tag, num_pagina, marco_asignado);

    return pagina;
}

int obtener_marco_libre(memoria_interna_t* memoria) {
    for (int i = 0; i < memoria->cantidad_marcos; i++) {
        if (!memoria->marcos_ocupados[i]) {
            return i;
        }
    }
    return -1;
}

int reemplazar_pagina_LRU(memoria_interna_t* memoria, int query_id, int fd_storage) {
    time_t menor_tiempo = 0;
    int marco_victima = -1;
    t_pagina* pagina_victima = NULL;
    t_tabla_de_paginas* tabla_victima = NULL;

    for (int i = 0; i < list_size(tablas_de_paginas); i++) {
        t_tabla_de_paginas* tabla = list_get(tablas_de_paginas, i);

        for (int j = 0; j < list_size(tabla->paginas); j++) {
            t_pagina* pag = list_get(tabla->paginas, j);

            if (pag->presente && (pagina_victima == NULL || pag->ultimo_acceso < menor_tiempo)) {
                menor_tiempo = pag->ultimo_acceso;
                marco_victima = pag->marco;
                pagina_victima = pag;
                tabla_victima = tabla;
            }
        }
    }

    if (marco_victima == -1) {
        log_error(logger, "No se encontró víctima para LRU");
        return -1;
    }

    if (pagina_victima->modificado) {
        void* datos = memoria->memoria + (marco_victima * tamanio_bloque);
        t_paquete* paquete_write = serializar_operacion_write_block(tabla_victima->nombre_file, tabla_victima->tag, pagina_victima->numero_pagina, datos, tamanio_bloque, query_id);
        enviar_paquete(paquete_write, fd_storage);
        esperar_respuesta_storage(fd_storage, query_id, fd_master);

        sem_wait(&sem_respuesta_storage);
    }

    pagina_victima->presente = 0;
    memoria->marcos_ocupados[marco_victima] = false;
    ultima_tabla_victima = tabla_victima;
    ultima_pagina_victima = pagina_victima;

    return marco_victima;
}

int primer_vuelta_clock(memoria_interna_t* memoria, int query_id){
    int total_marcos = memoria->cantidad_marcos;
    int marco_inicial = memoria->puntero_clock;
    
    // Recorrer TODOS los marcos
    for (int vueltas = 0; vueltas < total_marcos; vueltas++) {
        int marco_actual = (marco_inicial + vueltas) % total_marcos;
        t_pagina* pagina_actual = NULL;
        
        // Buscar qué página está en este marco
        for (int i = 0; i < list_size(tablas_de_paginas); i++) {
            t_tabla_de_paginas* tabla = list_get(tablas_de_paginas, i);
            
            for (int j = 0; j < list_size(tabla->paginas); j++) {
                t_pagina* pag = list_get(tabla->paginas, j);
                
                if (pag->presente && pag->marco == marco_actual) {
                    pagina_actual = pag;
                    break;
                }
            }
            if (pagina_actual){
                if (pagina_actual->uso == 0 && pagina_actual->modificado == 0){
                
                    ultima_tabla_victima = tabla;
                    ultima_pagina_victima = pagina_actual;
                    pagina_actual->presente = 0;
                    memoria->marcos_ocupados[marco_actual] = false;
                    memoria->puntero_clock = (marco_actual + 1) % total_marcos;
                    return marco_actual;
                }
            }
        }
    }
    
    return -1;
}

int segunda_vuelta_clock(memoria_interna_t* memoria, int query_id, int fd_storage){
    int total_marcos = memoria->cantidad_marcos;
    int marco_inicial = memoria->puntero_clock;
    
    // Recorrer TODOS los marcos
    for (int vueltas = 0; vueltas < total_marcos; vueltas++) {
        int marco_actual = (marco_inicial + vueltas) % total_marcos;
        t_pagina* pagina_actual = NULL;
        t_tabla_de_paginas* tabla_actual = NULL;
        
        // Buscar qué página está en este marco
        for (int i = 0; i < list_size(tablas_de_paginas); i++) {
            t_tabla_de_paginas* tabla = list_get(tablas_de_paginas, i);
            
            for (int j = 0; j < list_size(tabla->paginas); j++) {
                t_pagina* pag = list_get(tabla->paginas, j);
                
                if (pag->presente && pag->marco == marco_actual) {
                    pagina_actual = pag;
                    tabla_actual = tabla;
                    break;
                }
            }
            if (pagina_actual) break;
        }
        
        if (pagina_actual) {
            // Buscar víctima: uso=0 y modificado=1
            if (pagina_actual->uso == 0 && pagina_actual->modificado == 1) {
                ultima_tabla_victima = tabla_actual;
                ultima_pagina_victima = pagina_actual;
                // Escribir a storage antes de reemplazar
                void* datos = memoria->memoria + (marco_actual * tamanio_bloque);
                t_paquete* paquete_write = serializar_operacion_write_block(tabla_actual->nombre_file, tabla_actual->tag, pagina_actual->numero_pagina, datos, tamanio_bloque, query_id);
                enviar_paquete(paquete_write, fd_storage);
                esperar_respuesta_storage(fd_storage, query_id, fd_master);
                sem_wait(&sem_respuesta_storage);
                
                pagina_actual->presente = 0;
                pagina_actual->modificado = 0;
                memoria->marcos_ocupados[marco_actual] = false;
                memoria->puntero_clock = (marco_actual + 1) % total_marcos;
                return marco_actual;
            } else {
                pagina_actual->uso = 0;
            }
        }
    }
    
    return -1;
}

int reemplazar_pagina_CLOCK_M(memoria_interna_t* memoria, int query_id, int fd_storage) {
    int marco_victima = -1;

    // PRIMER VUELTA
    marco_victima = primer_vuelta_clock(memoria, query_id);
    if(marco_victima != -1) return marco_victima;

    // SEGUNDA VUELTA
    marco_victima = segunda_vuelta_clock(memoria, query_id, fd_storage);
    if(marco_victima != -1) return marco_victima;

    // TERCERA VUELTA
    marco_victima = primer_vuelta_clock(memoria, query_id);
    if(marco_victima != -1) return marco_victima;

    //ULTIMA VUELTA
    marco_victima = segunda_vuelta_clock(memoria, query_id, fd_storage);
    if(marco_victima != -1) return marco_victima;

    return marco_victima;
}

void flush_paginas_modificadas(char* nombre_file, char* tag, int query_id, int fd_storage) {
    t_tabla_de_paginas* tabla = buscar_tabla(NULL, nombre_file, tag);
    if (!tabla) {
        log_debug(logger, "No existe tabla para %s:%s en FLUSH", nombre_file, tag);
        return;
    }

    for (int i = 0; i < list_size(tabla->paginas); i++) {
        t_pagina* pag = list_get(tabla->paginas, i);

        if (pag->presente && pag->modificado) {
            extern memoria_interna_t* memoria_interna;
            void* datos = memoria_interna->memoria + (pag->marco * tamanio_bloque);

            t_paquete* paquete_write = serializar_operacion_write_block(nombre_file, tag, pag->numero_pagina, datos, tamanio_bloque, query_id);
            enviar_paquete(paquete_write, fd_storage);
            esperar_respuesta_storage(fd_storage, query_id, fd_master);
            sem_wait(&sem_respuesta_storage);

            pag->modificado = 0;
        }
    }
}
