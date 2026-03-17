#ifndef SERIALIZACION_H
#define SERIALIZACION_H

#include "utils/serializacion.h"

t_paquete* serializar_handshake_storage(int id_worker);
t_paquete* serializar_handshake_master(int id_worker);

#endif