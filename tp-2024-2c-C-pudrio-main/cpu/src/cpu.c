#include <utils/utils.h>
#include <cpu.h>

int main(int argc, char *argv[])
{

    // bool primer_pcb = true;
    contexto = malloc(sizeof(tcb_cpu));
    pthread_t hilo_cliente_conexion_memoria;

    pthread_t hilo_servidor_dispatch;
    pthread_t hilo_servidor_interrupt;

    cpu_logger = log_create(LOGGER_PATH, MODULE_NAME, true, LOG_LEVEL_DEBUG);
    inicializar_config(MODULE_CONFIG_PATH);

    sem_init(&sem_pedido_instruccion, 0, 0);

    info_conexion *info_conexion_memoria = crear_conexion_cliente(IP_MEMORIA, MEMORY_PORT, cpu_logger);
    FD_SOCKET_MEMORIA = inicializar_cliente(info_conexion_memoria, CLI_CPU);
    log_info(cpu_logger, "Conexion con MEMORIA exitosa");

    /*info_conexion_servidor *info_servidor_cpu_dispatch = crear_conexion_servidor(LISTENER_PORT_DISPATCH,LOGGER);
    info_conexion_servidor *info_servidor_cpu_interrupt = crear_conexion_servidor(LISTENER_PORT_INTERRUPT,LOGGER);

    pthread_create(&hilo_cliente_conexion_memoria, NULL, (void *) inicializar_cliente, info_conexion_memoria);

    pthread_create(&hilo_servidor_dispatch, NULL, (void *) inicializar_server_cpu, info_servidor_cpu_dispatch);
    pthread_create(&hilo_servidor_interrupt, NULL, (void *) inicializar_server_cpu, info_servidor_cpu_interrupt);

    FD_SOCKET_MEMORIA = inicializar_cliente(info_conexion_memoria); */

    // conectarse como cliente con memoria
    // FD_SOCKET_MEMORIA = crear_conexion(IP_MEMORIA, MEMORY_PORT);
    // log_info(cpu_logger, "Conexion con MEMORIA exitosa");

    // FD_SOCKET_K_DISPATCH = iniciar_servidor(LISTENER_PORT_DISPATCH, cpu_logger, "CPU-DISPATCH iniciado !!");
    pthread_t hilo_dispatch;
    int err_d = pthread_create(&hilo_dispatch, NULL, (void *)crearServidorKernel, (void *)LISTENER_PORT_DISPATCH);
    if (err_d != 0)
    {
        printf("Fallo de creación del hilo dispatch\n");
        return -1;
    }
    pthread_detach(hilo_dispatch);
    // Con detach el hilo se hace cargo y lo termina cuando haga falta,
    // sino se queda esperando que termine y no sigue con el resto del codigo

    crearServidorKernel((void *)LISTENER_PORT_INTERRUPT);

    return EXIT_SUCCESS;
}

void *crearServidorKernel(void *arg)
{
    char *puerto_escucha = (char *)arg;

    int server_fd = iniciar_servidor(puerto_escucha, cpu_logger);
    if (server_fd == -1)
    {
        log_error(cpu_logger, "Error al iniciar el servidor");
    }
    log_warning(cpu_logger, "Server escuchando en el puerto %s", puerto_escucha);

    int cliente_fd = esperar_cliente(server_fd);

    if (puerto_escucha == LISTENER_PORT_INTERRUPT)
    {
        printf(cpu_logger, "Se conecto al interrupt\n");
        FD_SOCKET_K_INTERRUPT = cliente_fd;
        manejar_handshake(cliente_fd);
        log_info(cpu_logger, "Voy a atender las solicitudes de interrupt");
        atender_mensajes_kernel_interrupt(cliente_fd);
    }
    else if (puerto_escucha == LISTENER_PORT_DISPATCH)
    {
        log_debug(cpu_logger, "Se conecto al dispatch\n");
        FD_SOCKET_K_DISPATCH = cliente_fd;
    }

    manejar_handshake(cliente_fd);
    log_info(cpu_logger, "Se conecto el kernel por el puerto %s", puerto_escucha);
    if (cliente_fd != -1)
    { // Cuando se conecta algun modulo
        log_info(cpu_logger, "Voy a atender las solicitudes de kernel");
        atender_mensajes_kernel(cliente_fd);
    }
}
void crearServidorMemoria(char *puerto_escucha)
{
    FD_SOCKET_MEMORIA = iniciar_servidor(puerto_escucha, cpu_logger);

    if (FD_SOCKET_MEMORIA == -1)
    {
        log_error(cpu_logger, "Error al iniciar el servidor");
    }
    log_warning(cpu_logger, "Server escuchando en el puerto %s", puerto_escucha);

    while (1)
    {
        int cliente_fd = esperar_cliente(FD_SOCKET_MEMORIA);
        manejar_handshake(cliente_fd);
        log_info(cpu_logger, "Se conecto la memoria por el puerto %s", puerto_escucha);
        if (cliente_fd != -1)
        { // Cuando se conecta algun modulo
            atender_mensajes_memoria(cliente_fd);
        }
    }
}

void manejar_handshake(int cliente_fd)
{

    uint32_t handshake, cod_op;
    recv(cliente_fd, &cod_op, sizeof(int32_t), MSG_WAITALL);
    recv(cliente_fd, &handshake, sizeof(int32_t), MSG_WAITALL);

    if (cod_op == 1 && handshake == CLI_KERNEL)
    {
        // log_info(cpu_logger, "Handshake realizado con exito! Kernel Conectado.");
        uint32_t OK = 1;
        send(cliente_fd, &OK, sizeof(int32_t), 0);
    }
    else
    {
        log_info(cpu_logger, "Error al realizar el handshake!");
        uint32_t NOT_OK = -1;
        send(cliente_fd, &NOT_OK, sizeof(int32_t), 0);
    }
}

