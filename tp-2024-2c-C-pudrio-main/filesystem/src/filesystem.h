#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_
#include <utils/utils.h>

#include <commons/bitarray.h>
#include <fcntl.h>    // Para O_RDWR, O_CREAT
#include <sys/mman.h> // Para mmap, PROT_READ, PROT_WRITE, MAP_SHARED, MAP_FAILED
#include <sys/stat.h> // Para S_IRUSR, S_IWUSR
#include <unistd.h>   // Para close, ftruncate, access
#include <dirent.h>   // Para opendir, readdir, closedir, DT_REG
#include <commons/string.h>

// declaro como variables globales todos los datos que vienen del config
// todas las variables globlaes las pongo en mayuscula para mostrar que son variables globales
char *LISTENING_PORT;
char *MOUNT_DIR;
int BLOCK_SIZE;
int BLOCK_COUNT;
int BLOCK_ACCESS_DELAY;
char *LOG_LEVEL;

// hago variables globales al logger usado, al path donde se encuentra el config y una
// mas para el config propiamente dicho
t_log *LOGGER;
t_config *MODULE_CONFIG;
char *MODULE_CONFIG_PATH = "filesystem.config";
char *LOGGER_PATH = "../filesystem.log";
char *MODULE_NAME = "filesystem";

struct bitmap
{
    char *direccion;
    uint32_t tamanio;
    t_bitarray *bitarray;
};
typedef struct bitmap t_bitmap;

char *block_path;
char *metadata_path;
char *bitmap_path;

FILE *archivoDeBloques;
t_bitmap *bitmap;
void *bloques_datos_addr;
char *puntero_auxiliar_mmap;
char *FILE_NAME = "bloques.dat";
void inicializar_server_filesystem(info_conexion_servidor *info);
void atender_cliente_filesystem(int *fd_conexion);
char *crear_metadata_path(char *nombreArchivo);
void crear_archivo_de_bloques();
void crear_bitmap();
void atender_peticion(int *fd_conexion);
int contar_bloques_libres();
int reservar_bloque();
int crear_directorio(const char *path);
char *generar_nombre_archivo(uint32_t pid, uint32_t tid);
int dump_memory(uint32_t pid, uint32_t tid, int tamanio_archivo, char *contenido);
void escribir_en_bloques(const char *contenido, int tamanio_contenido, int bloque_inicial);

#endif
