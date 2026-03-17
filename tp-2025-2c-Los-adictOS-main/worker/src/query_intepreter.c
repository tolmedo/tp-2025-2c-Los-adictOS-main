#include "query_interpreter.h"
#include "config.h"
#include "tabla_de_paginas.h"
#include "serializacion.h"
#include "hilo-storage.h"
#include "deserializacion.h"
#include "worker.h"

#include <utils/serializacion.h>
#include <utils/deserializacion.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <commons/string.h>
#include <semaphore.h>

extern t_log* logger;
extern int fd_storage;
extern int tamanio_bloque;
extern memoria_interna_t* memoria_interna;
extern t_list* tablas_de_paginas;
extern int fd_master;

// Variables de control protegidas por mutex
static int query_desalojada = 0;
static int pc_actual = 0;

// DEFINIR MUTEX 2
pthread_mutex_t mutex_control_query = PTHREAD_MUTEX_INITIALIZER;

extern sem_t sem_respuesta_storage;
extern sem_t sem_desalojo;
extern sem_t sem_query_finalizada;

void marcar_query_desalojada() {
    pthread_mutex_lock(&mutex_control_query);
    query_desalojada = 1;
    pthread_mutex_unlock(&mutex_control_query);
    
    log_debug(logger, "[DEBUG] Query marcada como desalojada");
}

int obtener_pc_actual() {
    pthread_mutex_lock(&mutex_control_query);
    int pc = pc_actual;
    pthread_mutex_unlock(&mutex_control_query);
    
    return pc;
}

static bool esta_query_desalojada() {
    pthread_mutex_lock(&mutex_control_query);
    bool desalojada = (query_desalojada == 1);
    pthread_mutex_unlock(&mutex_control_query);
    
    return desalojada;
}

static void actualizar_pc(int nuevo_pc) {
    pthread_mutex_lock(&mutex_control_query);
    pc_actual = nuevo_pc;
    pthread_mutex_unlock(&mutex_control_query);
}

void ejecutarQuery(char* path_query, int query_id, int pc_inicial) {
    pthread_mutex_lock(&mutex_ejecucion_query);
    query_en_ejecucion = true;
    log_info(logger, "## Query %d: Se recibe la Query. El path de operaciones es: %s", query_id, path_query);

    FILE* archivo = fopen(path_query, "r");
    if (!archivo) {
        log_error(logger, "No se pudo abrir el archivo de query: %s", path_query);

        query_en_ejecucion = false;
        pthread_mutex_unlock(&mutex_ejecucion_query);
        
        return;
    }

    char linea[512];
    int pc = 0;
    
    query_desalojada = 0;
    pc_actual = pc_inicial;

    while (pc < pc_inicial && fgets(linea, sizeof(linea), archivo)) {
        linea[strcspn(linea, "\n")] = 0;
        
        // Ignorar líneas vacías y comentarios
        if (strlen(linea) == 0 || linea[0] == '#') {
            continue;
        }

        pc++;
    }

    while (fgets(linea, sizeof(linea), archivo)) {
        if (esta_query_desalojada()) {
            
            fclose(archivo);
            
            query_en_ejecucion = false;
            int pc_final = obtener_pc_actual();
            log_debug(logger, "Query %d desalojada en PC %d", query_id, pc_final);
            pthread_mutex_unlock(&mutex_ejecucion_query);
            
            sem_post(&sem_desalojo);

            return;
        }

        linea[strcspn(linea, "\n")] = 0;
        
        if (strlen(linea) == 0 || linea[0] == '#') {
            continue;
        }

        // FETCH: Loguear instrucción SIN parámetros
        char* instruccion_sin_params = string_duplicate(linea);
        char* espacio = strchr(instruccion_sin_params, ' ');
        if (espacio) {
            *espacio = '\0';
        }

        log_info(logger, "## Query %d: FETCH - Program Counter: %d - %s", query_id, pc, instruccion_sin_params);
        free(instruccion_sin_params);

        t_instruccion* inst = parsear_instruccion(linea);

        if (inst) {
            ejecutar_instruccion(inst, query_id, pc);
            if(!query_en_ejecucion){
                destruir_instruccion(inst);
                liberar_marcos_query(query_id);
                fclose(archivo);
                log_error(logger, "Query %d finalizada por error", query_id);
                pthread_mutex_unlock(&mutex_ejecucion_query);

                sem_post(&sem_query_finalizada);
                return;
            }
            char* nombre_instruccion = NULL;
            switch(inst->tipo) {
                case INSTRUCCION_CREATE: nombre_instruccion = "CREATE"; break;
                case INSTRUCCION_TRUNCATE: nombre_instruccion = "TRUNCATE"; break;
                case INSTRUCCION_WRITE: nombre_instruccion = "WRITE"; break;
                case INSTRUCCION_READ: nombre_instruccion = "READ"; break;
                case INSTRUCCION_TAG: nombre_instruccion = "TAG"; break;
                case INSTRUCCION_COMMIT: nombre_instruccion = "COMMIT"; break;
                case INSTRUCCION_FLUSH: nombre_instruccion = "FLUSH"; break;
                case INSTRUCCION_DELETE: nombre_instruccion = "DELETE"; break;
                case INSTRUCCION_END: nombre_instruccion = "END"; break;
            }
            log_info(logger, "## Query %d: - Instrucción realizada: %s", query_id, nombre_instruccion);  

            destruir_instruccion(inst);
        }

        pc++;
        
        actualizar_pc(pc);
    }

    fclose(archivo);
    
    query_en_ejecucion = false;
    pthread_mutex_unlock(&mutex_ejecucion_query);
    
    log_debug(logger, "[DEBUG] Query %d finalizada - Worker libre", query_id);
}