int crear_conexion(char *ip, char *puerto)
{
    struct addrinfo hints;
    struct addrinfo *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int err = getaddrinfo(ip, puerto, &hints, &server_info);

    if (err != 0)
    {
        printf("Error al crear conexion");
        exit(-2);
    }

    int socket_cliente = socket(server_info->ai_family,
                                server_info->ai_socktype,
                                server_info->ai_protocol);

    if (setsockopt(socket_cliente, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) < 0)
    {
        perror("setsockopt(SO_REUSEADDR) failed");
    }

    connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen); // conecta socket

    freeaddrinfo(server_info);

    return socket_cliente;
}

int esperar_cliente(int socket_servidor)
{
    int socket_cliente = accept(socket_servidor, NULL, NULL);
    return socket_cliente;
}

int iniciar_servidor(char *puerto, t_log *logger) // Crea socket servidor, lo bindea y lo deja escuchando
{

    int socket_servidor;

    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, puerto, &hints, &servinfo);

    // Creamos el socket de escucha del servidor
    socket_servidor = socket(servinfo->ai_family,
                             servinfo->ai_socktype,
                             servinfo->ai_protocol);
    // Asociamos el socket a un puerto
    bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
    // Escuchamos las conexiones entrantes
    listen(socket_servidor, SOMAXCONN);
    freeaddrinfo(servinfo);

    return socket_servidor;
}

// la funcion read_config se ejecuta cuando se ejecuta initialize_config. La funcion initialize_config
// esta en el utils.c
void read_config(t_config *MODULE_CONFIG)
{
    IP_MEMORIA = config_get_string_value(MODULE_CONFIG, "IP_MEMORIA");
    MEMORY_PORT = config_get_string_value(MODULE_CONFIG, "PUERTO_MEMORIA");
    LISTENER_PORT_DISPATCH = config_get_string_value(MODULE_CONFIG, "PUERTO_ESCUCHA_DISPATCH");
    LISTENER_PORT_INTERRUPT = config_get_string_value(MODULE_CONFIG, "PUERTO_ESCUCHA_INTERRUPT");
    LOG_LEVEL = config_get_string_value(MODULE_CONFIG, "LOG_LEVEL");
}

/*void iniciar_semaforos(){
    sem_init(&sem_pedido_instruccion, 0, 0);
    sem_init(&sem_solicitud_lectura, 0, 0);
    sem_init(&sem_solicitud_escritura, 0, 0);
}*/

void inicializar_variables()
{
    respuesta_escritura = NULL;
    respuesta_lectura = NULL;
}

void atender_mensajes_kernel_interrupt(int socket_cliente)
{
    int fin = false;
    while (!fin)
    {
        // log_info(cpu_logger, "Esperando codigo de operacion de por interrupt");
        int cod_op;
        recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL);
        // printf("Recibo el cod_op %i \n", cod_op);
        switch (cod_op)
        {
        case INTERRUPCION_QUANTUM:
            log_info(cpu_logger, " ## Llega interrupción al puerto Interrupt");
            atender_interrupcion_quantum();
            // cicloDeInstruccion(); // esto capaz no va
            break;
        default:
            log_info(cpu_logger, "Codigo desconocido\n");
            fin = true;
            break;
        }
    }
}

void atender_mensajes_kernel(int socket_cliente)
{
    int fin = false;
    while (!fin)
    {
        // log_info(cpu_logger, "Esperando codigo de operacion de kernel en dispatch");
        int cod_op;

        recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL);
        // log_info(cpu_logger, "Recibi el cod op ");
        //  printf("Recibo el cod_op %i \n", cod_op);
        switch (cod_op)
        {

        case EJECUTAR_HILO:
            // log_debug(cpu_logger, "Me llego un hilo");
            recibir_tid_pid_kernel(socket_cliente);
            solicitar_contexto_mem();
            cicloDeInstruccion();
            break;
        case HANDSHAKE:
            log_info(cpu_logger, "Recibi un codigo de handshake");
        case -1:
            error_show("El cliente se desconecto. Terminando hilo del servidor\n");
            fin = true;
            break;
        default:
            log_info(cpu_logger, "Codigo desconocido\n");
            fin = true;
            break;
        }
    }
    return EXIT_SUCCESS;
}

void atender_mensajes_memoria(int socket_cliente)
{
    int fin = false;
    while (!fin)
    {
        int cod_op = recibir_operacion(socket_cliente);
        switch (cod_op)
        {
        case SOLICITUD_INSTRUCCION:
            log_trace(cpu_logger, "Me llego una instruccion");
            recibir_instruccion(socket_cliente);
            sem_post(&sem_pedido_instruccion);
            break;
        case -1:
            error_show("El cliente se desconecto. Terminando hilo del servidor");
            fin = true;
            break;
        default:
            break;
        }
    }
    return EXIT_SUCCESS;
}

void recibir_tid_pid_kernel(int FD_SOCKET_K_DISPATCH)
{
    int size_buffer = -1;
    uint32_t PIDrecibido = -1;
    uint32_t TIDrecibido = -1;

    recv(FD_SOCKET_K_DISPATCH, &size_buffer, sizeof(int), MSG_WAITALL);
    recv(FD_SOCKET_K_DISPATCH, &TIDrecibido, sizeof(uint32_t), MSG_WAITALL);
    recv(FD_SOCKET_K_DISPATCH, &PIDrecibido, sizeof(uint32_t), MSG_WAITALL);

    contexto->PID = PIDrecibido;
    contexto->TID = TIDrecibido;

    // log_info(cpu_logger, "El size es: %d", size_buffer);
    //  log_info(cpu_logger, "El pid es: %d", contexto->PID);
    //  log_info(cpu_logger, "El tid es: %d", contexto->TID);
    log_debug(cpu_logger, "Me llego el PID: %d y el TID %d", contexto->PID, contexto->TID);
    // log_info(cpu_logger, "Me llego el PIDnuevo: %d y el TIDnuevo %d", PIDrecibido, TIDrecibido);
}

