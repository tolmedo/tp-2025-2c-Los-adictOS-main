#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "commons/log.h"

#include "hilo-master.h"
#include "hilo-storage.h"
#include "serializacion.h"
#include "deserializacion.h"
#include "config.h"

extern t_log* logger;
t_mensaje_master* mensaje_master;
int id_worker;

void inicializar_worker(int id){
    id_worker = id;
    inicializar_storage();
    inicializar_master(id);
}