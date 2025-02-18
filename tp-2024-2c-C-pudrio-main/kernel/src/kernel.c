#include <kernel.h>

int main(int argc, char *argv[])
{
    if (argc == 3)
    {
        ARCHIVO = argv[1];
        TAMANIO = argv[2];
    }
    else
    {
        printf(RED "\n >> No se recibieron los parametros necesarios para inicializar el modulo kernel << \n\n" RESET);
        return 0;
    }

    LOGGER = log_create(LOGGER_PATH, MODULE_NAME, true, LOG_LEVEL_DEBUG);
    inicializar_config(MODULE_CONFIG_PATH);
    inicializar_variables();

    // Creo la conexion con el modulo CPU
    info_conexion *info_conexion_dispatch = crear_conexion_cliente(IP_CPU, CPU_PORT_DISPATCH, LOGGER);
    info_conexion *info_conexion_interrupt = crear_conexion_cliente(IP_CPU, CPU_PORT_INTERRUPT, LOGGER);
    FD_SOCKET_CPU_DISPATCH = inicializar_cliente(info_conexion_dispatch, CLI_KERNEL);
    FD_SOCKET_CPU_INTERRUPT = inicializar_cliente(info_conexion_interrupt, CLI_KERNEL);

    // Creo el proceso inicial
    crear_proceso(ARCHIVO, (uint32_t)atoi(TAMANIO), 0);

    pthread_t thread_id;
    int err = pthread_create(&thread_id, NULL, iniciar_planificacion_lp, NULL);
    if (err != 0)
    {
        printf("Error al crear hilo planificacion largo plazo\n");
        exit(EXIT_FAILURE);
    }
    pthread_detach(thread_id);

    iniciar_planificacion_cp();

    terminar_kernel(LOGGER, MODULE_CONFIG);
    return 0;
}

void read_config(t_config *MODULE_CONFIG)
{
    IP_MEMORIA = config_get_string_value(MODULE_CONFIG, "IP_MEMORIA");
    MEMORY_PORT = config_get_string_value(MODULE_CONFIG, "PUERTO_MEMORIA");
    IP_CPU = config_get_string_value(MODULE_CONFIG, "IP_CPU");
    CPU_PORT_DISPATCH = config_get_string_value(MODULE_CONFIG, "PUERTO_CPU_DISPATCH");
    CPU_PORT_INTERRUPT = config_get_string_value(MODULE_CONFIG, "PUERTO_CPU_INTERRUPT");
    PLANIFICATION_ALGORITHM = config_get_string_value(MODULE_CONFIG, "ALGORITMO_PLANIFICACION");
    QUANTUM = config_get_int_value(MODULE_CONFIG, "QUANTUM");
    LOG_LEVEL = config_get_string_value(MODULE_CONFIG, "LOG_LEVEL");
}

void iniciar_planificacion_cp()
{
    if (strcmp(PLANIFICATION_ALGORITHM, "FIFO") == 0)
    {
        log_debug(LOGGER, "Planificacion FIFO");
        fifo();
    }
    else if (strcmp(PLANIFICATION_ALGORITHM, "PRIORIDADES") == 0)
    {
        log_debug(LOGGER, "Planificacion PRIORIDADES");
        prioridad();
    }
    else if (strcmp(PLANIFICATION_ALGORITHM, "CMN") == 0)
    {
        log_debug(LOGGER, "Planificacion COLAS_MULTIPLES");
        colas_multiples();
    }
    else
    {
        log_error(LOGGER, "Algoritmo de planificacion no reconocido");
    }
}

void inicializar_variables()
{
    sem_init(&semaforo_new, 0, 0);
    sem_init(&semaforo_ready_a_execute, 0, 0);
    // sem_init(&semaforo_ready, 0, 1);

    lista_new = list_create();
    lista_ready = list_create();
    lista_exec = list_create();
    lista_blocked = list_create();
    lista_exit = list_create();
    lista_pcbs = list_create();

    // cola_procesos= queue_create();
}

pcb *crear_proceso(char *archivo, uint32_t tamanio, int prioridad_hilo)
{

    pcb *nuevo_proceso = malloc(sizeof(pcb));

    nuevo_proceso->PID = contador_procesos;

    t_list *nueva_lista_tids = list_create();
    nuevo_proceso->lista_tids = nueva_lista_tids;

    t_list *nueva_lista_mutex = list_create();
    nuevo_proceso->lista_mutex = nueva_lista_mutex;

    t_list *nueva_lista_pcbs = list_create();
    nuevo_proceso->lista_tcbs = nueva_lista_pcbs;

    nuevo_proceso->tamanio_proceso = tamanio;

    nuevo_proceso->contador_tid = 0;

    nuevo_proceso->archivo_pseudocodigo = archivo;

    nuevo_proceso->prioridad_hilo_0 = prioridad_hilo;

    if (list_size(lista_new) > 0)
    {
        list_add(lista_new, nuevo_proceso);
        contador_procesos += 1;
        log_debug(LOGGER, "No se pudo cargar el proceso porque el anterior no fue cargado");
        return nuevo_proceso;
    }
    // Hay que mandarle PROCESS_CREATE a la memoria
    int res = cargar_proceso_memoria(nuevo_proceso);

    if (res == -1)
    {

        // El proceso no se pudo cargar en memoria
        list_add(lista_new, nuevo_proceso);
        contador_procesos += 1;

        return nuevo_proceso;
    }
    else
    {

        // El proceso se pudo cargar en memoria
        contador_procesos += 1;

        creacion_hilo_cero(nuevo_proceso, prioridad_hilo, archivo); // Creo el hilo main

        // Lo agrego a la lista donde estan todos los pcbs
        list_add(lista_pcbs, nuevo_proceso);

        log_info(LOGGER, "## (<%i>:<0>) Se crea el proceso - Estado: NEW", nuevo_proceso->PID);

        return nuevo_proceso;
    }
}

