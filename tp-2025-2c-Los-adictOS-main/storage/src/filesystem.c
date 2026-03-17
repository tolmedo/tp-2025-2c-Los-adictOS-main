#define _GNU_SOURCE

#include "filesystem.h"
#include "config.h"
#include "superblock.h"
#include "serializacion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>


#include <commons/log.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <commons/crypto.h>

extern t_log* logger;
int error_read;

static t_dictionary* file_tag_locks = NULL; // commons dictionary
static pthread_mutex_t mutex_locks_dict = PTHREAD_MUTEX_INITIALIZER; // MUTEX PARA BLOQUES-FILE:TAG -> Dos Workers podrían intentar truncar el mismo File:Tag simultáneamente o Uno podría leer metadata mientras otro la está modificando

static pthread_mutex_t mutex_bitmap = PTHREAD_MUTEX_INITIALIZER; // MUTEX PARA BITMAPS -> Dos Workers podrían intentar obtener el mismo bloque libre simultáneamente o Uno podría marcar un bloque como libre mientras otro lo está usando

static pthread_mutex_t mutex_hash_index = PTHREAD_MUTEX_INITIALIZER; // MUTEX PARA HASH -> Dos Workers podrían intentar agregar el mismo hash simultáneamente durante COMMIT

// Función auxiliar para obtener/crear lock de un File:Tag
pthread_mutex_t* obtener_lock_file_tag(const char* nombre_file, const char* nombre_tag) {
    char* key = string_from_format("%s:%s", nombre_file, nombre_tag);
    
    pthread_mutex_lock(&mutex_locks_dict);
    
    t_file_tag_lock* lock = dictionary_get(file_tag_locks, key);
    
    if (!lock) {
        // Crear nuevo mutex para este File:Tag
        lock = malloc(sizeof(t_file_tag_lock));
        pthread_mutex_init(&lock->mutex, NULL);
        lock->key = string_duplicate(key);
        dictionary_put(file_tag_locks, key, lock);
        
        log_debug(logger, "Creado mutex para %s", key);
    }
    
    pthread_mutex_unlock(&mutex_locks_dict);
    
    free(key);
    return &lock->mutex;
}

t_filesystem* inicializar_filesystem(void) {
    t_filesystem* fs = malloc(sizeof(t_filesystem));
    if (!fs) {
        log_error(logger, "Error al asignar memoria para filesystem");
        return NULL;
    }

    // Inicializar valores
    fs->bitmap = NULL;
    fs->bitmap_data = NULL;
    fs->bitmap_fd = -1;
    fs->root_path = strdup(obtener_punto_montaje());
    fs->total_blocks = obtener_fs_size() / obtener_block_size();
    fs->bitmap_size = (fs->total_blocks + 7) / 8; // Redondeamos hacia arriba

    log_debug(logger, "Filesystem inicializado - Total de bloques: %d, Tamaño bitmap: %zu bytes", 
             fs->total_blocks, fs->bitmap_size);

    // Verificar si necesitamos formatear
    if (obtener_fresh_start()) {
        log_debug(logger, "FRESH_START=TRUE - Formateando filesystem...");
        if (!formatear_filesystem(fs)) {
            destruir_filesystem(fs);
            return NULL;
        }
    } else {
        log_debug(logger, "FRESH_START=FALSE - Cargando filesystem existente...");
        if (!inicializar_bitmap(fs) || !inicializar_blocks_index(fs)) {
            log_error(logger, "Error al cargar filesystem existente");
            destruir_filesystem(fs);
            return NULL;
        }
    }

    if (!file_tag_locks) {
        file_tag_locks = dictionary_create();
    }

    return fs;
}

void destruir_filesystem(t_filesystem* fs) {
    if (!fs) return;

    if (fs->bitmap) {
        bitarray_destroy(fs->bitmap);
    }

    if (fs->bitmap_data && fs->bitmap_data != MAP_FAILED) {
        munmap(fs->bitmap_data, fs->bitmap_size);
    }

    if (fs->bitmap_fd != -1) {
        close(fs->bitmap_fd);
    }

    free(fs->root_path);
    free(fs);
}

// ==================== FORMATEO DEL FILESYSTEM ====================

bool formatear_filesystem(t_filesystem* fs) {
    log_debug(logger, "Comenzando formateo del filesystem...");

    // 1. Eliminar archivos existentes si los hay (excepto superblock.config)
    char comando[512];
    snprintf(comando, sizeof(comando), "find %s -mindepth 1 ! -name 'superblock.config' -delete 2>/dev/null", fs->root_path);
    system(comando);

    // 2. Crear estructura de directorios
    if (!crear_directorios_base(fs)) {
        log_error(logger, "Error al crear directorios base");
        return false;
    }

    // 3. Inicializar bitmap
    if (!inicializar_bitmap(fs)) {
        log_error(logger, "Error al inicializar bitmap");
        return false;
    }

    // 4. Inicializar blocks_hash_index
    if (!inicializar_blocks_index(fs)) {
        log_error(logger, "Error al inicializar blocks_hash_index");
        return false;
    }

    // 5. Crear bloques físicos
    if (!crear_bloques_fisicos(fs)) {
        log_error(logger, "Error al crear bloques físicos");
        return false;
    }

    // 6. Crear initial_file con tag BASE
    if (!crear_initial_file(fs)) {
        log_error(logger, "Error al crear initial_file");
        return false;
    }

    log_debug(logger, "Filesystem formateado exitosamente");
    return true;
}

// ==================== INICIALIZACIÓN DE COMPONENTES ====================

bool inicializar_bitmap(t_filesystem* fs) {
    char* bitmap_path = string_from_format("%s/bitmap.bin", fs->root_path);
    
    // Abrir o crear el archivo
    fs->bitmap_fd = open(bitmap_path, O_RDWR | O_CREAT, 0644);
    if (fs->bitmap_fd == -1) {
        log_error(logger, "Error al abrir bitmap.bin: %s", strerror(errno));
        free(bitmap_path);
        return false;
    }

    // Si el archivo es nuevo o vacío, lo inicializamos
    struct stat st;
    fstat(fs->bitmap_fd, &st);
    
    if (st.st_size == 0 || obtener_fresh_start()) {
        // Expandir el archivo al tamaño necesario
        if (ftruncate(fs->bitmap_fd, fs->bitmap_size) == -1) {
            log_error(logger, "Error al expandir bitmap.bin: %s", strerror(errno));
            close(fs->bitmap_fd);
            free(bitmap_path);
            return false;
        }

        // Inicializar todos los bits en 0 (libres)
        lseek(fs->bitmap_fd, 0, SEEK_SET);
        char* ceros = calloc(1, fs->bitmap_size);
        write(fs->bitmap_fd, ceros, fs->bitmap_size);
        free(ceros);
    }

    // Mapear el archivo en memoria usando mmap
    fs->bitmap_data = mmap(NULL, fs->bitmap_size, PROT_READ | PROT_WRITE, 
                           MAP_SHARED, fs->bitmap_fd, 0);
    
    if (fs->bitmap_data == MAP_FAILED) {
        log_error(logger, "Error al mapear bitmap.bin: %s", strerror(errno));
        close(fs->bitmap_fd);
        free(bitmap_path);
        return false;
    }

    // Crear el bitarray
    fs->bitmap = bitarray_create_with_mode((char*)fs->bitmap_data, fs->bitmap_size, MSB_FIRST);
    
    log_debug(logger, "Bitmap inicializado - Tamaño: %zu bytes, Total bloques: %d", 
             fs->bitmap_size, fs->total_blocks);

    free(bitmap_path);
    return true;
}