void solicitar_contexto_mem()
{

    log_info(cpu_logger, "## TID: <%d> - Solicito Contexto Ejecución", contexto->TID);
    t_paquete *paquete = crear_paquete_con_codigo(SOLICITAR_CONTEXTO);
    agregar_a_paquete(paquete->buffer, &(contexto->PID), sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &(contexto->TID), sizeof(uint32_t));

    // log_info(cpu_logger, "Le pido el contexto a memoria de PID: %d y TID: %d", contexto->PID, contexto->TID);

    enviar_paquete(paquete, FD_SOCKET_MEMORIA);
    eliminar_paquete(paquete);

    uint32_t PID;
    uint32_t TID;
    uint32_t AX;
    uint32_t BX;
    uint32_t CX;
    uint32_t DX;
    uint32_t EX;
    uint32_t FX;
    uint32_t GX;
    uint32_t HX;
    uint32_t PC;
    int size_buffer = -1;
    int valor;

    recv(FD_SOCKET_MEMORIA, &valor, sizeof(int), MSG_WAITALL);
    // log_info(cpu_logger, "El valor es: %d", valor);
    recv(FD_SOCKET_MEMORIA, &size_buffer, sizeof(int), MSG_WAITALL);
    // recv(FD_SOCKET_MEMORIA, &PID, sizeof(uint32_t), MSG_WAITALL);
    // recv(FD_SOCKET_MEMORIA, &TID, sizeof(uint32_t), MSG_WAITALL);
    recv(FD_SOCKET_MEMORIA, &contexto->PC, sizeof(uint32_t), MSG_WAITALL);

    // log_info(cpu_logger, "El valor del PC es: %d", contexto->PC);
    recv(FD_SOCKET_MEMORIA, &contexto->AX, sizeof(uint32_t), MSG_WAITALL);
    recv(FD_SOCKET_MEMORIA, &contexto->BX, sizeof(uint32_t), MSG_WAITALL);
    recv(FD_SOCKET_MEMORIA, &contexto->CX, sizeof(uint32_t), MSG_WAITALL);
    recv(FD_SOCKET_MEMORIA, &contexto->DX, sizeof(uint32_t), MSG_WAITALL);
    recv(FD_SOCKET_MEMORIA, &contexto->EX, sizeof(uint32_t), MSG_WAITALL);
    recv(FD_SOCKET_MEMORIA, &contexto->FX, sizeof(uint32_t), MSG_WAITALL);
    recv(FD_SOCKET_MEMORIA, &contexto->GX, sizeof(uint32_t), MSG_WAITALL);
    recv(FD_SOCKET_MEMORIA, &contexto->HX, sizeof(uint32_t), MSG_WAITALL);
}

/* void atender_lectura(int FD_SOCKET_MEMORIA){

    int size_buffer = -1;
    uint32_t base_particion = -1;
    uint32_t tamanio_particion = -1;

    recv(*FD_SOCKET_MEMORIA, &size_buffer, sizeof(int), MSG_WAITALL);
    recv(*FD_SOCKET_MEMORIA, &base_particion, sizeof(int), MSG_WAITALL);
    recv(*FD_SOCKET_MEMORIA, &tamanio_particion, sizeof(int), MSG_WAITALL);

} */

void atender_interrupcion_quantum()
{

    int size_buffer = -1;
    uint32_t PID = -1;
    uint32_t TID = -1;

    recv(FD_SOCKET_K_INTERRUPT, &size_buffer, sizeof(int), MSG_WAITALL);
    recv(FD_SOCKET_K_INTERRUPT, &PID, sizeof(uint32_t), MSG_WAITALL);
    recv(FD_SOCKET_K_INTERRUPT, &TID, sizeof(uint32_t), MSG_WAITALL);

    // if (PID == contexto->PID && TID == contexto->TID)
    //{
    //  INTERRUMPI TID 3
    // log_error(cpu_logger, "Por quantum PID: %d - TID %d", PID, TID);
    pthread_mutex_lock(&mutex_hay_interrupcion);
    hay_interrupcion_quantum = true;
    pthread_mutex_unlock(&mutex_hay_interrupcion);
    //}
}

char *recibir_instruccion(int FD_SOCKET_MEMORIA)
{
    int cod_op;
    recv(FD_SOCKET_MEMORIA, &cod_op, sizeof(int), MSG_WAITALL);
    // log_info(cpu_logger, "El cod_op es: %d", cod_op);
    int size_buffer = -1;
    int tamanio_instruccion = -1;

    recv(FD_SOCKET_MEMORIA, &size_buffer, sizeof(int), MSG_WAITALL);
    recv(FD_SOCKET_MEMORIA, &tamanio_instruccion, sizeof(int), MSG_WAITALL);
    // chequear si hay que meter free
    char *instruccion = malloc(tamanio_instruccion);
    recv(FD_SOCKET_MEMORIA, instruccion, tamanio_instruccion, MSG_WAITALL);

    return instruccion;
    // free(instruccion);
}