void ejecutar_instruccion(t_instruccion* inst, int query_id, int pc) {
    if (!inst) return;
    t_paquete* paquete = NULL;
    t_tabla_de_paginas* tabla = NULL;
    int num_pag = 0, offset = 0;
    t_pagina* pagina = NULL;
    char datos_a_enviar[4096];
    memset(datos_a_enviar, 0, sizeof(datos_a_enviar));

    switch (inst->tipo) {
        case INSTRUCCION_CREATE:
            paquete = serializar_operacion_create(inst->argumentos.create.nombre_file, inst->argumentos.create.tag, query_id);
            enviar_paquete(paquete, fd_storage);
            esperar_respuesta_storage(fd_storage, query_id, fd_master);

            sem_wait(&sem_respuesta_storage);
            break;

        case INSTRUCCION_TRUNCATE: {
            paquete = serializar_operacion_truncate(inst->argumentos.truncate.nombre_file,inst->argumentos.truncate.tag,inst->argumentos.truncate.tamanio, query_id);
            enviar_paquete(paquete, fd_storage);
            esperar_respuesta_storage(fd_storage, query_id, fd_master);

            sem_wait(&sem_respuesta_storage);
            break;
        }
        case INSTRUCCION_WRITE: {
            tabla = buscar_tabla(memoria_interna, inst->argumentos.write.nombre_file, inst->argumentos.write.tag);
    
            if (!tabla) {
                log_error(logger, "Query %d: No se encontró tabla para %s:%s", 
                        query_id, inst->argumentos.write.nombre_file, inst->argumentos.write.tag);
                break;
            }

            uint32_t direccion = inst->argumentos.write.direccion_base;
            uint32_t contenido_len = strlen(inst->argumentos.write.contenido);
            uint32_t bytes_escritos = 0;

            while (bytes_escritos < contenido_len) {
                uint32_t direccion_actual = direccion + bytes_escritos;
                num_pag = direccion_actual / tamanio_bloque;
                offset = direccion_actual % tamanio_bloque;

                uint32_t bytes_restantes_contenido = contenido_len - bytes_escritos;
                uint32_t espacio_disponible_pagina = tamanio_bloque - offset;
                uint32_t bytes_a_escribir = (bytes_restantes_contenido < espacio_disponible_pagina) 
                                            ? bytes_restantes_contenido 
                                            : espacio_disponible_pagina;

                pagina = buscar_pagina(memoria_interna, tabla, num_pag, query_id, fd_storage, offset, inst->argumentos.write.nombre_file, inst->argumentos.write.tag);
        
                if (!pagina) {
                    log_error(logger, "Query %d: Error al buscar página %d", query_id, num_pag);
                    break;
                }

                // Escribir en la memoria interna
                void* destino = memoria_interna->memoria + (pagina->marco * tamanio_bloque) + offset;
                memcpy(destino, inst->argumentos.write.contenido + bytes_escritos, bytes_a_escribir);

                int retardo_instruccion = obtener_retardo_memoria();
                usleep(retardo_instruccion * 1000);
        
                // Marcar página como presente y modificada
                pagina->presente = 1;
                pagina->modificado = 1;

                int dir_fisica = (pagina->marco * tamanio_bloque) + offset;
        
                char buffer_log[bytes_a_escribir + 1];
                memcpy(buffer_log, inst->argumentos.write.contenido + bytes_escritos, bytes_a_escribir);
                buffer_log[bytes_a_escribir] = '\0';
        
                log_info(logger, "Query %d: Acción: ESCRIBIR - Dirección Física: %d - Valor: %s", query_id, dir_fisica, buffer_log);

                bytes_escritos += bytes_a_escribir;
            }
            break;
        }
        case INSTRUCCION_READ: {
            int tamanio = inst->argumentos.read.tamanio;
            char* nombre_file = inst->argumentos.read.nombre_file;
            char* tag_file = inst->argumentos.read.tag;

            tabla = buscar_tabla(memoria_interna, nombre_file, tag_file);

            if (!tabla) {
                log_error(logger, "[DEBUG] READ: tabla es NULL!");
                break;
            }

            uint32_t direccion = inst->argumentos.read.direccion_base;
            uint32_t bytes_leidos = 0;

            memset(datos_a_enviar, 0, sizeof(datos_a_enviar));

            while (bytes_leidos < tamanio) {
                uint32_t direccion_actual = direccion + bytes_leidos;
                num_pag = direccion_actual / tamanio_bloque;
                offset = direccion_actual % tamanio_bloque;

                uint32_t bytes_restantes_lectura = tamanio - bytes_leidos;
                uint32_t espacio_disponible_pagina = tamanio_bloque - offset;
                uint32_t bytes_a_leer = (bytes_restantes_lectura < espacio_disponible_pagina) 
                                ? bytes_restantes_lectura 
                                : espacio_disponible_pagina;

                pagina = buscar_pagina(memoria_interna, tabla, num_pag, query_id, fd_storage, offset, nombre_file, tag_file);
        
                if (!pagina) {
                    log_debug(logger, "[DEBUG] READ: pagina %d es NULL!", num_pag);
                    break;
                }

                void* origen = memoria_interna->memoria + (pagina->marco * tamanio_bloque) + offset;
                char buffer_parcial[bytes_a_leer + 1];
                memcpy(buffer_parcial, origen, bytes_a_leer);
                buffer_parcial[bytes_a_leer] = '\0';

                int retardo_instruccion = obtener_retardo_memoria();
                usleep(retardo_instruccion * 1000);

                int dir_fisica_lectura = (pagina->marco * tamanio_bloque) + offset;

                // Verificar si el buffer tiene contenido no vacío
                bool contenido_vacio = true;
                for(uint32_t k = 0; k < bytes_a_leer; k++) {
                    if(buffer_parcial[k] != '\0') {
                        contenido_vacio = false;
                        break;
                    }
                }

                log_info(logger, "Query %d: Acción: LEER - Dirección Física: %d - Valor: %s", query_id, dir_fisica_lectura, buffer_parcial);

                // Solo enviar al Master si hay contenido
                if(!contenido_vacio) {
                    paquete = crear_paquete(LECTURA_REALIZADA);
                    agregar_a_paquete(paquete, nombre_file, strlen(nombre_file) + 1);
                    agregar_a_paquete(paquete, tag_file, strlen(tag_file) + 1);
                    agregar_a_paquete(paquete, buffer_parcial, bytes_a_leer);
                    enviar_paquete(paquete, fd_master);
                    log_debug(logger, "Query %d: Lectura enviada al Master (%d bytes)", query_id, bytes_a_leer);
                } else {
                    log_debug(logger, "Query %d: Lectura con contenido vacío, no se envía al Master", query_id);
                }

                bytes_leidos += bytes_a_leer;
            }

            break;
        }       
        case INSTRUCCION_TAG: {
            paquete = serializar_operacion_tag(inst->argumentos.tag.nombre_file_origen,
                inst->argumentos.tag.tag_origen,
                inst->argumentos.tag.nombre_file_destino,
                inst->argumentos.tag.tag_destino,
                query_id);
            enviar_paquete(paquete, fd_storage);
            esperar_respuesta_storage(fd_storage, query_id, fd_master);

            sem_wait(&sem_respuesta_storage);
            break;
        }
        case INSTRUCCION_COMMIT:{
            flush_paginas_modificadas(inst->argumentos.commit.nombre_file, inst->argumentos.commit.tag, query_id, fd_storage);

            paquete = serializar_operacion_commit(inst->argumentos.commit.nombre_file, inst->argumentos.commit.tag, query_id);
            enviar_paquete(paquete, fd_storage);
            esperar_respuesta_storage(fd_storage, query_id, fd_master);

            sem_wait(&sem_respuesta_storage);
            break;
        }
        case INSTRUCCION_FLUSH:{
            flush_paginas_modificadas(inst->argumentos.flush.nombre_file, inst->argumentos.flush.tag, query_id, fd_storage);
        
            break;
        }
        case INSTRUCCION_DELETE:{
            paquete = serializar_operacion_delete(inst->argumentos.delete.nombre_file, inst->argumentos.delete.tag, query_id);
            enviar_paquete(paquete, fd_storage);
            esperar_respuesta_storage(fd_storage, query_id, fd_master);

            sem_wait(&sem_respuesta_storage);
            break;
        }
        case INSTRUCCION_END:{
            char* motivo = "COMPLETADA";
            flush_por_desalojo(query_id);

            paquete = serializar_fin_query(query_id, motivo);
            enviar_paquete(paquete, fd_master);

            break;
        }
        default:
            log_debug(logger, "Instrucción desconocida en PC %d", pc);
            break;
    }
}
void flush_por_desalojo(int query_id) {
    extern t_list* tablas_de_paginas;
    extern pthread_mutex_t mutex_tablas_paginas;
    extern int fd_storage;
    extern memoria_interna_t* memoria_interna;  // ← AGREGAR ESTA LÍNEA
    
    if (!tablas_de_paginas) {
        return;
    }
    
    pthread_mutex_lock(&mutex_tablas_paginas);
    
    log_debug(logger, "[DEBUG] Query %d: Iniciando flush de todas las tablas activas", query_id);
    
    for (int i = 0; i < list_size(tablas_de_paginas); i++) {
        t_tabla_de_paginas* tabla = list_get(tablas_de_paginas, i);
        
        if (!tabla || !tabla->paginas) {
            continue;
        }
        
        bool tiene_modificaciones = false;
        
        // Verificar si hay páginas modificadas
        for (int j = 0; j < list_size(tabla->paginas); j++) {
            t_pagina* pag = list_get(tabla->paginas, j);
            if (pag->presente && pag->modificado) {
                tiene_modificaciones = true;
                break;
            }
        }
        
        if (tiene_modificaciones) {
            log_debug(logger, "[DEBUG] Query %d: Flushing tabla %s:%s", query_id, tabla->nombre_file, tabla->tag);
            
            pthread_mutex_unlock(&mutex_tablas_paginas);
            flush_paginas_modificadas(tabla->nombre_file, tabla->tag, query_id, fd_storage);
            pthread_mutex_lock(&mutex_tablas_paginas);
        }
        
        // Liberar TODOS los marcos de esta tabla cuando se desaloja
        for (int j = 0; j < list_size(tabla->paginas); j++) {
            t_pagina* pag = list_get(tabla->paginas, j);
            if (pag->presente) {
                // Marcar marco como libre
                memoria_interna->marcos_ocupados[pag->marco] = false;
                // Marcar página como no presente
                pag->presente = 0;
                log_info(logger, "Query %d: Se libera el Marco: %d perteneciente al - File: %s - Tag: %s", query_id, pag->marco, tabla->nombre_file, tabla->tag);
            }
        }
    }
    
    pthread_mutex_unlock(&mutex_tablas_paginas);
}
void liberar_marcos_query(int query_id) {
    extern t_list* tablas_de_paginas;
    extern pthread_mutex_t mutex_tablas_paginas;
    extern memoria_interna_t* memoria_interna;

    if (!tablas_de_paginas) {
        return;
    }

    pthread_mutex_lock(&mutex_tablas_paginas);

    for (int i = 0; i < list_size(tablas_de_paginas); i++) {
        t_tabla_de_paginas* tabla = list_get(tablas_de_paginas, i);
        if (!tabla || !tabla->paginas) {
            continue;
        }

        for (int j = 0; j < list_size(tabla->paginas); j++) {
            t_pagina* pag = list_get(tabla->paginas, j);
            if (pag->presente) {
                memoria_interna->marcos_ocupados[pag->marco] = false;
                pag->presente = 0;
                pag->modificado = 0;

                log_info(logger,
                    "Query %d: Se libera el Marco: %d perteneciente al - File: %s - Tag: %s",
                    query_id, pag->marco, tabla->nombre_file, tabla->tag);
            }
        }
    }

    pthread_mutex_unlock(&mutex_tablas_paginas);
}
