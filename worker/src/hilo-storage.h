#ifndef HILO_MEMORIA_H
#define HILO_MEMORIA_H

// Inicialización
void inicializar_storage();

// Operaciones del Storage (síncronas - esperan respuesta)
bool storage_create_file(char* nombre_file, char* tag);

#endif