/* void recibir_instruccion(int FD_SOCKET_MEMORIA)
{
    int size_buffer = -1;
    int tamanio_instruccion = -1;

    // como recibir la instruccion?? ------------------------------------------------------
    recv(FD_SOCKET_MEMORIA, &size_buffer, sizeof(int), MSG_WAITALL);
    recv(FD_SOCKET_MEMORIA, &tamanio_instruccion, sizeof(int), MSG_WAITALL);
    char *instruccion = malloc(tamanio_instruccion);
    recv(FD_SOCKET_MEMORIA, instruccion, tamanio_instruccion, MSG_WAITALL);

    char *delimitador = " ";
    instruccion_dividida = string_split(instruccion, delimitador);
    free(instruccion);
} */

void cicloDeInstruccion()
{
    int while_ciclo = 1;

    while (while_ciclo)
    {

        hayQueDesalojar = false;
        // FETCH (solicita y recibe instruccion de memoria)
        fetch();

        // esperar que llegue la instruccion de Memoria
        // COmento el semaforo porque esta en 0 y se frena toda la ejecucion
        // sem_wait(&sem_pedido_instruccion);

        // DECODE Y EXECUTE

        char *instruccion = recibir_instruccion(FD_SOCKET_MEMORIA);
        decodeYExecute(instruccion, &while_ciclo);

        liberar_instruccion_dividida();

        // No estoy seguro si se deberia hacer un free. Defini el instruccion_dividida como un char* array[4]
        // Capaz si le haces free perdes el puntero al array. Y tener un array de 4 lugares no te va a cambiar la vida
        // puedo estar pifiando tranquilamente
        // free(instruccion_dividida);

        mostrar_contexto();

        // CHECK INTERRUPTION
        // pthread_mutex_lock(&mutex_hay_interrupcion);
        if (hayQueDesalojar)
        {
            // pthread_mutex_unlock(&mutex_hay_interrupcion);
            log_warning(cpu_logger, "Desalojando hilo con PID %d TID %d", contexto->PID, contexto->TID);

            while_ciclo = 0;
        }
        else if (hay_interrupcion_quantum)
        {
            // pthread_mutex_unlock(&mutex_hay_interrupcion);
            pthread_mutex_lock(&mutex_hay_interrupcion);
            hay_interrupcion_quantum = false;
            pthread_mutex_unlock(&mutex_hay_interrupcion);
            // log_warning(cpu_logger, "Interrupcion Quantum");
            log_warning(cpu_logger, "Desalojando hilo por QUANTUM con PID %d TID %d", contexto->PID, contexto->TID);

            // log_info(cpu_logger, "## TID: <%d> - Actualizo Contexto Ejecución", contexto->TID);

            t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
            enviar_contexto_Memoria(paquete);
            t_paquete *otro_paquete = crear_paquete_con_codigo(INTERRUPCION_QUANTUM);
            agregar_a_paquete(otro_paquete->buffer, &contexto->PID, sizeof(uint32_t));
            agregar_a_paquete(otro_paquete->buffer, &contexto->TID, sizeof(uint32_t));
            // log_error(cpu_logger, "Pid: %d, Tid: %d", contexto->PID, contexto->TID);
            enviar_paquete(otro_paquete, FD_SOCKET_K_DISPATCH);
            eliminar_paquete(otro_paquete);

            // log_info(cpu_logger, "Se envio a kernel la interrupcion por quantum");

            while_ciclo = 0;
            return 1;
        }

        // pthread_mutex_unlock(&mutex_hay_interrupcion);
    }
}

void mostrar_contexto()
{
    log_debug(cpu_logger, "[PID: %d] [TID: %d] [PC: %d] [REGISTROS: %u|%u|%u|%u|%u|%u|%u|%u]",
              (contexto->PID),
              (contexto->TID),
              (contexto->PC),
              (contexto->AX),
              (contexto->BX),
              (contexto->CX),
              (contexto->DX),
              (contexto->EX),
              (contexto->FX),
              (contexto->GX),
              (contexto->HX));
}

void fetch()
{

    log_info(cpu_logger, "## TID: <%d> - FETCH - Program Counter: <%d>", contexto->TID, contexto->PC); // log obligatorio Fetch Instrucción

    t_paquete *paquete = crear_paquete_con_codigo(SOLICITUD_INSTRUCCION);
    agregar_a_paquete(paquete->buffer, &contexto->PID, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &contexto->TID, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &contexto->PC, sizeof(uint32_t));
    enviar_paquete(paquete, FD_SOCKET_MEMORIA);
    eliminar_paquete(paquete);
}