void *iniciar_planificacion_lp(void *arg)
{
    log_warning(LOGGER, "Se inicia la planificacion de largo plazo");

    while (1)
    {
        sem_wait(&semaforo_new);
        if (list_size(lista_new) != 0)
        {
            pcb *proceso = list_get(lista_new, 0);

            int res = cargar_proceso_memoria(proceso);

            if (res != -1)
            {
                // log_debug(LOGGER, "Se pudo crear el proceso que anteriormente no se pudo %d", proceso->PID);

                // Se cargo el proceso en memoria
                list_add(lista_pcbs, proceso);

                creacion_hilo_cero(proceso, proceso->prioridad_hilo_0, proceso->archivo_pseudocodigo); // Creo el hilo main
                // Si se carga en memoria lo quito de la cola NEW
                list_remove(lista_new, 0);

                sem_post(&semaforo_new);
            }
        }
    }
}

int cargar_proceso_memoria(pcb *proceso)
{
    info_conexion *info_conexion_memoria = crear_conexion_cliente(IP_MEMORIA, MEMORY_PORT, LOGGER);
    FD_SOCKET_MEMORIA = inicializar_cliente(info_conexion_memoria, CLI_KERNEL);

    if (FD_SOCKET_MEMORIA != -1)
    {
        int res = enviar_crear_proceso_memoria(proceso, FD_SOCKET_MEMORIA);
        liberar_conexion(FD_SOCKET_MEMORIA);
        if (res != 0)
        {
            return res;
        }
        else
        {
            log_error(LOGGER, "No hay espacio suficiente en la memoria para cargar otro proceso");
            return -1;
        }
    }
    else
    {
        log_error(LOGGER, "No se envio el proceso a memoria ya que no se establecio la conexion");
        return -1;
    }
}
int finalizar_proceso_memoria(pcb *proceso)
{
    info_conexion *info_conexion_memoria = crear_conexion_cliente(IP_MEMORIA, MEMORY_PORT, LOGGER);
    FD_SOCKET_MEMORIA = inicializar_cliente(info_conexion_memoria, CLI_KERNEL);

    if (FD_SOCKET_MEMORIA != -1)
    {
        enviar_finalizar_proceso_memoria(proceso, FD_SOCKET_MEMORIA);
        liberar_conexion(FD_SOCKET_MEMORIA);

        // Libero espacio en ready
        // sem_post(&semaforo_new); // se pasa un proceso a ready
        // liberar_pcb(proceso);
        // printf("Se finalizo el proceso en memoria de forma exitosa\n");
        return 0;
    }
    else
    {
        log_error(LOGGER, "No se quito el proceso de memoria, ya que no se establecio la conexion");
        return -1;
    }
}

void enviar_finalizar_proceso_memoria(pcb *proceso, int conexion)
{

    t_paquete *paquete = crear_paquete_con_codigo(PROCESS_EXIT);
    agregar_a_paquete(paquete->buffer, &(proceso->PID), sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &(proceso->tamanio_proceso), sizeof(uint32_t));
    enviar_paquete(paquete, conexion);
    eliminar_paquete(paquete);
}
int enviar_crear_proceso_memoria(pcb *proceso, int conexion)
{

    t_paquete *paquete = crear_paquete_con_codigo(PROCESS_CREATE);
    // log_info(LOGGER, "El nombre del archivo es %s", proceso->archivo_pseudocodigo);
    uint32_t tamanio_archivo = strlen(proceso->archivo_pseudocodigo) + 1;
    agregar_a_paquete(paquete->buffer, &(proceso->PID), sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &(proceso->tamanio_proceso), sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &tamanio_archivo, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, proceso->archivo_pseudocodigo, tamanio_archivo);

    enviar_paquete(paquete, conexion);
    eliminar_paquete(paquete);

    int res;
    recv(conexion, &res, sizeof(int), MSG_WAITALL);

    return res;
}

void liberar_tid(void *elemento)
{
    free(elemento);
}
void liberar_pcb(pcb *proceso)
{

    list_clean_and_destroy_elements(proceso->lista_tids, liberar_tid);
    list_clean_and_destroy_elements(proceso->lista_mutex, liberar_tid);
    list_clean_and_destroy_elements(proceso->lista_tcbs, liberar_tid);

    free(proceso);
}

void terminar_kernel(t_log *LOGGER, t_config *MODULE_CONFIG)
{
    log_destroy(LOGGER);
    config_destroy(MODULE_CONFIG);

    free(lista_new);
    free(lista_ready);
    free(lista_exec);
    free(lista_blocked);
    free(lista_exit);

    printf("\n\033[1;32m >> Programa terminado con exito <<\033[0m\n\n");
}

void fifo()
{
    while (1)
    {
        // tomo el semaforo para pasar de ready a execute
        sem_wait(&semaforo_ready_a_execute);
        if (list_size(lista_ready) > 0)
        {

            tcb *primer_hilo = list_get(lista_ready, 0);
            // log_info(LOGGER, "Primero PID: %d TID: %d", primer_hilo->PID, primer_hilo->TID);
            //  list_remove(lista_ready, 0);

            primer_hilo->estado = EXECUTE;
            list_add(lista_exec, primer_hilo);

            // log_debug(LOGGER, "Enviando hilo a CPU - < TID: %i >", primer_hilo->TID);

            enviar_TID_PID(primer_hilo, FD_SOCKET_CPU_DISPATCH);

            // Pongo el bucle para que cuando termina de hacer lo necesario (como crear un hilo) siga ejecutando
            int cortar_hilo = 1;
            while (cortar_hilo)
            {
                int result = manejarRespuesta(primer_hilo, FD_SOCKET_CPU_DISPATCH);

                if (result == -1)
                {
                    list_remove(lista_exec, 0);
                    cortar_hilo = 0;
                }
            }
            // log_info(LOGGER, "Se deja de ejecutar PID: %d TID: %d \n", primer_hilo->PID, primer_hilo->TID);
        }
    }
}

