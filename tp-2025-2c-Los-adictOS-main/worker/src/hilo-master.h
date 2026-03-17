#ifndef HILO_MASTER_H
#define HILO_MASTER_H

void inicializar_master(int id_worker);
void* escuchar_master(void* arg);
void* ejecutar_query_thread(void* arg);

#endif