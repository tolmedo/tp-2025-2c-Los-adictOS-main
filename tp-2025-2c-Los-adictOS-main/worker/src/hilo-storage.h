#ifndef HILO_STORAGE_H
#define HILO_STORAGE_H

void inicializar_storage(void);
void esperar_respuesta_storage(int fd, int query_id, int fd_master);

#endif