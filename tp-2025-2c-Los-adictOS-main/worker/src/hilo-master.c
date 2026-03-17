#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <semaphore.h>

#include "commons/log.h"

#include <utils/conexiones.h>
#include <utils/serializacion.h>
#include <utils/deserializacion.h>

#include "hilo-master.h"
#include "config.h"
#include "serializacion.h"
#include "query_interpreter.h"
#include "worker.h"
#include "tabla_de_paginas.h"

static int fd_master_global;
int fd_master;

extern t_log* logger;
extern int worker_activo;

extern pthread_mutex_t mutex_ejecucion_query;
extern bool query_en_ejecucion;

extern sem_t sem_desalojo;

static int conectar_a_master();
static void handshake_con_master(int id_worker);

void inicializar_master(int id_worker){
    fd_master_global = conectar_a_master();
    fd_master = fd_master_global;
    handshake_con_master(id_worker);
}

static int conectar_a_master(){
    char* ip_master = obtener_ip_master();
    char* puerto_master = obtener_puerto_master();

    int fd = conectar_a_servidor(ip_master, puerto_master);

    if(fd < 0){
        log_error(logger, "Error al intentar conectar con Master en %s:%s", ip_master, puerto_master);
        exit(EXIT_FAILURE);
        return -1;
    }

    log_debug(logger, "Conexión establecida con Master en %s:%s", ip_master, puerto_master);

    return fd;
}

static void handshake_con_master(int id_worker){
    t_paquete* handshake = serializar_handshake(id_worker);
    enviar_paquete(handshake, fd_master_global);

    t_header header = obtener_header(fd_master_global);

    if (header != HANDSHAKE_OK){
        log_error(logger, "Handshake fallido: Master rechazó la conexión");
        close(fd_master_global);
        exit(EXIT_FAILURE);
    }
    
    void* payload = obtener_payload(fd_master_global);
    if (payload) {
        destruir_payload(payload);
    }

    log_debug(logger, "Handshake con Master establecido - Worker ID: %d", id_worker);
}

void* escuchar_master(void* arg) {
    int* id_worker_ptr = (int*)arg;
    int id_worker = *id_worker_ptr;
    free(id_worker_ptr);

    log_debug(logger, "Worker %d escuchando al Master", id_worker);

    while(worker_activo) {
        t_header header = obtener_header(fd_master_global);
        
        if(header < 0) {
            log_error(logger, "Conexión con Master perdida");
            worker_activo = 0;
            break;
        }

        if(header == 0) {
            log_debug(logger, "Header 0 recibido del Master, ignorando...");
            void* payload = obtener_payload(fd_master_global);
            destruir_payload(payload);
            continue;
        }

        switch(header) {
            case EJECUTAR_QUERY: {
                int offset = 0;
                void* payload = obtener_payload(fd_master_global);
                
                char* path_query = obtener_un_string(payload, &offset);
                int query_id = obtener_un_entero(payload, &offset);
                int pc_inicial = obtener_un_entero(payload, &offset);
                
                log_debug(logger, "Worker %d: Master solicita ejecutar Query %d desde PC %d: %s", id_worker, query_id, pc_inicial, path_query);

                // Ejecutar query en hilo separado
                pthread_t tid;
                typedef struct {
                    char* path;
                    int qid;
                    int pc;
                } args_query_t;
                
                args_query_t* args = malloc(sizeof(args_query_t));
                args->path = path_query;
                args->qid = query_id;
                args->pc = pc_inicial;
                
                //pthread_create(&tid, NULL, ejecutar_query_thread, args);
                if (pthread_create(&tid, NULL, ejecutar_query_thread, args) != 0) {
                    log_error(logger, "[CRÍTICO] No se pudo crear thread para Query %d", query_id);
                    free(args->path);
                    free(args);
                    destruir_payload(payload);
                    break;
                }
                pthread_detach(tid);
                
                destruir_payload(payload);
                break;
            }

            case DESALOJAR_QUERY: {
                int offset = 0;
                void* payload = obtener_payload(fd_master_global);
                int query_id = obtener_un_entero(payload, &offset);

                marcar_query_desalojada();

                sem_wait(&sem_desalojo);

                flush_por_desalojo(query_id);

                int pc_actual = obtener_pc_actual();

                t_paquete* respuesta = crear_paquete(DESALOJO_QUERY);
                agregar_a_paquete(respuesta, &query_id, sizeof(int));
                agregar_a_paquete(respuesta, &pc_actual, sizeof(int));
                enviar_paquete(respuesta, fd_master_global);
                
                log_info(logger, "## Query %d: Desalojada por pedido del Master", query_id);

                destruir_payload(payload);
                break;
            }
            default:
                log_debug(logger, "Worker %d: Mensaje desconocido del Master: %d", id_worker, header);
                if(header > 0) {
                    void* payload = obtener_payload(fd_master_global);
                    destruir_payload(payload);
                }
                break;
        }
    }

    return NULL;
}

void* ejecutar_query_thread(void* arg) {
    typedef struct {
        char* path;
        int qid;
        int pc;
    } args_query_t;
    
    args_query_t* args = (args_query_t*)arg;

    ejecutarQuery(args->path, args->qid, args->pc);

    if (args->path) {
        free(args->path);
    }
    free(args);
    return NULL;
}