void prioridad()
{
    while (1)
    {
        // tomo el semaforo para pasar de ready a execute
        sem_wait(&semaforo_ready_a_execute);
        if (list_size(lista_ready) > 0)
        {

            // funcion que devuelve el hilo mas prioritario y su posicion
            struct_hilo_posicion *hilo_ejecutar_posicion = hilo_prioritario();

            // asigno el hilo mas prioritario
            tcb *hilo_ejecutar = hilo_ejecutar_posicion->tcb;

            // remuevo el hilo de la lista de ready
            // list_remove(lista_ready, hilo_ejecutar_posicion->posicion);

            // lo paso a execute
            hilo_ejecutar->estado = EXECUTE;
            list_add(lista_exec, hilo_ejecutar);

            // log_debug(LOGGER, "Enviando hilo a CPU - < TID: %i >", hilo_ejecutar->TID);

            enviar_TID_PID(hilo_ejecutar, FD_SOCKET_CPU_DISPATCH);

            int cortar_hilo = 1;
            while (cortar_hilo)
            {
                int result = manejarRespuesta(hilo_ejecutar, FD_SOCKET_CPU_DISPATCH);

                if (result == -1)
                {
                    list_remove(lista_exec, 0);
                    cortar_hilo = 0;
                }
            }
        }
    }
}

struct_hilo_posicion *hilo_prioritario()
{
    int i;
    struct_hilo_posicion *hiloPos = malloc(sizeof(struct_hilo_posicion));

    tcb *hilo = list_get(lista_ready, 0);

    hiloPos->tcb = hilo;
    hiloPos->posicion = 0;

    for (i = 1; i < list_size(lista_ready); i++)
    {
        tcb *hilo_comparar = list_get(lista_ready, i);
        if (hilo->prioridad > hilo_comparar->prioridad)
        {

            hilo = hilo_comparar; // cambio al hilo de mayor prioridad

            hiloPos->tcb = hilo;
            hiloPos->posicion = i;
        }
    }
    return hiloPos; // retorno el hilo mas prioritario y su posicion
}

void *verQuantum(void *arg)
{

    tcb *hilo = (tcb *)arg;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    // log_info(LOGGER, "Entro a quantum con %i", segundos_quantum);

    usleep(QUANTUM * 1000);

    t_paquete *paquete = crear_paquete_con_codigo(INTERRUPCION_QUANTUM);
    agregar_a_paquete(paquete->buffer, &(hilo->PID), sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &(hilo->TID), sizeof(uint32_t));
    enviar_paquete(paquete, FD_SOCKET_CPU_INTERRUPT);

    log_info(LOGGER, "## (<%d>:<%d>) - Desalojado por fin de Quantum", hilo->PID, hilo->TID);

    eliminar_paquete(paquete);
}

void colas_multiples()
{
    while (1)
    {
        // log_info(LOGGER, "No pude pasar el semaforo");
        sem_wait(&semaforo_ready_a_execute);
        if (list_size(lista_ready) > 0)
        {
            // log_info(LOGGER, "Estoy post lista ready");
            /* Es como el de prioridades pero con RR Q=QUANTUM en cada nivel de prioridad */

            struct_hilo_posicion *hilo_ejecutar_posicion = hilo_prioritario();
            tcb *hilo_ejecutar = hilo_ejecutar_posicion->tcb;
            // list_remove(lista_ready, hilo_ejecutar_posicion->posicion);
            hilo_ejecutar->estado = EXECUTE;
            list_add(lista_exec, hilo_ejecutar);

            enviar_TID_PID(hilo_ejecutar, FD_SOCKET_CPU_DISPATCH);

            // log_debug(LOGGER, "Enviando hilo a CPU - < TID: %i >", hilo_ejecutar->TID);

            pthread_t thread_id_quantum;
            if (pthread_create(&thread_id_quantum, NULL, verQuantum, hilo_ejecutar) != 0)
            {
                printf("Error al crear hilo de quantum.\n");
                exit(EXIT_FAILURE);
            }
            pthread_detach(thread_id_quantum);

            int cortar_hilo = 1;
            while (cortar_hilo)
            {
                int result = manejarRespuesta(hilo_ejecutar, FD_SOCKET_CPU_DISPATCH);

                if (result == -1)
                {
                    list_remove(lista_exec, 0);
                    // log_debug(LOGGER, "Se saca de EXEC");
                    cortar_hilo = 0;
                }
                else if (result == 3)
                {
                    // log_debug(LOGGER, "Hilo %i bloqueado\n", hilo_ejecutar->TID);

                    pthread_cancel(thread_id_quantum);

                    hilo_ejecutar->estado = READY;

                    list_remove(lista_ready, hilo_ejecutar_posicion->posicion);
                    list_add(lista_ready, hilo_ejecutar);
                    sem_post(&semaforo_ready_a_execute);

                    cortar_hilo = 0;
                }
            }
        }
    }
}

void enviar_TID_PID(tcb *hilo, int conexion)
{
    t_paquete *paquete = crear_paquete_con_codigo(EJECUTAR_HILO);

    agregar_a_paquete(paquete->buffer, &(hilo->TID), sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &(hilo->PID), sizeof(uint32_t));

    log_debug(LOGGER, "Se envia a CPU (<%d>:<%d>)", hilo->PID, hilo->TID);

    enviar_paquete(paquete, conexion);
    eliminar_paquete(paquete);
}

op_code recibir_respuesta(int conexion, int TID_esperado)
{
    int TID;
    op_code motivo;
    recv(conexion, &TID, sizeof(int), MSG_WAITALL);
    recv(conexion, &motivo, sizeof(int), MSG_WAITALL);

    if (TID == TID_esperado)
    {
        return motivo;
    }
    else
    {
        return -1;
    }
}

