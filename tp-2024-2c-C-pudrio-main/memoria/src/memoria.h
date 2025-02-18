#ifndef MEMORIA_H_
#define MEMORIA_H_
#include <utils/utils.h>

// declaro como variables globales todos los datos que vienen del config
// todas las variables globlaes las pongo en mayuscula para mostrar que son variables globales
char *IP_FILESYSTEM;
char *FILESYSTEM_PORT;
char *LISTENING_PORT;
int MEMORY_SIZE;
char *PATH_INSTRUCTIONS;
int RESPONSE_DELAY;
char *SCHEME;
char *SEARCH_ALGORITHM;
char **PARTITIONS;
char *LOG_LEVEL;
void *fd_socket;
void *MEMORIA;
t_list *particiones;
t_list *lista_tcbs;

// hago variables globales al logger usado, al path donde se encuentra el config y una
// mas para el config propiamente dicho
t_log *LOGGER;
t_config *MODULE_CONFIG;
char *MODULE_CONFIG_PATH = "memoria.config";
char *LOGGER_PATH = "../memoria.log";
char *MODULE_NAME = "memoria";

typedef struct
{
    uint32_t PID;
    int base;
    int tamanio;
    int limite;
    bool ocupado;
} particion_memoria;

typedef struct
{
    uint32_t TID;
    uint32_t PID; // el identificador del proceso ASOCIADO
    uint32_t AX;
    uint32_t BX;
    uint32_t CX;
    uint32_t DX;
    uint32_t EX;
    uint32_t FX;
    uint32_t GX;
    uint32_t HX;
    uint32_t PC;
    t_list *lista_instrucciones;
    uint32_t base;
    uint32_t limite;
} tcb_memoria;

void read_config(t_config *MODULE_CONFIG);
t_list *esquema_particiones();
void inicializar_server_memoria(info_conexion_servidor *info);
void atender_cliente_memoria(int *fd_conexion);
void inicializar_cliente_memoria(info_conexion *info);
t_list *esquema_particiones();
bool esta_libre(particion_memoria *particion);
bool particion_menor(particion_memoria *a, particion_memoria *b);
bool particion_mayor(particion_memoria *a, particion_memoria *b);
particion_memoria *first_fit(int tamanio_a_asignar, t_list *particiones);
particion_memoria *best_fit(int tamanio_a_asignar, t_list *particiones);
particion_memoria *asignar_memoria(t_list *particiones, int tamanio_proceso, uint32_t PID);
void leer_instrucciones(FILE *archivo, t_list *lista);
void imprimir_instrucciones(t_list *lista, size_t size);
void procesar_solicitar_instruccion(int *fd_conexion);
int procesar_process_create(int *fd_conexion);
int procesar_guardar_contexto(int *fd_conexion);
void procesar_solicitar_contexto(int *fd_conexion);
int procesar_crear_hilo(int *fd_conexion);
int procesar_eliminar_hilo(int *fd_conexion);
int procesar_dump(int *fd_conexion);
void procesar_eliminar_proceso(int *fd_conexion);
void compactar_particiones_libres();
int procesar_escribir_memoria(int *fd_conexion);
void procesar_leer_memoria(int *fd_conexion);
void procesar_solicitar_base(int *fd_conexion);

#endif