bool inicializar_blocks_index(t_filesystem* fs) {
    char* index_path = string_from_format("%s/blocks_hash_index.config", fs->root_path);
    if (obtener_fresh_start()) {
        // Crear archivo vacío
        FILE* file = fopen(index_path, "w");
        if (!file) {
            log_error(logger, "Error al crear blocks_hash_index.config");
            free(index_path);
            return false;
        }
        fclose(file);
    }

    // Verificar que el archivo existe
    FILE* test = fopen(index_path, "r");
    if (!test) {
        log_error(logger, "Error al verificar blocks_hash_index.config");
        free(index_path);
        return false;
    }
    fclose(test);

    log_debug(logger, "Blocks hash index inicializado");
    free(index_path);
    return true;
}

bool crear_directorios_base(t_filesystem* fs) {
    // Crear directorio physical_blocks
    char* physical_path = string_from_format("%s/physical_blocks", fs->root_path);
    if (mkdir(physical_path, 0755) == -1 && errno != EEXIST) {
        log_error(logger, "Error al crear directorio physical_blocks: %s", strerror(errno));
        free(physical_path);
        return false;
    }
    free(physical_path);

    // Crear directorio files
    char* files_path = string_from_format("%s/files", fs->root_path);
    if (mkdir(files_path, 0755) == -1 && errno != EEXIST) {
        log_error(logger, "Error al crear directorio files: %s", strerror(errno));
        free(files_path);
        return false;
    }
    free(files_path);

    log_debug(logger, "Directorios base creados");
    return true;
}

bool crear_bloques_fisicos(t_filesystem* fs) {
    int block_size = obtener_block_size();
    char* block_data = calloc(1, block_size); // Inicializado con ceros
    
    for (int i = 0; i < fs->total_blocks; i++) {
        char* block_path = obtener_path_bloque_fisico(fs, i);
        
        FILE* file = fopen(block_path, "wb");
        if (!file) {
            log_error(logger, "Error al crear bloque físico %d: %s", i, strerror(errno));
            free(block_path);
            free(block_data);
            return false;
        }

        fwrite(block_data, 1, block_size, file);
        fclose(file);
        free(block_path);
    }

    free(block_data);
    log_debug(logger, "Creados %d bloques físicos de %d bytes cada uno", fs->total_blocks, block_size);
    return true;
}

bool crear_initial_file(t_filesystem* fs) {
    // Crear directorio initial_file
    char* file_path = string_from_format("%s/files/initial_file", fs->root_path);
    if (mkdir(file_path, 0755) == -1 && errno != EEXIST) {
        log_error(logger, "Error al crear directorio initial_file: %s", strerror(errno));
        free(file_path);
        return false;
    }

    // Crear directorio BASE (tag)
    char* tag_path = string_from_format("%s/BASE", file_path);
    if (mkdir(tag_path, 0755) == -1 && errno != EEXIST) {
        log_error(logger, "Error al crear directorio BASE: %s", strerror(errno));
        free(file_path);
        free(tag_path);
        return false;
    }

    // Crear directorio logical_blocks
    char* logical_path = string_from_format("%s/logical_blocks", tag_path);
    if (mkdir(logical_path, 0755) == -1 && errno != EEXIST) {
        log_error(logger, "Error al crear directorio logical_blocks: %s", strerror(errno));
        free(file_path);
        free(tag_path);
        free(logical_path);
        return false;
    }

    // Marcar bloque 0 como ocupado
    marcar_bloque_ocupado(fs, 0);

    // Llenar bloque 0 con ceros (ya está hecho en crear_bloques_fisicos)
    int block_size = obtener_block_size();
    char* ceros = calloc(1, block_size);
    
    // Calcular hash del bloque 0
    char* hash = calcular_hash_bloque(ceros, block_size);
    guardar_hash_bloque(fs, 0, hash);

    // Crear metadata.config
    char* metadata_path = string_from_format("%s/metadata.config", tag_path);
    FILE* metadata_file = fopen(metadata_path, "w");
    if (!metadata_file) {
        log_error(logger, "Error al crear metadata.config: %s", strerror(errno));
        free(ceros);
        free(hash);
        free(file_path);
        free(tag_path);
        free(logical_path);
        free(metadata_path);
        return false;
    }

    fprintf(metadata_file, "TAMAÑO=%d\n", block_size);
    fprintf(metadata_file, "BLOCKS=[0]\n");
    fprintf(metadata_file, "ESTADO=COMMITED\n");
    fclose(metadata_file);

    // Crear hard link del bloque lógico 0 al bloque físico 0
    char* physical_block = obtener_path_bloque_fisico(fs, 0);
    char* logical_block = string_from_format("%s/000000.dat", logical_path);
    
    if (link(physical_block, logical_block) == -1) {
        log_error(logger, "Error al crear hard link para bloque lógico 0: %s", strerror(errno));
        free(ceros);
        free(hash);
        free(file_path);
        free(tag_path);
        free(logical_path);
        free(metadata_path);
        free(physical_block);
        free(logical_block);
        return false;
    }

    log_debug(logger, "Creado initial_file con tag BASE y bloque 0 asignado");

    // Limpiar memoria
    free(ceros);
    free(hash);
    free(file_path);
    free(tag_path);
    free(logical_path);
    free(metadata_path);
    free(physical_block);
    free(logical_block);

    return true;
}

// ==================== FUNCIONES DE MANEJO DE BLOQUES ====================
/*
int obtener_bloque_libre(t_filesystem* fs) {
    pthread_mutex_lock(&mutex_bitmap);

    for (int i = 0; i < fs->total_blocks; i++) {
        if (!bitarray_test_bit(fs->bitmap, i)) {
            pthread_mutex_unlock(&mutex_bitmap);
            return i;
        }
    }

    pthread_mutex_unlock(&mutex_bitmap);
    return -1; // No hay bloques libres
}
*/

