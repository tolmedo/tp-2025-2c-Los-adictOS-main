#ifndef STORAGE_SUPERBLOCK_H_
#define STORAGE_SUPERBLOCK_H_

typedef struct {
    int fs_size;
    int block_size;
} t_superblock;

t_superblock* inicializar_superblock(char* path);
void liberar_superblock(t_superblock* superblock);

int obtener_block_size();
int obtener_fs_size();

#endif /* STORAGE_SUPERBLOCK_H_ */