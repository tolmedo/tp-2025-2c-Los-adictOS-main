#include "superblock.h"
#include <stdlib.h>
#include <commons/config.h>

extern t_superblock* superblock;

t_superblock* inicializar_superblock(char* path) {
    t_config* superblock_temp = config_create(path);
    if (superblock_temp == NULL) {
        return NULL;
    }

    t_superblock* superblock = malloc(sizeof(t_superblock));
    if (superblock == NULL) {
        config_destroy(superblock_temp);
        return NULL;
    }

    superblock->fs_size = config_get_int_value(superblock_temp, "FS_SIZE");
    superblock->block_size = config_get_int_value(superblock_temp, "BLOCK_SIZE");
    
    config_destroy(superblock_temp);
    return superblock;
}

void liberar_superblock(t_superblock* superblock) {
    if (superblock != NULL) {
        free(superblock);
    }
}

// ---------------- GETTERS ----------------------- //
int obtener_block_size() {
    return superblock ? superblock->block_size : 0;
}

int obtener_fs_size() {
    return superblock ? superblock->fs_size : 0;
}