int obtener_bloque_libre(t_filesystem* fs) {
    pthread_mutex_lock(&mutex_bitmap);

    for (int i = 0; i < fs->total_blocks; i++) {
        if (!bitarray_test_bit(fs->bitmap, i)) {
            // Marcar inmediatamente como ocupado ANTES de liberar el mutex
            bitarray_set_bit(fs->bitmap, i);
            msync(fs->bitmap_data, fs->bitmap_size, MS_SYNC);
            
            pthread_mutex_unlock(&mutex_bitmap);
            log_debug(logger, "Bloque %d asignado y marcado como ocupado", i);
            return i;
        }
    }

    pthread_mutex_unlock(&mutex_bitmap);
    log_error(logger, "No hay bloques libres disponibles");
    return -1;
}

void marcar_bloque_ocupado(t_filesystem* fs, int block_num) {
    pthread_mutex_lock(&mutex_bitmap);
    if (block_num >= 0 && block_num < fs->total_blocks) {

        bitarray_set_bit(fs->bitmap, block_num); // Pone en 1 el bit correspondiente al bloque block_num
        msync(fs->bitmap_data, fs->bitmap_size, MS_SYNC); // Fuerza que los cambios en memoria se escriban al disco

        log_debug(logger, "Bloque %d marcado como ocupado", block_num);
    }
    pthread_mutex_unlock(&mutex_bitmap);
}

void marcar_bloque_libre(t_filesystem* fs, int block_num) {
    pthread_mutex_lock(&mutex_bitmap);
    if (block_num >= 0 && block_num < fs->total_blocks) {

        bitarray_clean_bit(fs->bitmap, block_num);
        msync(fs->bitmap_data, fs->bitmap_size, MS_SYNC);

        log_debug(logger, "Bloque %d marcado como libre", block_num);
    }
    pthread_mutex_unlock(&mutex_bitmap);
}

bool esta_bloque_ocupado(t_filesystem* fs, int block_num) {
    if (block_num >= 0 && block_num < fs->total_blocks) {
        return bitarray_test_bit(fs->bitmap, block_num);
    }
    return false;
}

void* leer_bloque(t_filesystem* filesystem, char* nombre_file, char* nombre_tag, int bloque_logico){
    pthread_mutex_t* lock = obtener_lock_file_tag(nombre_file, nombre_tag);
    pthread_mutex_lock(lock);

    char* tag_path = obtener_path_tag(filesystem, nombre_file, nombre_tag);
    char* metadata_path = string_from_format("%s/metadata.config", tag_path);

    error_read = 0;

    if (tag_path == NULL) {
        free(tag_path);
        free(metadata_path);
        error_read = 2;
        pthread_mutex_unlock(lock);
        return NULL;
    }
    

    t_file_metadata* metadata = leer_metadata_file(metadata_path);
    if(!metadata){
        log_error(logger, "No se pudo leer el metadata de %s:%s", nombre_file, nombre_tag);
        free(tag_path);
        free(metadata_path);
        error_read = 1;
        pthread_mutex_unlock(lock);
        return NULL;
    }

    // Verificar que el bloque lógico esté asignado
    if (bloque_logico >= (int)metadata->blocks_count) {
        log_error(logger, "Bloque lógico %d fuera de rango (bloques asignados: %zu)", 
                  bloque_logico, metadata->blocks_count);
        liberar_metadata(metadata);
        free(tag_path);
        free(metadata_path);
        error_read = 3;
        pthread_mutex_unlock(lock);
        return NULL;
    }

    // Obtener número de bloque físico desde el metadata
    int bloque_fisico = metadata->blocks[bloque_logico];

    log_debug(logger, "Bloque lógico %d → bloque físico %d", bloque_logico, bloque_fisico);

    // Obtener path del bloque físico para leer el contenido
    char* path_bloque_fisico = obtener_path_bloque_fisico(filesystem, bloque_fisico);
    
    // Leer contenido del bloque físico
    FILE* file = fopen(path_bloque_fisico, "rb");
    if (!file) {
        log_error(logger, "Error al abrir bloque físico %d para lectura: %s", bloque_fisico, strerror(errno));
        free(path_bloque_fisico);
        liberar_metadata(metadata);
        free(tag_path);
        free(metadata_path);
        error_read = 1;
        pthread_mutex_unlock(lock);
        return NULL;
    }

    // Leer todo el bloque
    int block_size = obtener_block_size();
    void* contenido = malloc(block_size);
    size_t bytes_leidos = fread(contenido, 1, block_size, file);
    fclose(file);

    // Validar que se haya leído todo el bloque
    if (bytes_leidos != (size_t)block_size) {
        log_error(logger, "Error: solo se leyeron %zu de %d bytes del bloque físico %d", bytes_leidos, block_size, bloque_fisico);
        free(contenido);
        free(path_bloque_fisico);
        liberar_metadata(metadata);
        free(tag_path);
        free(metadata_path);
        error_read = 1;
        pthread_mutex_unlock(lock);
        return NULL;
    } 
    
    free(path_bloque_fisico);
    liberar_metadata(metadata);
    free(tag_path);
    free(metadata_path);
    

    pthread_mutex_unlock(lock);
    return contenido;

}

int contar_referencias_bloque_fisico(const char* physical_block_path) {
    struct stat st;
    
    // Obtener información del archivo
    if (stat(physical_block_path, &st) == -1) {
        return 0; // Error
    }
    
    // st_nlink = cantidad de hard links que apuntan a este archivo
    return st.st_nlink;
}

int escritura_bloque(char* path_bloque_fisico, int bloque_fisico, t_file_metadata* metadata, char* tag_path, char* metadata_path, void* contenido){
    FILE* file = fopen(path_bloque_fisico, "r+b");

    if (!file) {
        log_error(logger, "Error al abrir bloque físico %d para escritura: %s", bloque_fisico, strerror(errno));
        free(path_bloque_fisico);
        liberar_metadata(metadata);
        free(tag_path);
        free(metadata_path);
        return -1;
    }

    int block_size = obtener_block_size();
    fwrite(contenido, 1, block_size, file);
    log_debug(logger, "se escribe el bloque");
    fclose(file);

    return 1;
}

