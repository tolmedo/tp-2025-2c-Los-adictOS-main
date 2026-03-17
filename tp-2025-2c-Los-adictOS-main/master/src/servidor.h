#ifndef MASTER_SERVIDOR_H_
#define MASTER_SERVIDOR_H_

#include <stdbool.h>

extern volatile bool programa_activo;

void inicializar_servidor(void);
void finalizar_programa();

#endif