void decodeYExecute(char *instruccion, int *while_ciclo)
{

    int i = 0;
    char *delimitador = " ";
    int verificador = 1;

    char *palabra = strtok(instruccion, delimitador);

    while (verificador)
    {
        if (palabra != NULL)
        {
            instruccion_dividida[i] = palabra;
            i++;

            palabra = strtok(NULL, delimitador);
        }
        else
        {
            verificador = 0;
        }
    }

    if (strcmp(instruccion_dividida[0], "SET") == 0)
    { // SET(registro, valor)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s> <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1], instruccion_dividida[2]); // log obligatorio

        contexto->PC++;                                                  // aumenta PC
        uint32_t *registro = detectar_registro(instruccion_dividida[1]); // registro
        *registro = atoi(instruccion_dividida[2]);
        /*        for (int i = 0; i < 4; i++)
               {
                   instruccion_dividida[i] = NULL;
               } */
    }
    else if (strcmp(instruccion_dividida[0], "SUM") == 0)
    { // SUM(registroDestino, registroOrigen)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s> <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1], instruccion_dividida[2]); // log obligatorio

        contexto->PC++; // aumenta PC

        uint32_t *registro_destino = detectar_registro(instruccion_dividida[1]); // registro destino
        uint32_t *registro_origen = detectar_registro(instruccion_dividida[2]);  // registro origen

        *registro_destino = *registro_destino + *registro_origen;
    }
    else if (strcmp(instruccion_dividida[0], "SUB") == 0)
    { // SUB(registroDestino, registroOrigen)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s> <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1], instruccion_dividida[2]); // log obligatorio

        contexto->PC++; // aumenta PC

        uint32_t *registro_destino = detectar_registro(instruccion_dividida[1]); // registro destino
        uint32_t *registro_origen = detectar_registro(instruccion_dividida[2]);  // registro origen

        *registro_destino = *registro_destino - *registro_origen;
    }
    else if (strcmp(instruccion_dividida[0], "JNZ") == 0)
    { // JNZ(registro, instruccion)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s> <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1], instruccion_dividida[2]); // log obligatorio

        uint32_t *registro = detectar_registro(instruccion_dividida[1]); // registro

        if (*registro != 0)
        {
            int valorNuevoPC = atoi(instruccion_dividida[2]);
            contexto->PC = valorNuevoPC;
        }
        else
        {
            contexto->PC++;
        }
    }
    else if (strcmp(instruccion_dividida[0], "READ_MEM") == 0)
    { // READ_MEM(registro datos, registro direccion)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s> <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1], instruccion_dividida[2]); // log obligatorio

        contexto->PC++; // aumenta PC

        uint32_t *registro_datos = detectar_registro(instruccion_dividida[1]);
        uint32_t *registro_direccion = detectar_registro(instruccion_dividida[2]);

        int direccion_logica = *registro_direccion;

        t_paquete *paquete = crear_paquete_con_codigo(SOLICITAR_BASE);

        agregar_a_paquete(paquete->buffer, &contexto->PID, sizeof(uint32_t));

        enviar_paquete(paquete, FD_SOCKET_MEMORIA);
        eliminar_paquete(paquete);

        int size_buffer = -1;
        int base_particion = -1;
        int tamanio_particion = -1;
        int cod_op = -1;

        recv(FD_SOCKET_MEMORIA, &cod_op, sizeof(int), MSG_WAITALL);
        recv(FD_SOCKET_MEMORIA, &size_buffer, sizeof(int), MSG_WAITALL);
        recv(FD_SOCKET_MEMORIA, &base_particion, sizeof(int), MSG_WAITALL);
        recv(FD_SOCKET_MEMORIA, &tamanio_particion, sizeof(int), MSG_WAITALL);

        uint32_t valor = leer_de_memoria(direccion_logica, sizeof(*registro_datos), base_particion, tamanio_particion);

        if (valor >= 0)
        { // si no hubo PF
            // int un_valor = atoi(valor);

            *registro_datos = valor;
        }
        log_info(cpu_logger, "registro: %d", *registro_datos);
    }
    else if (strcmp(instruccion_dividida[0], "WRITE_MEM") == 0)
    { // WRITE_MEM(registro direccion, registro datos)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s> <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1], instruccion_dividida[2]); // log obligatorio

        contexto->PC++; // aumenta PC

        uint32_t *registro_direccion = detectar_registro(instruccion_dividida[1]);
        uint32_t *registro_datos = detectar_registro(instruccion_dividida[2]);

        int direccion_logica = *registro_direccion;

        uint32_t un_valor = *registro_datos;

        // char *valor_a_escribir = malloc(sizeof(char) * 2);
        /* valor_a_escribir[0] = (char)un_valor;
        valor_a_escribir[1] = '\0'; */

        t_paquete *paquete = crear_paquete_con_codigo(SOLICITAR_BASE);

        agregar_a_paquete(paquete->buffer, &contexto->PID, sizeof(uint32_t));

        enviar_paquete(paquete, FD_SOCKET_MEMORIA);
        eliminar_paquete(paquete);

        int size_buffer = -1;
        int cod_op = -1;
        int base_particion = -1;
        int tamanio_particion = -1;

        recv(FD_SOCKET_MEMORIA, &cod_op, sizeof(int), MSG_WAITALL);
        recv(FD_SOCKET_MEMORIA, &size_buffer, sizeof(int), MSG_WAITALL);
        recv(FD_SOCKET_MEMORIA, &base_particion, sizeof(int), MSG_WAITALL);
        recv(FD_SOCKET_MEMORIA, &tamanio_particion, sizeof(int), MSG_WAITALL);

        escribir_en_memoria(direccion_logica, un_valor, base_particion, tamanio_particion);

        // free(valor_a_escribir);
    }
    else if (strcmp(instruccion_dividida[0], "LOG") == 0)
    { // LOG(Registro)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1]);

        uint32_t *registro_direccion = detectar_registro(instruccion_dividida[1]);
        log_info(cpu_logger, "%d", *registro_direccion); // log obligatorio
                                                         // ver valor de registro y escribir
        contexto->PC++;                                  // aumenta PC
    }

    // SYSCALLS - Se va a repetir codigo porque todas se manejan de la misma manera.

    // se deberá actualizar el contexto deejecución en el módulo Memoria y se le deberá
    // devolver el control al Kernel para que este actúe en consecuencia.

    else if (strcmp(instruccion_dividida[0], "DUMP_MEMORY") == 0)
    { // DUMP_MEMORY

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s>", contexto->TID, instruccion_dividida[0]);
        contexto->PC++;

        t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
        enviar_contexto_Memoria(paquete);

        t_paquete *otro_paquete = crear_paquete_con_codigo(DUMP_MEMORY);
        enviar_pid_tid_Kernel(otro_paquete);
        hayQueDesalojar = true;
    }
    else if (strcmp(instruccion_dividida[0], "IO") == 0)
    { // IO (Tiempo)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1]);
        contexto->PC++;

        t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
        enviar_contexto_Memoria(paquete);

        t_paquete *otro_paquete = crear_paquete_con_codigo(IO);
        agregar_a_paquete(otro_paquete->buffer, &contexto->PID, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, &contexto->TID, sizeof(uint32_t));
        uint32_t tiempo = (uint32_t)atoi(instruccion_dividida[1]);
        agregar_a_paquete(otro_paquete->buffer, &tiempo, sizeof(uint32_t));
        // agregar_a_paquete(otro_paquete->buffer, &instruccion_dividida[1], sizeof(uint32_t));

        enviar_paquete(otro_paquete, FD_SOCKET_K_DISPATCH);
        eliminar_paquete(otro_paquete);

        hayQueDesalojar = true;
    }
    else if (strcmp(instruccion_dividida[0], "PROCESS_CREATE") == 0)
    { // PROCESS_CREATE (Archivo de instrucciones, Tamaño, Prioridad del tcb 0)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s> - <%s> - <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1], instruccion_dividida[2], instruccion_dividida[3]);
        contexto->PC++;

        // t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
        // enviar_contexto_Memoria(paquete);

        t_paquete *otro_paquete = crear_paquete_con_codigo(PROCESS_CREATE);
        uint32_t tamanio_archivo_instrucciones = strlen(instruccion_dividida[1]) + 1;
        agregar_a_paquete(otro_paquete->buffer, &contexto->PID, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, &contexto->TID, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, &tamanio_archivo_instrucciones, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, instruccion_dividida[1], tamanio_archivo_instrucciones); // archivo instrucciones
        // log_info(cpu_logger, "El archivo de pseudocodigo: %s", instruccion_dividida[1]);
        uint32_t tamanio_proceso = (uint32_t)atoi(instruccion_dividida[2]);

        agregar_a_paquete(otro_paquete->buffer, (&tamanio_proceso), sizeof(uint32_t));
        uint32_t prioridadTid = (uint32_t)atoi(instruccion_dividida[3]);            // tamanio
        agregar_a_paquete(otro_paquete->buffer, (&prioridadTid), sizeof(uint32_t)); // prioridad tid

        enviar_paquete(otro_paquete, FD_SOCKET_K_DISPATCH);
        eliminar_paquete(otro_paquete);
    }
    else if (strcmp(instruccion_dividida[0], "THREAD_CREATE") == 0)
    { // THREAD_CREATE (Archivo de instrucciones, Prioridad)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s> - <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1], instruccion_dividida[2]);

        contexto->PC++;

        // t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
        // enviar_contexto_Memoria(paquete);

        t_paquete *otro_paquete = crear_paquete_con_codigo(THREAD_CREATE);
        uint32_t tamanio_archivo_instrucciones = strlen(instruccion_dividida[1]) + 1;
        agregar_a_paquete(otro_paquete->buffer, &contexto->PID, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, &contexto->TID, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, &tamanio_archivo_instrucciones, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, instruccion_dividida[1], tamanio_archivo_instrucciones); // archivo instrucciones
        uint32_t prioridad = (uint32_t)atoi(instruccion_dividida[2]);
        agregar_a_paquete(otro_paquete->buffer, &prioridad, sizeof(uint32_t)); // prioridad tcb

        enviar_paquete(otro_paquete, FD_SOCKET_K_DISPATCH);
        eliminar_paquete(otro_paquete);
    }
    else if (strcmp(instruccion_dividida[0], "THREAD_JOIN") == 0)
    { // THREAD_JOIN (tid)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1]);

        contexto->PC++;

        t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
        enviar_contexto_Memoria(paquete);

        t_paquete *otro_paquete = crear_paquete_con_codigo(THREAD_JOIN);
        agregar_a_paquete(otro_paquete->buffer, &contexto->PID, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, &contexto->TID, sizeof(uint32_t));
        uint32_t tid_a_esperar = (uint32_t)atoi(instruccion_dividida[1]);
        agregar_a_paquete(otro_paquete->buffer, &tid_a_esperar, sizeof(uint32_t)); // tid

        enviar_paquete(otro_paquete, FD_SOCKET_K_DISPATCH);
        eliminar_paquete(otro_paquete);

        hayQueDesalojar = true;
    }
    else if (strcmp(instruccion_dividida[0], "THREAD_CANCEL") == 0)
    { // THREAD_CANCEL (tid)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1]);

        contexto->PC++;

        // t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
        // enviar_contexto_Memoria(paquete);

        t_paquete *otro_paquete = crear_paquete_con_codigo(THREAD_CANCEL);
        agregar_a_paquete(otro_paquete->buffer, &contexto->PID, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, &contexto->TID, sizeof(uint32_t));
        uint32_t tid = (uint32_t)atoi(instruccion_dividida[1]);
        agregar_a_paquete(otro_paquete->buffer, &tid, sizeof(uint32_t)); // tid

        enviar_paquete(otro_paquete, FD_SOCKET_K_DISPATCH);
        eliminar_paquete(otro_paquete);
    }
    else if (strcmp(instruccion_dividida[0], "MUTEX_CREATE") == 0)
    { // MUTEX_CREATE (Recurso)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1]);
        contexto->PC++;

        // t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
        // enviar_contexto_Memoria(paquete);

        t_paquete *otro_paquete = crear_paquete_con_codigo(MUTEX_CREATE);
        agregar_a_paquete(otro_paquete->buffer, &contexto->PID, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, &contexto->TID, sizeof(uint32_t));

        uint32_t tamanio_recurso = strlen(instruccion_dividida[1]) + 1;
        agregar_a_paquete(otro_paquete->buffer, &tamanio_recurso, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, instruccion_dividida[1], tamanio_recurso); // recurso
        // log_error(cpu_logger, "El tamaño es: %d", tamanio_recurso);
        // log_error(cpu_logger, "El recurso es: %s", instruccion_dividida[1]);
        enviar_paquete(otro_paquete, FD_SOCKET_K_DISPATCH);
        eliminar_paquete(otro_paquete);
    }
    else if (strcmp(instruccion_dividida[0], "MUTEX_LOCK") == 0)
    { // MUTEX_LOCK(Recurso)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1]);
        contexto->PC++;

        // t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
        // enviar_contexto_Memoria(paquete);

        t_paquete *otro_paquete = crear_paquete_con_codigo(MUTEX_LOCK);
        agregar_a_paquete(otro_paquete->buffer, &contexto->PID, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, &contexto->TID, sizeof(uint32_t));

        uint32_t tamanio_recurso = strlen(instruccion_dividida[1]) + 1;
        agregar_a_paquete(otro_paquete->buffer, &tamanio_recurso, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, instruccion_dividida[1], tamanio_recurso); // recurso

        enviar_paquete(otro_paquete, FD_SOCKET_K_DISPATCH);
        eliminar_paquete(otro_paquete);

        int resultado;
        recv(FD_SOCKET_K_DISPATCH, &resultado, sizeof(int), MSG_WAITALL);
        // log_error(cpu_logger, "El resultado del mutex lock es: %d", resultado);
        if (resultado == 0)
        {

            hayQueDesalojar = true;
            t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
            enviar_contexto_Memoria(paquete);
        }
    }
    else if (strcmp(instruccion_dividida[0], "MUTEX_UNLOCK") == 0)
    { // MUTEX_UNLOCK (Recurso)

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s> - <%s>", contexto->TID, instruccion_dividida[0], instruccion_dividida[1]);

        contexto->PC++;

        // t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
        // enviar_contexto_Memoria(paquete);

        t_paquete *otro_paquete = crear_paquete_con_codigo(MUTEX_UNLOCK);
        agregar_a_paquete(otro_paquete->buffer, &contexto->PID, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, &contexto->TID, sizeof(uint32_t));
        uint32_t tamanio_recurso = strlen(instruccion_dividida[1]) + 1;
        agregar_a_paquete(otro_paquete->buffer, &tamanio_recurso, sizeof(uint32_t));
        agregar_a_paquete(otro_paquete->buffer, instruccion_dividida[1], tamanio_recurso); // recurso

        // log_error(cpu_logger, "El tamaño es: %d", tamanio_recurso);
        // log_error(cpu_logger, "El recurso es: %s", instruccion_dividida[1]);

        enviar_paquete(otro_paquete, FD_SOCKET_K_DISPATCH);
        eliminar_paquete(otro_paquete);
    }
    else if (strcmp(instruccion_dividida[0], "THREAD_EXIT") == 0)
    { // THREAD_EXIT

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s>", contexto->TID, instruccion_dividida[0]);
        contexto->PC++;

        // t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
        // enviar_contexto_Memoria(paquete);

        t_paquete *otro_paquete = crear_paquete_con_codigo(THREAD_EXIT);
        enviar_pid_tid_Kernel(otro_paquete);

        hayQueDesalojar = true;
    }
    else if (strcmp(instruccion_dividida[0], "PROCESS_EXIT") == 0)
    { // PROCESS_EXIT

        log_info(cpu_logger, "## TID: <%d> - Ejecutando: <%s>", contexto->TID, instruccion_dividida[0]);

        contexto->PC++;

        // t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
        // enviar_contexto_Memoria(paquete);

        t_paquete *otro_paquete = crear_paquete_con_codigo(PROCESS_EXIT);
        enviar_pid_tid_Kernel(otro_paquete);

        hayQueDesalojar = true;
    }
    else
    {
        // Cuando no se reconoce el comando
        log_error(cpu_logger, "Instruccion no reconocida - %s", instruccion_dividida[0]);
        contexto->PC++;

        t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
        enviar_contexto_Memoria(paquete);

        t_paquete *otro_paquete = crear_paquete_con_codigo(PROCESS_EXIT);
        enviar_pid_tid_Kernel(otro_paquete);

        hayQueDesalojar = true;
    }
}

