#ifndef KERNEL_H_
#define KERNEL_H_
#include <utils/utils.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


// Códigos de colores ANSI
#define RESET "\x1b[0m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN "\x1b[36m"
#define WHITE "\x1b[37m"

typedef struct
{
    uint32_t TID;
    uint32_t prioridad;
} valorHilo;

// struct para el tcb y tener la posicion de ese tcb en una lista
typedef struct
{
    tcb *tcb;
    int posicion;
} struct_hilo_posicion;

typedef struct
{
    int PID;
    char *nombre;
    int valor;
    t_list *lista_hilos;
    int TID;
} t_mutex;

typedef struct
{
    int TID;
    int PID;
    char *mutex;
} mutex_pcb;

typedef struct
{
    int TID;
    int PID;
} tid_pid_t;

typedef struct
{
    tcb* hilo;
    int milisegundos;
} hiloMiliseg;


char *IP_MEMORIA;
char *MEMORY_PORT;
char *IP_CPU;
char *CPU_PORT_DISPATCH;
char *CPU_PORT_INTERRUPT;
char *PLANIFICATION_ALGORITHM;
int QUANTUM;
char *LOG_LEVEL;

t_config *MODULE_CONFIG;
t_log *LOGGER;
char *MODULE_CONFIG_PATH = "kernel.config";
char *LOGGER_PATH = "../kernel.log";
char *MODULE_NAME = "kernel";
char *ARCHIVO;
char *TAMANIO;

int FD_SOCKET_MEMORIA;
int FD_SOCKET_CPU_DISPATCH;
int FD_SOCKET_CPU_INTERRUPT;

t_list *lista_new;
t_list *lista_ready;
t_list *lista_exec;
t_list *lista_blocked;
t_list *lista_exit;

t_list *lista_pcbs;

t_queue *cola_procesos;
uint32_t contador_procesos = 0;
int contador_mutex_id = 0;

sem_t semaforo_new;             // Para indicar cuando un proceso pasa a ready
sem_t semaforo_ready_a_execute; // un hilo pasa a execute
sem_t semaforo_ready;           // hilo pasa a ready;
void terminar_kernel(t_log *LOGGER, t_config *MODULE_CONFIG);
pcb *crear_proceso(char *archivo, uint32_t tamanio,int prioridad_hilo);
void inicializar_variables();
void *iniciar_planificacion_lp(void *arg);
void iniciar_planificacion_cp();
void desbloquearHilo(void *arg);
int F_IO(tcb* hilo, int milisegundos);
int cargar_proceso_memoria(pcb *proceso);
int finalizar_proceso_memoria(pcb *proceso);
void enviar_finalizar_proceso_memoria(pcb *proceso, int conexion);
int enviar_crear_proceso_memoria(pcb *proceso, int conexion);
int recibir_motivo(int socket_cliente);
struct_hilo_posicion *hilo_prioritario();
int F_PROCESS_CREATE(char *nombre_archivo, int tamanio_proceso, int prioridad);
int F_THREAD_CREATE(char *nombre_archivo, int prioridad, int PID);
int F_THREAD_JOIN(tcb* hilo, int TID_finalizar);
void colas_multiples();
void prioridad();
int F_PROCESS_EXIT(tcb *hilo);

void liberar_pcb(pcb *proceso);
void liberar_mutex(void *elemento);
void liberar_tid(void *elemento);
int finalizar_hilo(tcb *hilo);
int creacion_hilo(pcb *proceso_padre, int prioridad, char *archivo);
void liberar_hilos_bloqueados(tcb *hilo);

void crearServidor();
int iniciar_servidor(char *puerto);
void *manejar_cliente(void *arg);
int recibir_operacion(int socket_cliente);
void *recibir_buffer(int *size, int socket_cliente);
void recibir_process_create(int socket_cliente);
void enviar_TID_PID(tcb *hilo, int conexion);
void fifo();
int cargar_hilo_memoria(tcb *nuevo_hilo);
int manejarRespuesta(tcb *hilo, int conexion);
op_code recibir_respuesta(int conexion, int TID_esperado);
void creacion_hilo_cero(pcb *proceso_padre, int prioridad, char *archivo);

#endif