int escribir_bloque(t_filesystem* filesystem, char* nombre_file, char* nombre_tag, int bloque_logico, void* contenido, int tamanio){
    pthread_mutex_t* lock = obtener_lock_file_tag(nombre_file, nombre_tag);
    pthread_mutex_lock(lock);

    char* tag_path = obtener_path_tag(filesystem, nombre_file, nombre_tag);
    char* metadata_path = string_from_format("%s/metadata.config", tag_path);

    t_file_metadata* metadata = leer_metadata_file(metadata_path);
    if(!metadata){
        log_error(logger, "No se pudo leer el metadata de %s:%s", nombre_file, nombre_tag);
        free(tag_path);
        free(metadata_path);
        pthread_mutex_unlock(lock);
        return -1;
    }

    if(metadata->size < tamanio){
        free(tag_path);
        free(metadata_path);
        pthread_mutex_unlock(lock);
        return 2;
    }

    // Verificar que NO esté en COMMITED
    if (metadata->state == COMMITED){
        liberar_metadata(metadata);
        free(tag_path);
        free(metadata_path);
        pthread_mutex_unlock(lock);
        return 3;
    }

    // Verificar que el bloque lógico esté asignado
    if (bloque_logico >= (int)metadata->blocks_count) {
        log_error(logger, "Bloque lógico %d fuera de rango (bloques asignados: %zu)", bloque_logico, metadata->blocks_count);
        liberar_metadata(metadata);
        free(tag_path);
        free(metadata_path);
        pthread_mutex_unlock(lock);
        return -1;
    }

    int bloque_fisico = metadata->blocks[bloque_logico];

    log_debug(logger, "Bloque lógico %d → bloque físico %d", bloque_logico, bloque_fisico);

    char* path_bloque_fisico = obtener_path_bloque_fisico(filesystem, bloque_fisico);
    int cantidad_hard_links = contar_referencias_bloque_fisico(path_bloque_fisico);

    log_debug(logger, "Bloque físico %d con %d referencias", bloque_fisico, cantidad_hard_links);

    if (cantidad_hard_links == 2){
        int resultado = escritura_bloque(path_bloque_fisico, bloque_fisico, metadata, tag_path, metadata_path, contenido);
        if (resultado == -1) return false;
    } else {
        int bloque_nuevo = obtener_bloque_libre(filesystem);

        log_debug(logger, "bloque físico libre: %d", bloque_nuevo);

        if (bloque_nuevo == -1){
            free(path_bloque_fisico);
            liberar_metadata(metadata);
            free(tag_path);
            free(metadata_path);
            pthread_mutex_unlock(lock);
            return 4;
        }

        char* path_bloque_nuevo = obtener_path_bloque_fisico(filesystem, bloque_nuevo);
        log_debug(logger,"path bloque físico nuevo %s", path_bloque_nuevo);

        int resultado = escritura_bloque(path_bloque_nuevo, bloque_nuevo, metadata, tag_path, metadata_path, contenido);
        log_debug(logger,"se escribe en bloque %s %d", path_bloque_nuevo, bloque_nuevo);
        if (resultado == -1) return false;

        char* path_bloque_logico = obtener_path_bloque_logico(tag_path, bloque_logico);
        log_debug(logger,"path bloque logico %s", path_bloque_logico);

        if(unlink(path_bloque_logico) == -1){
            log_error(logger, "Error al eliminar hard link: %s", strerror(errno));
            free(path_bloque_nuevo);
            free(path_bloque_fisico);
            free(path_bloque_logico);
            liberar_metadata(metadata);
            free(tag_path);
            free(metadata_path);
            pthread_mutex_unlock(lock);
            return -1;
        }

        if (link(path_bloque_nuevo, path_bloque_logico) == -1){
            log_error(logger, "Error al crear nuevo hard link: %s", strerror(errno));
            free(path_bloque_nuevo);
            free(path_bloque_fisico);
            free(path_bloque_logico);
            liberar_metadata(metadata);
            free(tag_path);
            free(metadata_path);
            pthread_mutex_unlock(lock);
            return -1;
        }

        metadata->blocks[bloque_logico] = bloque_nuevo;
        log_debug(logger, "marcado como ocupado");
        //marcar_bloque_ocupado(filesystem, bloque_nuevo);

        free(path_bloque_nuevo);
        free(path_bloque_logico);
    }

    if(!guardar_metadata_file(metadata_path, metadata)){
        log_error(logger, "Error al guardar metadata");
        free(path_bloque_fisico);
        liberar_metadata(metadata);
        free(tag_path);
        free(metadata_path);
        pthread_mutex_unlock(lock);
        return -1;
    }

    free(path_bloque_fisico);
    liberar_metadata(metadata);
    free(tag_path);
    free(metadata_path);


    pthread_mutex_unlock(lock);
    return 1;
}



// ==================== FUNCIONES DE HASH ====================

char* calcular_hash_bloque(void* data, size_t size) {
    return crypto_md5(data, size);
}

int buscar_bloque_por_hash(t_filesystem* fs, const char* hash) {
    pthread_mutex_lock(&mutex_hash_index);

    char* index_path = string_from_format("%s/blocks_hash_index.config", fs->root_path);
    FILE* file = fopen(index_path, "r");
    
    if (!file) {
        free(index_path);
        pthread_mutex_unlock(&mutex_hash_index);
        return -1;
    }

    char line[256];
    int block_num = -1;
    
    while (fgets(line, sizeof(line), file)) {
        // Formato: hash=blockXXXX
        char* equals = strchr(line, '=');
        if (equals) {
            *equals = '\0';
            if (strcmp(line, hash) == 0) {
                sscanf(equals + 1, "block%d", &block_num);
                break;
            }
        }
    }
    
    fclose(file);
    free(index_path);

    pthread_mutex_unlock(&mutex_hash_index);
    return block_num;
}

bool guardar_hash_bloque(t_filesystem* fs, int block_num, const char* hash) {
    pthread_mutex_lock(&mutex_hash_index);

    char* index_path = string_from_format("%s/blocks_hash_index.config", fs->root_path);
    
    // Abrir en modo append
    FILE* file = fopen(index_path, "a");
    if (!file) {
        log_error(logger, "Error al abrir blocks_hash_index.config");
        free(index_path);
        pthread_mutex_unlock(&mutex_hash_index);
        return false;
    }
    
    fprintf(file, "%s=block%04d\n", hash, block_num);
    fclose(file);
    
    free(index_path);

    pthread_mutex_unlock(&mutex_hash_index);
    log_debug(logger, "Hash guardado para bloque %d: %s", block_num, hash);
    return true;
}

// ==================== FUNCIONES DE PATHS ====================

char* obtener_path_file(t_filesystem* fs, const char* nombre_file) {
    return string_from_format("%s/files/%s", fs->root_path, nombre_file);
}

char* obtener_path_tag(t_filesystem* fs, const char* nombre_file, const char* nombre_tag) {
    return string_from_format("%s/files/%s/%s", fs->root_path, nombre_file, nombre_tag);
}

char* obtener_path_bloque_fisico(t_filesystem* fs, int block_num) {
    return string_from_format("%s/physical_blocks/block%04d.dat", fs->root_path, block_num);
}

char* obtener_path_bloque_logico(const char* path_tag, int logical_block_num) {
    return string_from_format("%s/logical_blocks/%06d.dat", path_tag, logical_block_num);
}

