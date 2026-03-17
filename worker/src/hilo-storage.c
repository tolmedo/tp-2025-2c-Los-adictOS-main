#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>

#include "commons/log.h"

#include <utils/conexiones.h>
#include <utils/serializacion.h>
#include <utils/deserializacion.h>
#include <utils/conexiones.h>

#include "hilo-storage.h"

#include "config.h"
#include "serializacion.h"
#include "deserializacion.h"

static int fd_storage;

extern t_log* logger;

extern int worker_activo;

extern int id_worker;

static int conectar_a_storage();
static void handshake_con_storage();

void inicializar_storage(){
    fd_storage = conectar_a_storage();
    handshake_con_storage();
}

static int conectar_a_storage(){
    char* ip_storage = obtener_ip_storage();
    char* puerto_storage = obtener_puerto_storage();

    int fd_storage = conectar_a_servidor(ip_storage, puerto_storage);

    if(fd_storage < 0){
        log_error(logger, "Error al intentar conectar con Storage en %s:%s", ip_storage, puerto_storage);
        exit(EXIT_FAILURE);
        return -1;
    }

    log_info(logger, "Conexion establecida con Storage en %s:%s", ip_storage, puerto_storage);

    return fd_storage;
}

static void handshake_con_storage(){
    t_paquete* handshake = serializar_handshake_storage(id_worker);
    enviar_paquete(handshake, fd_storage);

    t_header header = obtener_header(fd_storage);
    void* payload = obtener_payload(fd_storage);

    if (header != HANDSHAKE_OK){
        log_error(logger, "Handshake fallido: Storage rechazó la conexión");
        destruir_payload(payload);
        close(fd_storage);
        exit(EXIT_FAILURE);
    }

    int offset = 0;
    int tamanio_bloque = obtener_un_entero(payload, &offset);

    destruir_payload(payload);
    
    log_info(logger, "Handshake con Storage establecido");
    log_info(logger, "tamaño de bloque: %d", tamanio_bloque);
}

// ================ FUNCIONES PARA ENVIAR OPERACIONES AL STORAGE ================

bool storage_create_file(char* nombre_file, char* tag) {
    t_paquete* paquete = crear_paquete(CREATE_FILE);
    agregar_a_paquete(paquete, nombre_file, strlen(nombre_file) + 1);
    agregar_a_paquete(paquete, tag, strlen(tag) + 1);
    
    if (!enviar_paquete(paquete, fd_storage)) {
        log_error(logger, "Error enviando CREATE_FILE al Storage");
        return false;
    }
    
    // Esperar respuesta Storage
    t_header header = obtener_header(fd_storage);
    void* payload = obtener_payload(fd_storage);
    
    bool exito = (header == RESPUESTA_CREATE_FILE);
    
    if (!exito) {
        log_error(logger, "CREATE_FILE falló: %s:%s", nombre_file, tag);
    }
    
    destruir_payload(payload);
    return exito;
}

// TODO: Seguir la lógica de: bool storage_create_file(char* nombre_file, char* tag) -> Para c/u de las instrucciones (con su "RESPUESTA_INSTRUCCION")