int manejarRespuesta(tcb *hilo, int conexion)
{
    int cod_op;
    uint32_t PID, TID;
    recv(conexion, &cod_op, sizeof(int), MSG_WAITALL);

    int size_buffer;
    recv(conexion, &size_buffer, sizeof(int), MSG_WAITALL);

    recv(conexion, &PID, sizeof(uint32_t), MSG_WAITALL);
    recv(conexion, &TID, sizeof(uint32_t), MSG_WAITALL);
    // log_error(LOGGER, "Se recibio el PID %i - TID %i", PID, TID);

    // log_error(LOGGER, "EL cod op es %d", cod_op);

    // Dependiendo el codigo de respuesta ejecuto una SYSCALL u otra
    int resultado = 2;
    switch (cod_op)
    {
    case PROCESS_CREATE:
        log_info(LOGGER, "## (<%d>:<%d>) - Solicitó syscall: <PROCESS_CREATE> ", PID, TID);
        int tamanio, tamanio_proceso, prioridad;

        recv(conexion, &tamanio, sizeof(int), MSG_WAITALL);
        char *nombre_archivo = malloc(tamanio);
        recv(conexion, nombre_archivo, tamanio, MSG_WAITALL);

        recv(conexion, &tamanio_proceso, sizeof(int), MSG_WAITALL);
        recv(conexion, &prioridad, sizeof(int), MSG_WAITALL);

        resultado = F_PROCESS_CREATE(nombre_archivo, tamanio_proceso, prioridad);
        break;
    case PROCESS_EXIT:
        log_info(LOGGER, "## (<%d>:<%d>) - Solicitó syscall: <PROCESS_EXIT> ", PID, TID);
        if (hilo->TID == 0)
        {
            resultado = F_PROCESS_EXIT(hilo);
            if (resultado == 2)
            {
                return -1;
            }
        }
        else
        {
            printf("La funcion PROCESS_EXIT solo puede ser invocada por el hilo 0, y fue llamada por el %i", hilo->TID);
        }
        break;
    case THREAD_CREATE:
        log_info(LOGGER, "## (<%d>:<%d>) - Solicitó syscall: <THREAD_CREATE> ", PID, TID);
        uint32_t tamanio2, prioridad2;
        recv(conexion, &tamanio2, sizeof(uint32_t), MSG_WAITALL);
        char *nombre_archivo2 = malloc(tamanio2);
        recv(conexion, nombre_archivo2, tamanio2, MSG_WAITALL);

        recv(conexion, &prioridad2, sizeof(uint32_t), MSG_WAITALL);

        printf("Nombre archivo: %s - Prioridad: %i\n", nombre_archivo2, prioridad2);

        resultado = F_THREAD_CREATE(nombre_archivo2, (int)prioridad2, hilo->PID);
        break;
    case THREAD_JOIN:
        log_info(LOGGER, "## (<%d>:<%d>) - Solicitó syscall: <THREAD_JOIN> ", PID, TID);
        uint32_t TID_finalizar;
        recv(conexion, &TID_finalizar, sizeof(uint32_t), MSG_WAITALL);
        resultado = F_THREAD_JOIN(hilo, TID_finalizar);
        break;
    case THREAD_CANCEL:
        log_info(LOGGER, "## (<%d>:<%d>) - Solicitó syscall: <THREAD_CANCEL> ", PID, TID);
        uint32_t TID_finalizar2;
        recv(conexion, &TID_finalizar2, sizeof(uint32_t), MSG_WAITALL);
        resultado = F_THREAD_CANCEL((int)TID_finalizar2);
        break;
    case THREAD_EXIT:
        log_info(LOGGER, "## (<%d>:<%d>) - Solicitó syscall: <THREAD_EXIT> ", PID, TID);
        // resultado = F_THREAD_EXIT(hilo->TID);
        resultado = 0;
        break;
    case MUTEX_CREATE:
        log_info(LOGGER, "## (<%d>:<%d>) - Solicitó syscall: <MUTEX_CREATE> ", PID, TID);
        uint32_t tamanio3;
        recv(conexion, &tamanio3, sizeof(uint32_t), MSG_WAITALL);
        // log_error(LOGGER, "EL TAMAÑO ES: %d", tamanio3);
        char *nombre_mutex = malloc(tamanio3);
        recv(conexion, nombre_mutex, tamanio3, MSG_WAITALL);
        resultado = F_MUTEX_CREATE((int)PID, nombre_mutex);
        break;
    case MUTEX_LOCK:
        log_info(LOGGER, "## (<%d>:<%d>) - Solicitó syscall: <MUTEX_LOCK> ", PID, TID);
        uint32_t tamanio4;
        recv(conexion, &tamanio4, sizeof(uint32_t), MSG_WAITALL);
        char *nombre_mutex2 = malloc(tamanio4);
        recv(conexion, nombre_mutex2, tamanio4, MSG_WAITALL);
        resultado = F_MUTEX_LOCK((int)TID, (int)PID, nombre_mutex2);
        break;
    case MUTEX_UNLOCK:
        log_info(LOGGER, "## (<%d>:<%d>) - Solicitó syscall: <MUTEX_UNLOCK> ", PID, TID);
        uint32_t tamanio5;
        recv(conexion, &tamanio5, sizeof(uint32_t), MSG_WAITALL);
        char *nombre_mutex3 = malloc(tamanio5);
        recv(conexion, nombre_mutex3, tamanio5, MSG_WAITALL);
        // log_warning(LOGGER, "El nombre del mutex es %s", nombre_mutex3);
        resultado = F_MUTEX_UNLOCK((int)TID, (int)PID, nombre_mutex3);
        break;
    case DUMP_MEMORY:
        log_info(LOGGER, "## (<%d>:<%d>) - Solicitó syscall: <DUMP_MEMORY> ", PID, TID);
        resultado = F_DUMP_MEMORY(hilo);
        break;
    case IO:
        log_info(LOGGER, "## (<%d>:<%d>) - Solicitó syscall: <IO> ", PID, TID);
        uint32_t milisegundos;
        recv(conexion, &milisegundos, sizeof(uint32_t), MSG_WAITALL);
        resultado = F_IO(hilo, (int)milisegundos);
        break;
    case INTERRUPCION_QUANTUM:
        // log_debug(LOGGER, "Entre a INTERRUPCION_QUANTUM");
        return 3;
        break;
    case SEGMENTATION_FAULT:
        // log_debug(LOGGER, "Hubo segmentation fault se finaliza el proceso");
        resultado = F_PROCESS_EXIT(hilo);
        if (resultado == 2)
        {
            return -1;
        }
    case -1:
        log_debug(LOGGER, "Se recibio -1, mensaje de error\n");
        break;
    default:
        break;
    }

    // Dependiendo el resultado lo añado en bloqued, exit, etc.
    if (resultado == -1)
    { // ERROR
        // Ocurrio un error
        log_warning(LOGGER, "Ocurrio un error\n");
        return -1;
    }
    else if (resultado == 0)
    { // EXIT
        // log_info(LOGGER, "Hilo %i finalizado\n", hilo->TID);
        finalizar_hilo(hilo);
        return -1;
    }
    else if (resultado == 1)
    { // BLOCK
        // log_info(LOGGER, "Hilo %i bloqueado\n", hilo->TID);
        quitarHiloLista(hilo->TID, hilo->PID);

        hilo->estado = BLOCK;
        list_add(lista_blocked, hilo);
        return -1;
    }
    // El resultado 2 es que no haga nada mas
}