// ==================== FUNCIONES DE FILES ====================
bool crear_file(t_filesystem* fs, const char* nombre_file, const char* nombre_tag) {
    if (!fs || !nombre_file || !nombre_tag) {
        log_error(logger, "crear_file() recibió parámetros NULL");
        return false;
    }

    pthread_mutex_t* lock = obtener_lock_file_tag(nombre_file, nombre_tag);
    pthread_mutex_lock(lock);
    
    // Crear directorio del File
    char* file_path = obtener_path_file(fs, nombre_file);
    if (!file_path) {
        log_error(logger, "Error obteniendo path del file");
        return false;
    }
    
    if (mkdir(file_path, 0755) == -1) {
        if (errno != EEXIST) {
            log_error(logger, "Error al crear directorio para File %s: %s", nombre_file, strerror(errno));
            free(file_path);
            pthread_mutex_unlock(lock);
            return false;
        }
        log_debug(logger, "Directorio %s ya existe", file_path);
    }

    // Crear Tag inicial
    bool result = crear_tag(fs, nombre_file, nombre_tag);
    
    free(file_path);

    pthread_mutex_unlock(lock);
    return result;
}

bool crear_tag(t_filesystem* fs, const char* nombre_file, const char* nombre_tag) {
    if (!fs || !nombre_file || !nombre_tag) {
        log_error(logger, "crear_tag() recibió parámetros NULL");
        return false;
    }
    
    // Crear directorio del Tag
    char* tag_path = obtener_path_tag(fs, nombre_file, nombre_tag);
    if (mkdir(tag_path, 0755) == -1) {
        if (errno != EEXIST) {
            log_error(logger, "Error al crear directorio para Tag %s: %s", nombre_tag, strerror(errno));
            free(tag_path);
            return false;
        }
        log_error(logger, "File:Tag preexistente");
        free(tag_path);
        return false;
    }

    // Crear directorio logical_blocks
    char* logical_path = string_from_format("%s/logical_blocks", tag_path);
    if (mkdir(logical_path, 0755) == -1 && errno != EEXIST) {
        log_error(logger, "Error al crear directorio logical_blocks: %s", strerror(errno));
        free(tag_path);
        free(logical_path);
        return false;
    }

    // Crear metadata.config inicial (tamaño 0, sin bloques)
    char* metadata_path = string_from_format("%s/metadata.config", tag_path);
    FILE* metadata_file = fopen(metadata_path, "w");
    if (!metadata_file) {
        log_error(logger, "Error al crear metadata.config: %s", strerror(errno));
        free(tag_path);
        free(logical_path);
        free(metadata_path);
        return false;
    }

    fprintf(metadata_file, "TAMAÑO=0\n");
    fprintf(metadata_file, "BLOCKS=[]\n");
    fprintf(metadata_file, "ESTADO=WORK_IN_PROGRESS\n");
    fclose(metadata_file);

    log_debug(logger, "Tag creado %s:%s", nombre_tag, nombre_file);

    free(tag_path);
    free(logical_path);
    free(metadata_path);
    return true;
}

int eliminar_file(t_filesystem* filesystem, char* nombre_file, char* nombre_tag, int query_id){
    pthread_mutex_t* lock = obtener_lock_file_tag(nombre_file, nombre_tag);
    pthread_mutex_lock(lock);

    //char* file_path = obtener_path_file(filesystem, nombre_file);
    char* tag_path = obtener_path_tag(filesystem,nombre_file,nombre_tag);
    if (!tag_path) {
        log_error(logger, "Error obteniendo path del tag");
        free(tag_path);
        pthread_mutex_unlock(lock);
        return 2;
    }

    char* metadata_path = string_from_format("%s/metadata.config", tag_path);
    t_file_metadata* metadata = leer_metadata_file(metadata_path);
    if(!metadata){
        log_error(logger, "No se pudo leer el metadata de %s:%s", nombre_file, nombre_tag);
        free(tag_path);
        free(metadata_path);
        liberar_metadata(metadata);
        pthread_mutex_unlock(lock);
        return -1;
    }

    // Eliminar TODOS los hardlinks
    for(int i = 0; i < metadata->blocks_count; i++){
        int bloque_logico = i;
        int bloque_fisico = metadata->blocks[i];
        char* path_bloque_logico = obtener_path_bloque_logico(tag_path, bloque_logico);
        
        if(unlink(path_bloque_logico) == -1){
            log_error(logger, "Error al eliminar hard link del bloque: %d", bloque_logico);
            free(tag_path);
            free(metadata_path);
            liberar_metadata(metadata);
            free(path_bloque_logico);
            pthread_mutex_unlock(lock);
            return -1;
        }
        
        log_info(logger, "##%d - %s:%s Se eliminó el hard link del bloque lógico %d al bloque físico %d",query_id,nombre_file,nombre_tag,bloque_logico,bloque_fisico);

        char* path_bloque_fisico = obtener_path_bloque_fisico(filesystem, bloque_fisico);

        int referencias = contar_referencias_bloque_fisico(path_bloque_fisico);
        if (referencias == 1){
            marcar_bloque_libre(filesystem, bloque_fisico);
            log_info(logger, "##%d - Bloque Físico Liberado - Número de Bloque: %d", query_id, bloque_fisico);
        }
        free(path_bloque_fisico);
        free(path_bloque_logico);
    }

    if(unlink(metadata_path) == -1){
        log_error(logger, "Error al elimnar metadata.config");
        free(tag_path);
        free(metadata_path);
        liberar_metadata(metadata);
        pthread_mutex_unlock(lock);
        return -1;
    }

    char* path_logical_blocks = string_from_format("%s/logical_blocks", tag_path);
    if(rmdir(path_logical_blocks) == -1){
        log_error(logger, "Error al eliminar %s", path_logical_blocks);
        free(tag_path);
        free(metadata_path);
        liberar_metadata(metadata);
        free(path_logical_blocks);
        pthread_mutex_unlock(lock);
        return -1;
    }
    free(path_logical_blocks);
    
    if(rmdir(tag_path) == -1){
        log_error(logger, "Error al eliminar %s", tag_path);
        free(tag_path);
        free(metadata_path);
        liberar_metadata(metadata);
        pthread_mutex_unlock(lock);
        return -1;
    }
    free(tag_path);
    free(metadata_path);
    liberar_metadata(metadata);
    
    pthread_mutex_unlock(lock);
    return 1;
}

// ==================== FUNCIONES DE METADATA ====================

