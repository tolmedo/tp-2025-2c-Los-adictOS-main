#include <stdlib.h>
#include <string.h>
#include <commons/string.h>
#include "deserializacion.h"

// Función auxiliar para separar NOMBRE_FILE:TAG
static void separar_file_tag(char* file_tag, char** nombre_file, char** tag) {
    char** partes = string_split(file_tag, ":");
    if (partes && partes[0] && partes[1]) {
        *nombre_file = string_duplicate(partes[0]);
        *tag = string_duplicate(partes[1]);
    } else {
        *nombre_file = string_duplicate("");
        *tag = string_duplicate("");
    }
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

static t_instruccion* parsear_write(char** tokens) {
    t_instruccion* instr = malloc(sizeof(t_instruccion));
    instr->tipo = INSTRUCCION_WRITE;
    
    separar_file_tag(tokens[1], &instr->argumentos.write.nombre_file, &instr->argumentos.write.tag);
    instr->argumentos.write.direccion_base = atoi(tokens[2]);
    instr->argumentos.write.contenido = string_duplicate(tokens[3]);
    
    return instr;
}

static t_instruccion* parsear_read(char** tokens) {
    t_instruccion* instr = malloc(sizeof(t_instruccion));
    instr->tipo = INSTRUCCION_READ;
    
    separar_file_tag(tokens[1], &instr->argumentos.read.nombre_file, &instr->argumentos.read.tag);
    instr->argumentos.read.direccion_base = atoi(tokens[2]);
    instr->argumentos.read.tamanio = atoi(tokens[3]);
    
    return instr;
}

static t_instruccion* parsear_tag(char** tokens) {
    t_instruccion* instr = malloc(sizeof(t_instruccion));
    instr->tipo = INSTRUCCION_TAG;
    
    separar_file_tag(tokens[1], &instr->argumentos.tag.nombre_file_origen, &instr->argumentos.tag.tag_origen);
    separar_file_tag(tokens[2], &instr->argumentos.tag.nombre_file_destino, &instr->argumentos.tag.tag_destino);
    
    return instr;
}

static t_instruccion* parsear_commit(char** tokens) {
    t_instruccion* instr = malloc(sizeof(t_instruccion));
    instr->tipo = INSTRUCCION_COMMIT;
    
    separar_file_tag(tokens[1], &instr->argumentos.commit.nombre_file, &instr->argumentos.commit.tag);
    
    return instr;
}

static t_instruccion* parsear_flush(char** tokens) {
    t_instruccion* instr = malloc(sizeof(t_instruccion));
    instr->tipo = INSTRUCCION_FLUSH;
    
    separar_file_tag(tokens[1], &instr->argumentos.flush.nombre_file, &instr->argumentos.flush.tag);
    
    return instr;
}

static t_instruccion* parsear_delete(char** tokens) {
    t_instruccion* instr = malloc(sizeof(t_instruccion));
    instr->tipo = INSTRUCCION_DELETE;
    
    separar_file_tag(tokens[1], &instr->argumentos.delete.nombre_file, &instr->argumentos.delete.tag);
    
    return instr;
}

static t_instruccion* parsear_end(char** tokens) {
    t_instruccion* instr = malloc(sizeof(t_instruccion));
    instr->tipo = INSTRUCCION_END;
    
    return instr;
}

t_instruccion* parsear_instruccion(char* linea) {
    // Remover salto de línea si existe
    size_t len = strlen(linea);
    if (len > 0 && linea[len - 1] == '\n') {
        linea[len - 1] = '\0';
    }
    
    char** tokens = string_split(linea, " ");
    t_instruccion* instruccion = NULL;
    
    if (!tokens || !tokens[0]) {
        if (tokens) string_array_destroy(tokens);
        return NULL;
    }
    
    if (string_equals_ignore_case(tokens[0], "CREATE")) {
        instruccion = parsear_create(tokens);
    } else if (string_equals_ignore_case(tokens[0], "TRUNCATE")) {
        instruccion = parsear_truncate(tokens);
    } else if (string_equals_ignore_case(tokens[0], "WRITE")) {
        instruccion = parsear_write(tokens);
    } else if (string_equals_ignore_case(tokens[0], "READ")) {
        instruccion = parsear_read(tokens);
    } else if (string_equals_ignore_case(tokens[0], "TAG")) {
        instruccion = parsear_tag(tokens);
    } else if (string_equals_ignore_case(tokens[0], "COMMIT")) {
        instruccion = parsear_commit(tokens);
    } else if (string_equals_ignore_case(tokens[0], "FLUSH")) {
        instruccion = parsear_flush(tokens);
    } else if (string_equals_ignore_case(tokens[0], "DELETE")) {
        instruccion = parsear_delete(tokens);
    } else if (string_equals_ignore_case(tokens[0], "END")) {
        instruccion = parsear_end(tokens);
    }

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
            
        case INSTRUCCION_WRITE:
            free(instruccion->argumentos.write.nombre_file);
            free(instruccion->argumentos.write.tag);
            free(instruccion->argumentos.write.contenido);
            break;
            
        case INSTRUCCION_READ:
            free(instruccion->argumentos.read.nombre_file);
            free(instruccion->argumentos.read.tag);
            break;
            
        case INSTRUCCION_TAG:
            free(instruccion->argumentos.tag.nombre_file_origen);
            free(instruccion->argumentos.tag.tag_origen);
            free(instruccion->argumentos.tag.nombre_file_destino);
            free(instruccion->argumentos.tag.tag_destino);
            break;
            
        case INSTRUCCION_COMMIT:
            free(instruccion->argumentos.commit.nombre_file);
            free(instruccion->argumentos.commit.tag);
            break;
            
        case INSTRUCCION_FLUSH:
            free(instruccion->argumentos.flush.nombre_file);
            free(instruccion->argumentos.flush.tag);
            break;
            
        case INSTRUCCION_DELETE:
            free(instruccion->argumentos.delete.nombre_file);
            free(instruccion->argumentos.delete.tag);
            break;
            
        case INSTRUCCION_END:
            break;
    }
    
    free(instruccion);
}