void creacion_hilo_cero(pcb *proceso_padre, int prioridad, char *archivo)
{
    tcb *nuevo_hilo = malloc(sizeof(tcb));

    nuevo_hilo->TID = proceso_padre->contador_tid;
    proceso_padre->contador_tid += 1;

    nuevo_hilo->prioridad = (uint32_t)prioridad;
    nuevo_hilo->PID = proceso_padre->PID;
    nuevo_hilo->estado = READY;
    nuevo_hilo->archivo = archivo;

    // Creo la lista para los TID de los hilos que estan bloqueados por este hilo
    t_list *nueva_lista_bloqueados = list_create();
    nuevo_hilo->lista_bloquedos = nueva_lista_bloqueados;

    list_add(proceso_padre->lista_tcbs, nuevo_hilo);
    list_add(proceso_padre->lista_tids, &(nuevo_hilo->TID));

    list_add(lista_ready, nuevo_hilo);
    sem_post(&semaforo_ready_a_execute);

    log_info(LOGGER, "## (<%d>:<%d>) Se crea el Hilo - Estado: READY", nuevo_hilo->PID, nuevo_hilo->TID);
}
int creacion_hilo(pcb *proceso_padre, int prioridad, char *archivo)
{
    tcb *nuevo_hilo = malloc(sizeof(tcb));

    nuevo_hilo->TID = proceso_padre->contador_tid;
    proceso_padre->contador_tid += 1;

    nuevo_hilo->prioridad = prioridad;
    nuevo_hilo->PID = proceso_padre->PID;
    nuevo_hilo->estado = READY;
    nuevo_hilo->archivo = archivo;

    // Creo la lista para los TID de los hilos que estan bloqueados por este hilo
    t_list *nueva_lista_bloqueados = list_create();
    nuevo_hilo->lista_bloquedos = nueva_lista_bloqueados;

    list_add(proceso_padre->lista_tcbs, nuevo_hilo);
    list_add(proceso_padre->lista_tids, &(nuevo_hilo->TID));

    // log_info(LOGGER, "Se crea el hilo %i", nuevo_hilo->TID);

    int res = cargar_hilo_memoria(nuevo_hilo);
    log_info(LOGGER, "## (<%d>:<%d>) Se crea el Hilo - Estado: READY", nuevo_hilo->PID, nuevo_hilo->TID);
    return res;
}

int cargar_hilo_memoria(tcb *nuevo_hilo)
{
    info_conexion *info_conexion_memoria = crear_conexion_cliente(IP_MEMORIA, MEMORY_PORT, LOGGER);
    FD_SOCKET_MEMORIA = inicializar_cliente(info_conexion_memoria, CLI_KERNEL);

    if (FD_SOCKET_MEMORIA != -1)
    {
        // Creo el hilo
        //  Envio el hilo a memoria para ser creado
        uint32_t tamanio_archivo = strlen(nuevo_hilo->archivo) + 1;
        t_paquete *paquete = crear_paquete_con_codigo(THREAD_CREATE);
        agregar_a_paquete(paquete->buffer, &(nuevo_hilo->PID), sizeof(uint32_t));
        agregar_a_paquete(paquete->buffer, &(nuevo_hilo->TID), sizeof(uint32_t));
        agregar_a_paquete(paquete->buffer, &tamanio_archivo, sizeof(uint32_t));
        agregar_a_paquete(paquete->buffer, nuevo_hilo->archivo, tamanio_archivo);

        enviar_paquete(paquete, FD_SOCKET_MEMORIA);
        eliminar_paquete(paquete);

        // Recibo la respuesta
        int res;
        recv(FD_SOCKET_MEMORIA, &res, sizeof(int), MSG_WAITALL);
        liberar_conexion(FD_SOCKET_MEMORIA);

        if (res == 1)
        {
            nuevo_hilo->estado = READY;
            list_add(lista_ready, nuevo_hilo);
            sem_post(&semaforo_ready_a_execute);

            return 0;
        }
        else
        {
            log_error(LOGGER, "No hay espacio suficiente en la memoria para cargar un nuevo hilo");
            return -1;
        }
    }
    else
    {
        log_error(LOGGER, "No se envio el hilo a memoria ya que no se establecio la conexion");
        return -1;
    }
}