t_file_metadata* leer_metadata_file(const char* path_metadata) {
    t_config* config = config_create((char*)path_metadata);
    if (!config) {
        log_error(logger, "Error al leer metadata desde %s", path_metadata);
        return NULL;
    }

    t_file_metadata* metadata = malloc(sizeof(t_file_metadata));
    
    // Leer tamaño
    metadata->size = config_get_int_value(config, "TAMAÑO");
    
    // Leer estado y convertir a enum
    char* estado = config_get_string_value(config, "ESTADO");
    metadata->state = (strcmp(estado, "COMMITED") == 0) ? COMMITED : WORK_IN_PROGRESS;
    
    // Leer array de bloques
    if (config_has_property(config, "BLOCKS")) {
        char** blocks_array = config_get_array_value(config, "BLOCKS");
        
        // Contar cantidad de elementos
        int count = 0;
        while (blocks_array[count] != NULL) {
            count++;
        }
        
        metadata->blocks_count = count;
        
        if (count > 0) {
            metadata->blocks = malloc(sizeof(int) * count);
            for (int i = 0; i < count; i++) {
                metadata->blocks[i] = atoi(blocks_array[i]);
            }
        } else {
            metadata->blocks = NULL;
        }
        
        // Liberar array temporal
        for (int i = 0; blocks_array[i] != NULL; i++) {
            free(blocks_array[i]);
        }
        free(blocks_array);
    } else {
        metadata->blocks = NULL;
        metadata->blocks_count = 0;
    }
    
    config_destroy(config);
    
    log_debug(logger, "Metadata leída: tamaño=%zu, bloques=%zu, estado=%s", 
              metadata->size, metadata->blocks_count, 
              metadata->state == COMMITED ? "COMMITED" : "WORK_IN_PROGRESS");
    
    return metadata;
}

bool guardar_metadata_file(const char* path_metadata, t_file_metadata* metadata) {
    FILE* file = fopen(path_metadata, "w");
    if (!file) {
        log_error(logger, "Error al abrir archivo de metadata para escritura: %s", path_metadata);
        return false;
    }

    // Escribir tamaño
    fprintf(file, "TAMAÑO=%zu\n", metadata->size);
    
    // Escribir bloques
    fprintf(file, "BLOCKS=[");
    for (size_t i = 0; i < metadata->blocks_count; i++) {
        fprintf(file, "%d", metadata->blocks[i]);
        if (i < metadata->blocks_count - 1) {
            fprintf(file, ",");
        }
    }
    fprintf(file, "]\n");
    
    // Escribir estado
    fprintf(file, "ESTADO=%s\n", 
            metadata->state == COMMITED ? "COMMITED" : "WORK_IN_PROGRESS");
    
    log_debug(logger, "Metadata guardada: tamaño=%zu, bloques=%zu, estado=%s", 
              metadata->size, metadata->blocks_count, 
              metadata->state == COMMITED ? "COMMITED" : "WORK_IN_PROGRESS");

    fclose(file);
    return true;
}

void liberar_metadata(t_file_metadata* metadata) {
    if (metadata) {
        if (metadata->blocks) {
            free(metadata->blocks);
        }
        free(metadata);
    }
}

// ==================== FUNCIONES DE TAG ====================
int truncar_file(t_filesystem* filesystem, char* nombre_file, char* nombre_tag, int nuevo_tamanio, int query_id){
    pthread_mutex_t* lock = obtener_lock_file_tag(nombre_file, nombre_tag);
    pthread_mutex_lock(lock);

    char* tag_path = obtener_path_tag(filesystem, nombre_file, nombre_tag);

    if (access(tag_path, F_OK) != 0) {
        log_error(logger, "Tag %s:%s no existe", nombre_file, nombre_tag);
        free(tag_path);
        pthread_mutex_unlock(lock);
        return 4;
    }

    char* metadata_path = string_from_format("%s/metadata.config", tag_path);

    t_file_metadata* metadata = leer_metadata_file(metadata_path);
    if(!metadata){
        log_error(logger, "No se pudo leer el metadata de %s:%s", nombre_file, nombre_tag);
        free(tag_path);
        free(metadata_path);
        pthread_mutex_unlock(lock);
        return -1;
    }

    // Verificar que NO esté en COMMITED
    if (metadata->state == COMMITED){
        liberar_metadata(metadata);
        free(tag_path);
        free(metadata_path);
        pthread_mutex_unlock(lock);
        return 2;
    }

    int block_size = obtener_block_size();
    int bloques_necesarios = (nuevo_tamanio + block_size - 1) / block_size; // Redondeo hacia arriba
    int bloques_actuales = metadata->blocks_count;

    log_debug(logger, "Truncar: tamaño actual=%zu, nuevo=%d, bloques actuales=%d, bloques necesarios=%d",
              metadata->size, nuevo_tamanio, bloques_actuales, bloques_necesarios);

    if (bloques_necesarios > bloques_actuales) {
        // ========== CASO 1: AGRANDAR - Asignar más bloques ==========
        int bloques_a_agregar = bloques_necesarios - bloques_actuales;
        
        // Realocar el array de bloques
        int* nuevos_bloques = realloc(metadata->blocks, sizeof(int) * bloques_necesarios);
        if (!nuevos_bloques) {
            log_error(logger, "Error al realocar array de bloques");
            liberar_metadata(metadata);
            free(tag_path);
            free(metadata_path);
            pthread_mutex_unlock(lock);
            return -1;
        }

        metadata->blocks = nuevos_bloques;

        // Asignar nuevos bloques físicos
        for (int i = 0; i < bloques_a_agregar; i++) {
            int indice_bloque_logico = bloques_actuales + i;

            // Obtener paths
            char* path_bloque_fisico = obtener_path_bloque_fisico(filesystem, 0);
            char* path_bloque_logico = obtener_path_bloque_logico(tag_path, bloques_actuales + i);

            // Crear hard link
            if (link(path_bloque_fisico, path_bloque_logico) == -1) {

                if (errno == EEXIST) {
                    log_debug(logger, "Bloque lógico ya existe, eliminando y recreando...");
                    unlink(path_bloque_logico);
        
                    if (link(path_bloque_fisico, path_bloque_logico) == -1) {
                        log_error(logger, "Error al crear hard link después de eliminar: %s", strerror(errno));
                        free(path_bloque_fisico);
                        free(path_bloque_logico);
                        liberar_metadata(metadata);
                        free(tag_path);
                        free(metadata_path);
                        pthread_mutex_unlock(lock);
                        return -1;
                    }
                } else {
                    log_error(logger, "Error al crear hard link: %s", strerror(errno));
                    free(path_bloque_fisico);
                    free(path_bloque_logico);
                    liberar_metadata(metadata);
                    free(tag_path);
                    free(metadata_path);
                    pthread_mutex_unlock(lock);
                    return -1;
                }
            }           

            // Actualizar metadata
            metadata->blocks[indice_bloque_logico] = 0;

            log_info(logger, "##%d - %s:%s Se agregó el hard link del bloque lógico %d al bloque físico 0", query_id, nombre_file, nombre_tag, indice_bloque_logico);

            free(path_bloque_fisico);
            free(path_bloque_logico);
        }

        metadata->blocks_count = bloques_necesarios;

    } else if (bloques_necesarios < bloques_actuales) {
        // ========== CASO 2: ACHICAR - Liberar bloques sobrantes ==========

        for (int i = bloques_actuales - 1; i >= bloques_necesarios; i--) {
            int bloque_fisico = metadata->blocks[i];

            // Obtener paths
            char* path_bloque_logico = obtener_path_bloque_logico(tag_path, i);
            char* path_bloque_fisico = obtener_path_bloque_fisico(filesystem, bloque_fisico);

            // Verificar cuántas referencias tiene el bloque físico
            int referencias = contar_referencias_bloque_fisico(path_bloque_fisico);

            // Eliminar hard link del bloque lógico
            if (unlink(path_bloque_logico) == -1) {
                log_error(logger, "Error al eliminar hard link del bloque lógico %d: %s", i, strerror(errno));
                free(path_bloque_logico);
                free(path_bloque_fisico);
                continue;
            }

            // Si era la única referencia, liberar el bloque físico
            if (referencias == 1) {
                marcar_bloque_libre(filesystem, bloque_fisico);
                log_info(logger, "##%d - Bloque Físico Liberado - Número de Bloque: %d", query_id, bloque_fisico);
            }

            free(path_bloque_logico);
            free(path_bloque_fisico);
        }

        // Reducir el array de bloques
        if (bloques_necesarios > 0) {
            int* nuevos_bloques = realloc(metadata->blocks, sizeof(int) * bloques_necesarios);
            if (nuevos_bloques) {
                metadata->blocks = nuevos_bloques;
            }
        } else {
            free(metadata->blocks);
            metadata->blocks = NULL;
        }

        metadata->blocks_count = bloques_necesarios;
    }
    
    // Caso 3: bloques_necesarios == bloques_actuales → no hacer nada

    // Actualizar tamaño
    metadata->size = nuevo_tamanio;

    // Guardar metadata
    if (!guardar_metadata_file(metadata_path, metadata)) {
        log_error(logger, "Error al guardar metadata");
        liberar_metadata(metadata);
        free(tag_path);
        free(metadata_path);
        pthread_mutex_unlock(lock);
        return -1;
    }

    liberar_metadata(metadata);
    free(tag_path);
    free(metadata_path);
    pthread_mutex_unlock(lock);
    return 1;
}