uint32_t *detectar_registro(char *registro)
{

    if (strcmp(registro, "AX") == 0)
    {
        return &(contexto->AX);
    }

    else if (strcmp(registro, "BX") == 0)
    {
        return &(contexto->BX);
    }

    else if (strcmp(registro, "CX") == 0)
    {
        return &(contexto->CX);
    }

    else if (strcmp(registro, "DX") == 0)
    {
        return &(contexto->DX);
    }

    if (strcmp(registro, "EX") == 0)
    {
        return &(contexto->EX);
    }

    else if (strcmp(registro, "FX") == 0)
    {
        return &(contexto->FX);
    }

    else if (strcmp(registro, "GX") == 0)
    {
        return &(contexto->GX);
    }

    else if (strcmp(registro, "HX") == 0)
    {
        return &(contexto->HX);
    }
}

void liberar_instruccion_dividida()
{

    int i = 0;
    for (int i = 0; i < 4; i++)
    {
        instruccion_dividida[i] = NULL;
    }
    /*     while (instruccion_dividida[i] != NULL)
        {

            free(instruccion_dividida[i]);
            i++;
        } */
}

// ----------------------- Manejo con Memoria -------------------------------------

uint32_t leer_de_memoria(int direccion_logica, int tamanio, int base_particion, int tamanio_particion)
{ // listo

    int direccion_fisica = traducir_direccion(direccion_logica, base_particion, tamanio_particion);

    if (direccion_fisica != -1)
    {
        t_paquete *paquete = crear_paquete_con_codigo(READ_MEM);
        agregar_a_paquete(paquete->buffer, &contexto->PID, sizeof(uint32_t));
        agregar_a_paquete(paquete->buffer, &contexto->TID, sizeof(uint32_t));
        agregar_a_paquete(paquete->buffer, &direccion_fisica, sizeof(uint32_t));
        enviar_paquete(paquete, FD_SOCKET_MEMORIA);
        eliminar_paquete(paquete);

        uint32_t valor_recibido;
        // void *respuesta_lectura = malloc(4);
        recv(FD_SOCKET_MEMORIA, &valor_recibido, 4, MSG_WAITALL);
        // log_info(cpu_logger, "La respuesta de la lectura es: %s", respuesta_lectura);

        // memcpy(&valor_recibido, respuesta_lectura, sizeof(uint32_t));
        log_debug(cpu_logger, "El valor leido desde la memoria es: %d", valor_recibido);
        if (valor_recibido >= 0)
        {
            log_info(cpu_logger, "## TID: <%d> - Accion: LEER - Direccion Fisica: <%d> ", contexto->TID, direccion_fisica);

            // free(respuesta_lectura);
            return valor_recibido;
        }
        else
        {
            log_warning(cpu_logger, "No se pudo leer");
            return "ERROR";
        }
    }
}

