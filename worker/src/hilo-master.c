#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>

#include "commons/log.h"

#include <utils/conexiones.h>
#include <utils/serializacion.h>
#include <utils/deserializacion.h>
#include <utils/conexiones.h>

#include "hilo-master.h"
#include "config.h"
#include "serializacion.h"
#include "deserializacion.h"

static int fd_master;

extern t_log* logger;

extern int worker_activo;

static int conectar_a_master();
static void handshake_con_master(int id_worker);

void inicializar_master(int id_worker){
    fd_master = conectar_a_master();
    handshake_con_master(id_worker);
}

static int conectar_a_master(){
    char* ip_master = obtener_ip_master();
    char* puerto_storage = obtener_puerto_master();

    int fd_master = conectar_a_servidor(ip_master, puerto_storage);

    if(fd_master < 0){
        log_error(logger, "Error al intentar conectar con Master en %s:%s", ip_master, puerto_storage);
        exit(EXIT_FAILURE);
        return -1;
    }

    log_info(logger, "Conexion establecida con Master en %s:%s", ip_master, puerto_storage);

    return fd_master;
}

static void handshake_con_master(int id_worker){
    t_paquete* handshake = serializar_handshake_master(id_worker);
    enviar_paquete(handshake, fd_master);

    t_header header = obtener_header(fd_master);
    //void* payload = obtener_payload(fd_master);

    if (header != HANDSHAKE_OK){
        log_error(logger, "Handshake fallido: Storage rechazó la conexión");
        //destruir_payload(payload);
        close(fd_master);
        exit(EXIT_FAILURE);
    }

    //int offset = 0;
    //destruir_payload(payload);
    
    log_info(logger, "Handshake con Master establecido");

}