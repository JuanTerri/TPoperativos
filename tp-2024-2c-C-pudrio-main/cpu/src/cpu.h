#ifndef CPU_H_
#define CPU_H_

#include <utils/utils.h>

char *IP_MEMORIA;
char *MEMORY_PORT;
char *LISTENER_PORT_DISPATCH;
char *LISTENER_PORT_INTERRUPT;
char *LOG_LEVEL;

t_config *MODULE_CONFIG;
t_log *LOGGER;
t_log *cpu_logger;
t_log *cpu_logger_extra;

char *MODULE_CONFIG_PATH = "cpu.config";
char *LOGGER_PATH = "../cpu.log";
char *MODULE_NAME = "cpu";
void *FD_SOCKET;

sem_t sem_pedido_instruccion;
sem_t sem_solicitud_lectura;
sem_t sem_solicitud_escritura;

int FD_SOCKET_MEMORIA;
int FD_SOCKET_K_DISPATCH;
int FD_SOCKET_K_INTERRUPT;

char *respuesta_escritura;
char *respuesta_lectura;

bool hayQueDesalojar;
bool hay_interrupcion_quantum;

pthread_mutex_t mutex_hay_interrupcion = PTHREAD_MUTEX_INITIALIZER;

char *instruccion_dividida[4];

typedef struct
{
    uint32_t TID;
    uint32_t PID;
    uint32_t PC;
    uint32_t AX;
    uint32_t BX;
    uint32_t CX;
    uint32_t DX;
    uint32_t EX;
    uint32_t FX;
    uint32_t GX;
    uint32_t HX;
} tcb_cpu;

tcb_cpu *contexto;

void atender_hilo_cpu_kInterrupt();
void atender_hilo_cpu_kDispatch();
void atender_hilo_cpu_memoria();

// memoria
void enviar_contexto_Memoria(t_paquete *paquete);
void escribir_en_memoria(int direccion_logica, uint32_t valor_a_escribir, int base_particion, int tamanio_particion);
uint32_t leer_de_memoria(int direccion_logica, int tamanio, int base_particion, int tamanio_particion);

// kernel
void enviar_pid_tid_Kernel(t_paquete *paquete);
void recibir_tid_pid_kernel(int FD_SOCKET_K_DISPATCH);

// mmu
int traducir_direccion(int direccion_logica, int base_particion, int tamanio_particion);

void read_config(t_config *MODULE_CONFIG);
void inicializar_variables();
void iniciar_cpu();
void iniciar_semaforos();
char *recibir_instruccion(int FD_SOCKET_MEMORIA);
void liberar_instruccion_dividida();
uint32_t *detectar_registro(char *registro);
void inicializar_varialbles();

void recibir_pid_tid_kernel(int FD_SOCKET_K_DISPATCH);
void solicitar_contexto_mem();
void atender_lectura(int FD_SOCKET_MEMORIA);
void atender_interrupcion_quantum();
void cicloDeInstruccion();
void mostrar_contexto();
void fetch();
void decodeYExecute(char *instruccion, int *while_ciclo);

int crear_conexion(char *ip, char *puerto);

void crearServidorMemoria(char *puerto_escucha);
int esperar_cliente(int socket_servidor);
int iniciar_servidor(char *puerto, t_log *logger);
void atender_mensajes_kernel(int socket_cliente);
void atender_mensajes_memoria(int socket_cliente);
void *crearServidorKernel(void *arg);
void crearServidorMemoria(char *puerto_escucha);
void atender_mensajes_kernel_interrupt(int socket_cliente);
#endif