void escribir_en_memoria(int direccion_logica, uint32_t valor_a_escribir, int base_particion, int tamanio_particion)
{
    int res;
    int direccion_fisica = traducir_direccion(direccion_logica, base_particion, tamanio_particion);

    if (direccion_fisica != -1)
    {

        t_paquete *paquete = crear_paquete_con_codigo(WRITE_MEM);
        // uint32_t tamanio_valor_a_escribir = strlen(valor_a_escribir) + 1;
        agregar_a_paquete(paquete->buffer, &contexto->PID, sizeof(uint32_t));
        agregar_a_paquete(paquete->buffer, &contexto->TID, sizeof(uint32_t));
        agregar_a_paquete(paquete->buffer, &direccion_fisica, sizeof(uint32_t));
        // agregar_a_paquete(paquete->buffer, &tamanio_valor_a_escribir, sizeof(uint32_t));
        agregar_a_paquete(paquete->buffer, &valor_a_escribir, sizeof(uint32_t));

        enviar_paquete(paquete, FD_SOCKET_MEMORIA);

        log_debug(cpu_logger, "Mando a memoria: %d", valor_a_escribir);
        eliminar_paquete(paquete);

        recv(FD_SOCKET_MEMORIA, &res, sizeof(int), MSG_WAITALL);

        // log_info(cpu_logger, "Resultado: %d", res);

        if (res == 1)
        {
            log_info(cpu_logger, "## TID: <%d> - Accion: ESCRIBIR - Direccion Fisica: <%d> ", contexto->TID, direccion_fisica);
        }
        else
        {
            log_warning(cpu_logger, "No se pudo escribir");
        }
    }
    // sem_wait(&sem_solicitud_escritura);
}

