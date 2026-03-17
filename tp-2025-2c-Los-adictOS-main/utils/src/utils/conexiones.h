#ifndef UTILS_RED_H_
#define UTILS_RED_H_

#include <stdbool.h>
#include <stdint.h>

int finalizar_servidor(int server_socket);
int aceptar_cliente(int fd_server);
int conectar_a_servidor(char *ip, char* puerto);
int crear_servidor(char* puerto);

#endif /* UTILS_RED_H_ */