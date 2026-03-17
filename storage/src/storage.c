#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <commons/log.h>

#include <utils/conexiones.h>

#include "servidor.h"
#include "storage.h"
#include "deserializacion.h"
#include "serializacion.h"
#include "config.h"
#include "superblock.h"


extern t_log* logger;
extern t_config_storage* config_storage;
extern t_superblock* superblock;

void inicializar_storage(){
    inicializar_servidor();
    finalizar_programa();
}