void enviar_contexto_Memoria(t_paquete *paquete)
{
    log_info(cpu_logger, "## TID: <%d> - Actualizo Contexto Ejecución", contexto->TID);
    int res;
    agregar_a_paquete(paquete->buffer, &contexto->PID, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &contexto->TID, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &contexto->PC, sizeof(uint32_t));

    agregar_a_paquete(paquete->buffer, &contexto->AX, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &contexto->BX, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &contexto->CX, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &contexto->DX, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &contexto->EX, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &contexto->FX, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &contexto->GX, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &contexto->HX, sizeof(uint32_t));

    enviar_paquete(paquete, FD_SOCKET_MEMORIA);
    eliminar_paquete(paquete);
    // log_debug(cpu_logger, "Envio contexto a memoria");
    recv(FD_SOCKET_MEMORIA, &res, sizeof(int), MSG_WAITALL);

    if (res = 1)
    {
        log_debug(cpu_logger, "La memoria pudo guardar el contexto");
    }
    else
    {
        log_info(cpu_logger, "La memoria no pudo guardar el contexto");
    }
}

// ----------------------- Manejo con Kernel -------------------------------------

void enviar_pid_tid_Kernel(t_paquete *paquete)
{

    agregar_a_paquete(paquete->buffer, &contexto->PID, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &contexto->TID, sizeof(uint32_t));

    enviar_paquete(paquete, FD_SOCKET_K_DISPATCH);
    eliminar_paquete(paquete);
    // log_error(cpu_logger, "Se le envia la respuesta a kernel");
}

// ----------------------- Traduccion MMU ----------------------------------------

int traducir_direccion(int direccion_logica, int base_particion, int tamanio_particion)
{ // creo que es asi simplemente
    // Validar que la dirección lógica esté dentro de los límites de la partición
    if (direccion_logica + 4 > tamanio_particion)
    {
        log_error(cpu_logger, "Segmentation Fault - Direccion fuera de los limites de la particion");

        t_paquete *paquete = crear_paquete_con_codigo(GUARDAR_CONTEXTO);
        enviar_contexto_Memoria(paquete);

        t_paquete *otro_paquete = crear_paquete_con_codigo(SEGMENTATION_FAULT);
        enviar_pid_tid_Kernel(otro_paquete);

        // Señalar al kernel que ocurrió un Segmentation Fault
        hayQueDesalojar = true;
        return -1; // Indicar Segmentation Fault
    }

    // Traducción de dirección lógica a dirección física
    int direccion_fisica = base_particion + direccion_logica;

    return direccion_fisica;
}