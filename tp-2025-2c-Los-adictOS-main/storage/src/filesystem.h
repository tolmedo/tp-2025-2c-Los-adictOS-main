#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <commons/bitarray.h>
#include <commons/config.h>
#include <pthread.h>

// Estructura para manejar el filesystem
typedef struct {
    t_bitarray* bitmap;         // Bitmap de bloques libres/ocupados
    void* bitmap_data;          // Datos del bitmap mapeados
    int bitmap_fd;              // File descriptor del bitmap
    size_t bitmap_size;         // Tamaño del bitmap en bytes
    char* root_path;            // Path raíz del filesystem
    int total_blocks;           // Cantidad total de bloques
} t_filesystem;

// Estado de un File/Tag
typedef enum {
    WORK_IN_PROGRESS,
    COMMITED
} t_file_state;

// Metadata de un File/Tag
typedef struct {
    size_t size;                // Tamaño del archivo
    t_file_state state;         // Estado del archivo
    int* blocks;                // Array de números de bloques físicos
    size_t blocks_count;        // Cantidad de bloques
} t_file_metadata;

typedef struct {
    pthread_mutex_t mutex;
    char* key; // "NOMBRE_FILE:TAG"
} t_file_tag_lock;

// Funciones principales del filesystem
t_filesystem* inicializar_filesystem(void);
void destruir_filesystem(t_filesystem* fs);

// Funciones de inicialización
bool crear_estructura_filesystem(t_filesystem* fs);
bool inicializar_bitmap(t_filesystem* fs);
bool inicializar_blocks_index(t_filesystem* fs);
bool crear_directorios_base(t_filesystem* fs);
bool crear_bloques_fisicos(t_filesystem* fs);
bool crear_initial_file(t_filesystem* fs);
bool formatear_filesystem(t_filesystem* fs);

// Funciones auxiliares
int obtener_bloque_libre(t_filesystem* fs);
void marcar_bloque_ocupado(t_filesystem* fs, int block_num);
void marcar_bloque_libre(t_filesystem* fs, int block_num);
bool esta_bloque_ocupado(t_filesystem* fs, int block_num);
char* calcular_hash_bloque(void* data, size_t size);
int buscar_bloque_por_hash(t_filesystem* fs, const char* hash);
bool guardar_hash_bloque(t_filesystem* fs, int block_num, const char* hash);

// Funciones para manejar Files
bool crear_file(t_filesystem* fs, const char* nombre_file, const char* nombre_tag);
bool crear_tag(t_filesystem* fs, const char* nombre_file, const char* nombre_tag);
t_file_metadata* leer_metadata_file(const char* path_metadata);
bool guardar_metadata_file(const char* path_metadata, t_file_metadata* metadata);
void liberar_metadata(t_file_metadata* metadata);

// Funciones para paths
char* obtener_path_file(t_filesystem* fs, const char* nombre_file);
char* obtener_path_tag(t_filesystem* fs, const char* nombre_file, const char* nombre_tag);
char* obtener_path_bloque_fisico(t_filesystem* fs, int block_num);
char* obtener_path_bloque_logico(const char* path_tag, int logical_block_num);

// Funciones para Operaciones de Storage
void* leer_bloque(t_filesystem* filesystem, char* nombre_file, char* nombre_tag, int bloque_logico);
int escribir_bloque(t_filesystem* filesystem, char* nombre_file, char* nombre_tag, int bloque_logico, void* contenido, int tamanio);
bool commitear_tag(t_filesystem* filesystem, char* nombre_file, char* nombre_tag, int query_id);
int truncar_file(t_filesystem* filesystem, char* nombre_file, char* nombre_tag, int nuevo_tamanio, int query_id);
int eliminar_file(t_filesystem* filesystem, char* nombre_file, char* nombre_tag, int query_id);
int copiar_tag(t_filesystem* filesystem, char* nombre_file_origen, char* nombre_tag_origen, char* nombre_file_destino, char* nombre_tag_destino, int query_id);

#endif // FILESYSTEM_H