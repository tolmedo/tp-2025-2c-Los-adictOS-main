#include <stdlib.h>
#include <string.h>
#include <commons/string.h>
#include "deserializacion.h"

// Función auxiliar para separar NOMBRE_FILE:TAG
static void separar_file_tag(char* file_tag, char** nombre_file, char** tag) {
    char** partes = string_split(file_tag, ":");
    *nombre_file = string_duplicate(partes[0]);
    *tag = string_duplicate(partes[1]);
    string_array_destroy(partes);
}

static t_instruccion* parsear_create(char** tokens) {
    t_instruccion* instr = malloc(sizeof(t_instruccion));
    instr->tipo = INSTRUCCION_CREATE;
    
    // tokens[1] = "MATERIAS:BASE"
    separar_file_tag(tokens[1], &instr->argumentos.create.nombre_file,&instr->argumentos.create.tag);
    
    return instr;
}

static t_instruccion* parsear_truncate(char** tokens) {
    t_instruccion* instr = malloc(sizeof(t_instruccion));
    instr->tipo = INSTRUCCION_TRUNCATE;
    
    // tokens[1] = "MATERIAS:BASE", tokens[2] = "1024"
    separar_file_tag(tokens[1], &instr->argumentos.truncate.nombre_file,&instr->argumentos.truncate.tag);
    instr->argumentos.truncate.tamanio = atoi(tokens[2]);
    
    return instr;
}

//---------------------------------------- //
// TODO: Parsear c/u de las instrucciones
//---------------------------------------- //

t_instruccion* parsear_instruccion(char* linea) {
    // Remover salto de línea si existe
    if (linea[strlen(linea) - 1] == '\n') {
        linea[strlen(linea) - 1] = '\0';
    }
    
    char** tokens = string_split(linea, " ");
    t_instruccion* instruccion = NULL;
    
    if (string_equals_ignore_case(tokens[0], "CREATE")) {
        instruccion = parsear_create(tokens);
    } else if (string_equals_ignore_case(tokens[0], "TRUNCATE")) {
        instruccion = parsear_truncate(tokens);
    } else if (string_equals_ignore_case(tokens[0], "WRITE"))
    // Y así con todas las instrucciones

    string_array_destroy(tokens);
    return instruccion;
}

void destruir_instruccion(t_instruccion* instruccion) { 
    if (!instruccion) return;
    
    switch(instruccion->tipo) {
        case INSTRUCCION_CREATE:
            free(instruccion->argumentos.create.nombre_file);
            free(instruccion->argumentos.create.tag);
            break;  
        case INSTRUCCION_TRUNCATE:
            free(instruccion->argumentos.truncate.nombre_file);
            free(instruccion->argumentos.truncate.tag);
            break;
        // TODO: destruir c/u de los argumentos de las instrucciones .... (tienen que ser de tipo char*)
        case INSTRUCCION_END:
            // No tiene campos que liberar
            break;
    }
    
    free(instruccion);
}