int finalizar_hilo(tcb *hilo)
{
    info_conexion *info_conexion_memoria = crear_conexion_cliente(IP_MEMORIA, MEMORY_PORT, LOGGER);
    FD_SOCKET_MEMORIA = inicializar_cliente(info_conexion_memoria, CLI_KERNEL);

    if (FD_SOCKET_MEMORIA != -1)
    {
        // Envio el hilo a memoria para ser finalizado
        t_paquete *paquete = crear_paquete_con_codigo(THREAD_EXIT);
        agregar_a_paquete(paquete->buffer, &(hilo->PID), sizeof(uint32_t));
        agregar_a_paquete(paquete->buffer, &(hilo->TID), sizeof(uint32_t));
        enviar_paquete(paquete, FD_SOCKET_MEMORIA);
        eliminar_paquete(paquete);

        // Recibo la respuesta
        int res;
        recv(FD_SOCKET_MEMORIA, &res, sizeof(int), MSG_WAITALL);
        liberar_conexion(FD_SOCKET_MEMORIA);

        if (res == 1)
        {
            quitarHiloLista(hilo->TID, hilo->PID);
            hilo->estado = EXIT;
            list_add(lista_exit, hilo);
            liberar_hilos_bloqueados(hilo);
            log_info(LOGGER, "## (<%d>:<%d>) Finaliza el hilo", hilo->PID, hilo->TID);
            return 0;
        }
        else
        {
            log_error(LOGGER, "Error al finalizar el hilo en memoria");
            return -1;
        }
    }
    else
    {
        log_error(LOGGER, "No se envio el hilo a memoria ya que no se establecio la conexion");
        return -1;
    }
}
void liberar_hilos_bloqueados(tcb *hilo)
{
    for (int i = 0; i < list_size(hilo->lista_bloquedos); i++)
    {
        int TID_desbloquear = list_get(hilo->lista_bloquedos, i);

        for (int i = 0; i < list_size(lista_blocked); i++)
        {
            tcb *hilo_bloqueado = list_get(lista_blocked, i);

            if (hilo_bloqueado->TID == TID_desbloquear && hilo_bloqueado->PID == hilo->PID)
            {
                list_remove(lista_blocked, i);
                list_add(lista_ready, hilo_bloqueado);
                sem_post(&semaforo_ready_a_execute);
            }
        }
    }
}

void quitarHiloLista(int TID, int PID)
{
    // Reviso cada lista y lo quito de la que se encuentre
    for (int i = 0; i < list_size(lista_ready); i++)
    {
        tcb *hilo_quitar = list_get(lista_ready, i);
        if (hilo_quitar->TID == TID && hilo_quitar->PID == PID)
        {
            // log_info(LOGGER, "Se quita el hilo de la lista ready %d", TID);
            list_remove(lista_ready, i);
            return;
        }
    }
    for (int i = 0; i < list_size(lista_blocked); i++)
    {
        tcb *hilo_quitar = list_get(lista_blocked, i);
        if (hilo_quitar->TID == TID && hilo_quitar->PID == PID)
        {
            list_remove(lista_blocked, i);
            return;
        }
    }
}

/*                          SYSCALLS                        */
// Procesos
int F_PROCESS_CREATE(char *nombre_archivo, int tamanio_proceso, int prioridad)
{

    // Crear un nuevo PCB y un TCB asociado con TID 0 y dejarlo en estado NEW
    // La prioridad de el hilo main no es siempre 0 (maxima) ??

    crear_proceso(nombre_archivo, tamanio_proceso, prioridad);
    return 2; // No hace falta hacer nada
}

int F_PROCESS_EXIT(tcb *hilo)
{
    // Finalizar el PCB correspondiente al TCB y enviar todos los TCBs a la cola EXIT
    // Solo sera llamada por el TID 0 del proceso y debera indicar a la memoria la finalizacion de
    // dicho proceso

    // Primero busco el proceso en la lista de PCBs
    for (int i = 0; i < list_size(lista_pcbs); i++)
    {
        pcb *pcb_terminar = list_get(lista_pcbs, i);

        if (hilo->PID == pcb_terminar->PID)
        {
            // Termino todos los hilos que pertenecen a ese proceso
            for (int j = 0; j < list_size(pcb_terminar->lista_tcbs); j++)
            {
                tcb *hilo_terminar = list_get(pcb_terminar->lista_tcbs, j);
                quitarHiloLista(hilo_terminar->TID, hilo_terminar->PID); // Quita el hilo de la lista donde se encuentre, ready o blocked
                hilo_terminar->estado = EXIT;
                list_add(lista_exit, hilo_terminar);
                liberar_hilos_bloqueados(hilo_terminar);
            }
            list_remove(lista_pcbs, i);              // Saco el proceso de la lista de procesos
            finalizar_proceso_memoria(pcb_terminar); // Le envio a memoria que finalice el proceso

            log_info(LOGGER, "## Finaliza el proceso <%d>", hilo->PID);

            break;
        }
    }

    sem_post(&semaforo_new); // Intenta cargar los procesos que no pudieron ser cargados antes

    return 2; // El codigo 2 es que no haga nada
}

// Threads
int F_THREAD_CREATE(char *nombre_archivo, int prioridad, int PID)
{

    pcb *proceso_padre;
    // Busco el proceso padre
    for (int i = 0; i < list_size(lista_pcbs); i++)
    {
        pcb *proceso = list_get(lista_pcbs, i);
        if (proceso->PID == PID)
        {
            proceso_padre = proceso;
            break;
        }
    }
    if (proceso_padre)
    {
        creacion_hilo(proceso_padre, prioridad, nombre_archivo);
        return 2;
    }

    return 0; // Si no encontro al proceso padre
}