bool commitear_tag(t_filesystem* filesystem, char* nombre_file, char* nombre_tag, int query_id){
    pthread_mutex_t* lock = obtener_lock_file_tag(nombre_file, nombre_tag);
    pthread_mutex_lock(lock);

    char* tag_path = obtener_path_tag(filesystem, nombre_file, nombre_tag);
    char* metadata_path = string_from_format("%s/metadata.config", tag_path);

    t_file_metadata* metadata = leer_metadata_file(metadata_path);
    if(!metadata){
        log_error(logger, "No se pudo leer el metadata de %s:%s", nombre_file, nombre_tag);
        free(tag_path);
        free(metadata_path);
        pthread_mutex_unlock(lock);
        return false;
    }

    if (metadata->state == COMMITED){
        log_debug(logger, "%s:%s ya está COMMITED", nombre_file, nombre_tag);
        liberar_metadata(metadata);
        free(tag_path);
        free(metadata_path);
        pthread_mutex_unlock(lock);
        return true; 
    }

    for (int i = 0; i < (int)metadata->blocks_count; i++){
        int bloque_fisico_actual = metadata->blocks[i];

        if (bloque_fisico_actual == 0) {
            log_debug(logger, "Bloque lógico %d ya apunta al bloque físico 0, skip deduplicación", i);
            continue;
        }

        log_debug(logger, "Procesando bloque lógico %d - bloque físico %d", i, bloque_fisico_actual);

        // Obtener path del bloque físico para leer el contenido
        char* path_bloque_fisico = obtener_path_bloque_fisico(filesystem, bloque_fisico_actual);
    
        // Leer contenido del bloque físico
        FILE* file = fopen(path_bloque_fisico, "rb");
        if (!file) {
            log_error(logger, "Error al abrir bloque físico %d para lectura: %s", bloque_fisico_actual, strerror(errno));
            free(path_bloque_fisico);
            continue;
        }

        // Leer todo el bloque
        int block_size = obtener_block_size();
        void* contenido = malloc(block_size);
        size_t bytes_leidos = fread(contenido, 1, block_size, file);
        fclose(file);

        // Validar que se haya leído todo el bloque
        if (bytes_leidos != (size_t)block_size) {
            log_error(logger, "Error: solo se leyeron %zu de %d bytes del bloque físico %d", 
                      bytes_leidos, block_size, bloque_fisico_actual);
            free(contenido);
            free(path_bloque_fisico);
            continue;
        }

        // Calcular hash del bloque
        char* hash = calcular_hash_bloque(contenido, block_size);
        free(contenido);  // Ya no necesitamos el contenido

        log_debug(logger, "Hash del bloque físico %d: %s", bloque_fisico_actual, hash);

        // Buscar si ya existe un bloque con este hash
        int bloque_encontrado = buscar_bloque_por_hash(filesystem, hash);

        if(bloque_encontrado != -1){
            // ========== CASO 1: Bloque duplicado encontrado ==========
            log_debug(logger, "Bloque duplicado encontrado: físico %d tiene mismo hash que físico %d", 
                      bloque_fisico_actual, bloque_encontrado);

            char* path_bloque_logico_actual = obtener_path_bloque_logico(tag_path, i);
            log_debug(logger, "Path bloque lógico a realizar hardlink: %s", path_bloque_logico_actual);

            // Eliminar el hard link actual
            if(unlink(path_bloque_logico_actual) == -1){
                log_error(logger, "Error al eliminar hard link: %s", strerror(errno));
                free(path_bloque_logico_actual);
                free(path_bloque_fisico);
                free(hash);
                continue;
            }

            log_info(logger, "##%d - %s:%s Se eliminó el hard link del bloque lógico %d al bloque físico %d", 
                     query_id, nombre_file, nombre_tag, i, bloque_fisico_actual);

            // Crear hard link al bloque pre-existente
            char* path_bloque_fisico_preexistente = obtener_path_bloque_fisico(filesystem, bloque_encontrado);
            log_debug(logger, "Path bloque físico a realizar hardlink: %s", path_bloque_fisico_preexistente);

            if (link(path_bloque_fisico_preexistente, path_bloque_logico_actual) == -1){
                log_error(logger, "Error al realizar hard link: %s", strerror(errno));
                free(path_bloque_fisico_preexistente);
                free(path_bloque_logico_actual);
                free(path_bloque_fisico);
                free(hash);
                continue;
            }

            log_info(logger, "##%d - %s:%s Se agregó el hard link del bloque lógico %d al bloque físico %d", 
                     query_id, nombre_file, nombre_tag, i, bloque_encontrado);

            // Actualizar metadata
            metadata->blocks[i] = bloque_encontrado;
            log_info(logger, "##%d - %s:%s Bloque Lógico %d se reasigna de %d a %d", 
                     query_id, nombre_file, nombre_tag, i, bloque_fisico_actual, bloque_encontrado);
              // Marcar el bloque como libre
            int referencias = contar_referencias_bloque_fisico(path_bloque_fisico);
            if(referencias == 1){
                marcar_bloque_libre(filesystem, bloque_fisico_actual);
                log_info(logger, "##%d - Bloque Físico Liberado - Número de Bloque: %d", query_id, bloque_fisico_actual);
            }
            free(path_bloque_fisico_preexistente);
            free(path_bloque_logico_actual);

        } else {
            // ========== CASO 2: Bloque nuevo (sin duplicados) ==========
            if(guardar_hash_bloque(filesystem, bloque_fisico_actual, hash)){
                log_info(logger, "##%d - Bloque Físico Reservado - Número de Bloque: %d", 
                         query_id, bloque_fisico_actual);
            } else {
                log_error(logger, "NO se pudo guardar en block_hash_index.config");
                free(path_bloque_fisico);
                free(hash);
                continue;
            }
        }

        // Liberar memoria de esta iteración
        free(path_bloque_fisico);
        free(hash);
    }

    // Cambiar estado a COMMITED
    metadata->state = COMMITED;

    // Guardar metadata actualizado
    if (!guardar_metadata_file(metadata_path, metadata)){
        log_error(logger, "NO se pudo guardar el metadata");
        liberar_metadata(metadata);
        free(tag_path);
        free(metadata_path);
        pthread_mutex_unlock(lock);
        return false;
    }

    // Limpiar memoria final
    liberar_metadata(metadata);
    free(tag_path);
    free(metadata_path);


    pthread_mutex_unlock(lock);
    return true;
}

