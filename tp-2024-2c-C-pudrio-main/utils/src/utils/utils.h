#ifndef UTILS_H_
#define UTILS_H_

#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <commons/config.h>
#include <commons/log.h>
#include <pthread.h>
#include <semaphore.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
typedef enum
{
	SET,				   // 0
	MUTEX_CREATE,		   // 1
	SUB,				   // 2
	JNZ,				   // 3
	LOG,				   // 4
	HANDSHAKE,			   // 5
	MUTEX_LOCK,			   // 6
	MUTEX_UNLOCK,		   // 7
	DUMP_MEMORY,		   // 8
	IO,					   // 9
	PROCESS_CREATE,		   // 10
	THREAD_CREATE,		   // 11
	THREAD_CANCEL,		   // 12
	THREAD_JOIN,		   // 13
	THREAD_EXIT,		   // 14
	PROCESS_EXIT,		   // 15
	SOLICITAR_INSTRUCCION, // 16
	EJECUTAR_HILO,		   // 17
	SUM,				   // 18

	// KERNEL-CPU
	INTERRUPCION_QUANTUM, // 19
	SEGMENTATION_FAULT,	  // 20

	// MEMORIA-CPU
	SOLICITUD_INSTRUCCION, // 21
	GUARDAR_CONTEXTO,	   // 22
	SOLICITAR_CONTEXTO,	   // 23
	ENVIO_PAQUETE,		   // 24
	READ_MEM,			   // 25
	WRITE_MEM,			   // 26
	SOLICITAR_BASE		   // 27

} op_code;

typedef enum
{
	CLI_MEMORIA,
	CLI_CPU,
	CLI_KERNEL
} tipo_cliente;
typedef struct
{
	uint32_t PID; // Identificador del proceso
	t_list *lista_tids;
	t_list *lista_mutex;
	t_list *lista_tcbs;
	char *archivo_pseudocodigo;
	uint32_t tamanio_proceso;
	uint32_t contador_tid;
	uint32_t base;
	uint32_t limite;
	int prioridad_hilo_0;
} pcb;

typedef enum
{
	NEW,
	READY,
	EXECUTE,
	BLOCK,
	EXIT
} estado_tcb;

typedef struct
{
	uint32_t TID;
	uint32_t prioridad;
	char *archivo;
	estado_tcb estado;
	// uint32_t mutex;
	uint32_t PID; // el identificador del proceso ASOCIADO
	t_list *lista_bloquedos;
} tcb;

typedef struct
{
	int size;
	void *stream;
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer *buffer;
} t_paquete;

extern t_config *MODULE_CONFIG;
extern t_log *LOGGER;
extern char *LOGGER_PATH;
extern char *MODULE_NAME;

typedef struct
{
	char *ip;
	char *puerto;
	t_log *logger;
} info_conexion;

typedef struct
{
	char *puerto;
	t_log *logger;
} info_conexion_servidor;

/**
 * @brief Imprime un saludo por consola
 * @param quien Módulo desde donde se llama a la función
 * @return No devuelve nada
 */
void inicializar_server(info_conexion_servidor *info);
int inicializar_cliente(info_conexion *info, tipo_cliente cliente);
void inicializar_config(char *path);
void atender_cliente(int *fd_conexion);
info_conexion *crear_conexion_cliente(char *ip, char *puerto, t_log *logger);
info_conexion_servidor *crear_conexion_servidor(char *puerto, t_log *logger);
void agregar_a_paquete(t_buffer *buffer, void *valor, int tamanio);
void enviar_paquete(t_paquete *paquete, int socket_cliente);
t_paquete *crear_paquete_con_codigo(op_code codigo_operacion);
void *serializar_paquete(t_paquete *paquete, int bytes);
void crear_buffer(t_paquete *paquete);
void eliminar_paquete(t_paquete *paquete);
void liberar_conexion(int socket_cliente);
void serializar_tamanio(t_buffer *buffer, uint32_t source);
char *concatenar_strings(char *a, char *b);
int recibir_operacion(int socket_cliente);
// initialize_logger()
#endif