int F_THREAD_JOIN(tcb *hilo, int TID_finalice)
{

    // esta syscall recibe como parámetro un TID, mueve el hilo que la invocó al estado BLOCK
    // hasta que el TID pasado por parámetro finalice. En caso de que el TID pasado por parámetro no
    // exista o ya haya finalizado, esta syscall no hace nada y el hilo que la invocó continuará su
    // ejecución.

    // Buscar el hilo y le añado a la lista de hilos que bloqueó
    for (int i = 0; i < list_size(lista_ready); i++)
    {
        tcb *hilo_buscado = list_get(lista_ready, i);
        if (hilo_buscado->TID == TID_finalice)
        {
            list_add(hilo_buscado->lista_bloquedos, hilo->TID);
        }
    }
    for (int i = 0; i < list_size(lista_blocked); i++)
    {
        tcb *hilo_buscado = list_get(lista_blocked, i);
        if (hilo_buscado->TID == TID_finalice)
        {
            list_add(hilo_buscado->lista_bloquedos, hilo->TID);
        }
    }

    log_info(LOGGER, "## (<%d>:<%d>) - Bloqueado por: <PTHREAD_JOIN> ", hilo->PID, hilo->TID);

    return 1; // Codigo para pasar a bloqueado el hilo
}

int F_THREAD_CANCEL(int TID)
{
    // Finalizar el thread, pasandolo al estado EXIT.
    // Se le indica a la memoria la finalizacion de dicho hilo.
    // Si no existe o ya fue finalizado, esta syscall no hace nada.
    // El hilo que la invoco continua su ejecucion
    tcb *hilo_finalizar;
    for (int i = 0; i < list_size(lista_exec); i++)
    {
        tcb *hilo = list_get(lista_exec, i);
        if (hilo->TID == TID)
        {
            hilo_finalizar = hilo;
            list_remove(lista_exec, i);
        }
    }
    if (!hilo_finalizar)
    {
        for (int i = 0; i < list_size(lista_blocked); i++)
        {
            tcb *hilo = list_get(lista_blocked, i);
            if (hilo->TID == TID)
            {
                hilo_finalizar = hilo;
                list_remove(lista_blocked, i);
            }
        }
    }
    if (!hilo_finalizar)
    {
        for (int i = 0; i < list_size(lista_ready); i++)
        {
            tcb *hilo = list_get(lista_ready, i);
            if (hilo->TID == TID)
            {
                hilo_finalizar = hilo;
                list_remove(lista_ready, i);
            }
        }
    }
    if (hilo_finalizar)
    {
        finalizar_hilo(hilo_finalizar); // Se encarga de todo
    }
    return 2;
}

int F_THREAD_EXIT()
{
    // Finaliza el hilo que la invoco, pasandolo a estado EXIT
    // Se debera indicar a memoria la finalizacion del hilo

    return 0; // Lo manda a exit
}

// Terminar de corregir estas funciones

// Mutex
int F_MUTEX_CREATE(int PID, char *nombre_mutex)
{
    pcb *proceso;
    // Buscar el proceso
    for (int i = 0; i < list_size(lista_pcbs); i++)
    {
        pcb *proceso_lista = list_get(lista_pcbs, i);
        if (proceso_lista->PID == PID)
        {
            proceso = proceso_lista;
            break;
        }
    }
    if (proceso)
    {
        // Corroborar que no exista otro recurso con el mismo nombre
        for (int i = 0; i < list_size(proceso->lista_mutex); i++)
        {
            t_mutex *mutex_lista = list_get(proceso->lista_mutex, i);
            if (strcmp(mutex_lista->nombre, nombre_mutex) == 0)
            {
                return 0; // Si ya existe otro mutex con ese nombre
            }
        }

        t_mutex *new_mutex = malloc(sizeof(t_mutex));
        new_mutex->lista_hilos = list_create(); // La cola de hilos que quieren este mutex
        new_mutex->nombre = nombre_mutex;
        new_mutex->valor = 1;
        new_mutex->TID = -1; // El hilo al que esta asignado
        list_add(proceso->lista_mutex, new_mutex);
        return 2; // Si fue todo bien, no hago nada
    }
    return 0; // Si no se encontro el proceso
}

int F_MUTEX_LOCK(int TID, int PID, char *nombre_mutex)
{
    // log_info(LOGGER, "MUTEX LOCK");
    //  Buscar el proceso
    pcb *proceso;
    for (int i = 0; i < list_size(lista_pcbs); i++)
    {
        pcb *proceso_lista = list_get(lista_pcbs, i);
        if (proceso_lista->PID == PID)
        {
            proceso = proceso_lista;
        }
    }
    if (proceso)
    {
        // Corroborar que exista el mutex
        t_mutex *mutex_buscado;
        for (int i = 0; i < list_size(proceso->lista_mutex); i++)
        {
            t_mutex *mutex_lista = list_get(proceso->lista_mutex, i);
            if (strcmp(mutex_lista->nombre, nombre_mutex) == 0)
            {
                mutex_buscado = mutex_lista;
                break;
            }
        }

        if (mutex_buscado)
        {
            if (mutex_buscado->valor == 1)
            { // Si no está tomado
                // Asignar el mutex al hilo

                log_warning(LOGGER, "SE LE ASIGNA EL RECURSO %s AL HILO %d ", nombre_mutex, TID);

                mutex_buscado->valor = 0;
                mutex_buscado->TID = TID;
                int resultado = 1;
                send(FD_SOCKET_CPU_DISPATCH, &resultado, sizeof(int), 0);
                return 2;
            }
            else if (mutex_buscado->valor == 0)
            { // Si ya está tomado
                log_info(LOGGER, "## (<%d>:<%d>) - Bloqueado por: <%s> ", PID, TID, nombre_mutex);

                list_add(mutex_buscado->lista_hilos, TID);
                int resultado = 0;
                send(FD_SOCKET_CPU_DISPATCH, &resultado, sizeof(int), 0);
                return 1; // Codigo para bloquear el hilo
            }
        }
        else
        {
            log_warning(LOGGER, "NO SE ENCONTRO EL RECURSO <%s> ", nombre_mutex);
            int resultado = 0;
            send(FD_SOCKET_CPU_DISPATCH, &resultado, sizeof(int), 0);
            return 0; // Codigo para mandar a exit
        }
    }
    return 0;
}