int copiar_tag(t_filesystem* filesystem, char* nombre_file_origen, char* nombre_tag_origen, char* nombre_file_destino, char* nombre_tag_destino, int query_id){
    pthread_mutex_t* lock_origen = obtener_lock_file_tag(nombre_file_origen, nombre_tag_origen);
    pthread_mutex_lock(lock_origen);
    char* tag_origen_path = obtener_path_tag(filesystem, nombre_file_origen, nombre_tag_origen);

    if(!tag_origen_path){
        free(tag_origen_path);
        pthread_mutex_unlock(lock_origen);
        return 3;
    }
    
    char* metadata_origen_path = string_from_format("%s/metadata.config", tag_origen_path);
    t_file_metadata* metadata_origen = leer_metadata_file(metadata_origen_path);
    
    if(!metadata_origen){
        log_error(logger, "No se pudo leer el metadata de %s:%s", nombre_file_origen, nombre_tag_origen);
        free(tag_origen_path);
        free(metadata_origen_path);
        liberar_metadata(metadata_origen);
        pthread_mutex_unlock(lock_origen);
        return -1;
    }

    if(!crear_file(filesystem,nombre_file_destino,nombre_tag_destino)){
        free(tag_origen_path);
        free(metadata_origen_path);
        liberar_metadata(metadata_origen);
        pthread_mutex_unlock(lock_origen);
        return 2;
    }

    pthread_mutex_t* lock_destino = obtener_lock_file_tag(nombre_file_destino, nombre_tag_destino);
    pthread_mutex_lock(lock_destino);
    char* tag_destino_path = obtener_path_tag(filesystem, nombre_file_destino, nombre_tag_destino);
    char* metadata_destino_path = string_from_format("%s/metadata.config", tag_destino_path);
    t_file_metadata* metadata_destino = leer_metadata_file(metadata_destino_path);

    if(!metadata_destino){
        log_error(logger, "No se pudo leer el metadata de %s:%s", nombre_file_destino, nombre_tag_destino);
        free(tag_destino_path);
        free(metadata_destino_path);
        free(tag_origen_path);
        free(metadata_origen_path);
        liberar_metadata(metadata_origen);
        liberar_metadata(metadata_destino);
        pthread_mutex_unlock(lock_destino);
        pthread_mutex_unlock(lock_origen);
        return -1;
    }

    metadata_destino->size  = metadata_origen->size;
    metadata_destino->blocks_count = metadata_origen->blocks_count;

    if (metadata_destino->blocks_count > 0) {
        metadata_destino->blocks = malloc(sizeof(int) * metadata_destino->blocks_count);

        for (int i = 0; i < (int)metadata_destino->blocks_count; i++) {
            int bloque_fisico = metadata_origen->blocks[i];
            metadata_destino->blocks[i] = bloque_fisico;

            char* path_bloque_fisico = obtener_path_bloque_fisico(filesystem, bloque_fisico);
            char* path_bloque_logico_dest = obtener_path_bloque_logico(tag_destino_path, i);

            if (link(path_bloque_fisico, path_bloque_logico_dest) == -1) {
                log_error(logger,"Error creando hard link para %s:%s bloque lógico %d (físico %d): %s",nombre_file_destino, nombre_tag_destino, i, bloque_fisico, strerror(errno));
                free(path_bloque_fisico);
                free(path_bloque_logico_dest);
                free(tag_destino_path);
                free(metadata_destino_path);
                free(tag_origen_path);
                free(metadata_origen_path);
                liberar_metadata(metadata_origen);
                liberar_metadata(metadata_destino);
                pthread_mutex_unlock(lock_destino);
                pthread_mutex_unlock(lock_origen);
                return -1;
            }

            log_info(logger, "##%d - %s:%s Se agregó el hard link del bloque lógico %d al bloque físico %d", query_id, nombre_file_destino, nombre_tag_destino, i, bloque_fisico);
            free(path_bloque_fisico);
            free(path_bloque_logico_dest);
            
        }
    } else {
        metadata_destino->blocks = NULL;
    }

    free(tag_origen_path);
    free(metadata_origen_path);
    liberar_metadata(metadata_origen);

    if (!guardar_metadata_file(metadata_destino_path, metadata_destino)) {
            log_error(logger, "Error al guardar metadata del destino %s:%s",nombre_file_destino, nombre_tag_destino);
            free(tag_destino_path);
            free(metadata_destino_path);
            liberar_metadata(metadata_destino);
            pthread_mutex_unlock(lock_destino);
            pthread_mutex_unlock(lock_origen);
            return -1;
    } else {
            log_debug(logger,"##%d - TAG copiado de %s:%s a %s:%s (size=%zu, blocks=%zu)",query_id,nombre_file_origen, nombre_tag_origen,nombre_file_destino, nombre_tag_destino,
            metadata_destino->size, metadata_destino->blocks_count);
    }
    
    free(tag_destino_path);
    free(metadata_destino_path);
    liberar_metadata(metadata_destino);
    pthread_mutex_unlock(lock_destino);
    pthread_mutex_unlock(lock_origen);

    return 1;
}