int F_MUTEX_UNLOCK(int TID, int PID, char *nombre_mutex)
{
    // Buscar el proceso
    pcb *proceso;
    for (int i = 0; i < list_size(lista_pcbs); i++)
    {
        pcb *proceso_lista = list_get(lista_pcbs, i);
        if (proceso_lista->PID == PID)
        {
            proceso = proceso_lista;
        }
    }

    if (proceso)
    {
        // log_debug(LOGGER, "Buscando %s ", nombre_mutex);

        // Corroborar que exista el mutex
        t_mutex *mutex_buscado = malloc(sizeof(t_mutex));
        for (int i = 0; i < list_size(proceso->lista_mutex); i++)
        {
            t_mutex *mutex_lista = list_get(proceso->lista_mutex, i);

            // log_debug(LOGGER, "Encontre %s ", mutex_lista->nombre);

            if (strcmp(mutex_lista->nombre, nombre_mutex) == 0)
            {
                mutex_buscado = mutex_lista;
                // log_debug(LOGGER, "Mi recurso es %s -TID tanto %d ", mutex_buscado->nombre, mutex_buscado->TID);
                break;
            }
        }
        if (mutex_buscado)
        {
            if (mutex_buscado->TID == TID)
            { // Si lo tiene asignado
                if (list_size(mutex_buscado->lista_hilos) > 0)
                { // Si hay otros hilos esperando ese mutex
                    int TID_siguiente = list_get(mutex_buscado->lista_hilos, 0);
                    int res = list_remove(mutex_buscado->lista_hilos, 0);

                    mutex_buscado->TID = TID_siguiente;

                    // Agrego a lista_ready el que estaba bloqueado
                    for (int i = 0; i < list_size(lista_blocked); i++)
                    {
                        tcb *hilo_block = list_get(lista_blocked, i);
                        if (hilo_block->TID == TID_siguiente)
                        {
                            log_warning(LOGGER, "SE DESBLOQUEA EL HILO: %d por el recurso %s", hilo_block->TID, nombre_mutex);
                            list_remove(lista_blocked, i);
                            hilo_block->estado = READY;
                            list_add(lista_ready, hilo_block);
                            sem_post(&semaforo_ready_a_execute);
                        }
                    }
                }
                else
                {
                    mutex_buscado->valor = 1;
                    mutex_buscado->TID = -1;
                }
            }
        }
        return 2;
    }
    return 0; // Si no existe el proceso
}

void desbloquearHiloDumpMemory(void *arg)
{
    tcb *hilo = (tcb *)arg;

    int res;
    recv(FD_SOCKET_MEMORIA, &res, sizeof(int), MSG_WAITALL);

    if (res == 1)
    {
        // Agrega devuelta a ready y lo saca de lista blocked
        hilo->estado = READY;
        quitarHiloLista(hilo->TID, hilo->PID); // Lo quito de la lista blocked
        list_add(lista_ready, hilo);
        sem_post(&semaforo_ready_a_execute);
        log_info(LOGGER, "## (<%d>:<%d>) - Finalizó DUMP_MEMORY y pasa a READY", hilo->PID, hilo->TID);
    }
    else
    {
        log_error(LOGGER, "No se pudo realizar el DUMP_MEMORY");
    }
}

int F_DUMP_MEMORY(tcb *hilo)
{
    info_conexion *info_conexion_memoria = crear_conexion_cliente(IP_MEMORIA, MEMORY_PORT, LOGGER);
    FD_SOCKET_MEMORIA = inicializar_cliente(info_conexion_memoria, CLI_KERNEL);

    if (FD_SOCKET_MEMORIA != -1)
    {
        t_paquete *paquete = crear_paquete_con_codigo(DUMP_MEMORY);
        agregar_a_paquete(paquete->buffer, &(hilo->PID), sizeof(uint32_t));
        agregar_a_paquete(paquete->buffer, &(hilo->TID), sizeof(uint32_t));
        enviar_paquete(paquete, FD_SOCKET_MEMORIA);
        eliminar_paquete(paquete);

        // Creo un hilo para que desbloquee el hilo cuando le llegue la respuesta
        pthread_t thread_id_io;
        if (pthread_create(&thread_id_io, NULL, desbloquearHiloDumpMemory, hilo) != 0)
        {
            printf("Error al crear hilo de DUMP_MEMORY.\n");
            return 0;
        }
        pthread_detach(thread_id_io);

        return 1; // Bloqueo el hilo
    }
    else
    {
        log_error(LOGGER, "No se pudo establecer la conexion con memoria");
        return 0; // Error si no se pudo conectar con memoria
    }
}

void desbloquearHilo(void *arg)
{
    hiloMiliseg *newHiloMilisec = (hiloMiliseg *)arg;

    usleep(newHiloMilisec->milisegundos * 1000);

    // Agrega devuelta a ready y lo saca de lista blocked
    tcb *hilo = newHiloMilisec->hilo;
    hilo->estado = READY;
    quitarHiloLista(hilo->TID, hilo->PID); // Lo quito de la lista blocked
    list_add(lista_ready, hilo);
    sem_post(&semaforo_ready_a_execute);

    log_info(LOGGER, "## (<%d>:<%d>) - Finalizó IO y pasa a READY", hilo->PID, hilo->TID);
}

int F_IO(tcb *hilo, int milisegundos)
{
    hiloMiliseg *newHiloMilisec = malloc(sizeof(hiloMiliseg));
    newHiloMilisec->hilo = hilo;
    newHiloMilisec->milisegundos = milisegundos;

    pthread_t thread_id_io;
    if (pthread_create(&thread_id_io, NULL, desbloquearHilo, newHiloMilisec) != 0)
    {
        printf("Error al crear hilo de IO.\n");
        return 0;
    }
    pthread_detach(thread_id_io); // No espero que termine
    log_info(LOGGER, "## (<%d>:<%d>) - Bloqueado por: <IO>", hilo->PID, hilo->TID);
    return 1; //--> Bloquea el hilo
}
