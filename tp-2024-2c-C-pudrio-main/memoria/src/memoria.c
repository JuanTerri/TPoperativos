#include <utils/utils.h>
#include <memoria.h>
#include <commons/memory.h>

// la funcion read_config se ejecuta cuando se ejecuta initialize_config. La funcion initialize_config
// esta en el utils.c

int main(int argc, char *argv[])
{
    // pthread_t hilo_cliente_conexion_memoria;

    pthread_t hilo_servidor_memoria;

    LOGGER = log_create(LOGGER_PATH, MODULE_NAME, true, LOG_LEVEL_DEBUG);
    inicializar_config(MODULE_CONFIG_PATH);

    lista_tcbs = list_create();

    particiones = esquema_particiones();

    MEMORIA = malloc(MEMORY_SIZE);
    memset(MEMORIA, 0, MEMORY_SIZE);
    // mem_hexdump(MEMORIA, MEMORY_SIZE);

    // info_conexion *info_conexion_filesystem = crear_conexion_cliente(IP_FILESYSTEM, FILESYSTEM_PORT, LOGGER);
    info_conexion_servidor *info_servidor_memoria = crear_conexion_servidor(LISTENING_PORT, LOGGER);

    // pthread_create(&hilo_cliente_conexion_memoria, NULL, (void *)inicializar_cliente_memoria, info_conexion_filesystem);
    pthread_create(&hilo_servidor_memoria, NULL, (void *)inicializar_server_memoria, info_servidor_memoria);

    // pthread_join(hilo_cliente_conexion_memoria, NULL);
    pthread_join(hilo_servidor_memoria, NULL);

    return 0;
}

void read_config(t_config *MODULE_CONFIG)
{
    IP_FILESYSTEM = config_get_string_value(MODULE_CONFIG, "IP_FILESYSTEM");
    FILESYSTEM_PORT = config_get_string_value(MODULE_CONFIG, "PUERTO_FILESYSTEM");
    LISTENING_PORT = config_get_string_value(MODULE_CONFIG, "PUERTO_ESCUCHA");
    MEMORY_SIZE = config_get_int_value(MODULE_CONFIG, "TAM_MEMORIA");
    PATH_INSTRUCTIONS = config_get_string_value(MODULE_CONFIG, "PATH_INSTRUCCIONES");
    RESPONSE_DELAY = config_get_int_value(MODULE_CONFIG, "RETARDO_RESPUESTA");
    SCHEME = config_get_string_value(MODULE_CONFIG, "ESQUEMA");
    SEARCH_ALGORITHM = config_get_string_value(MODULE_CONFIG, "ALGORITMO_BUSQUEDA");
    PARTITIONS = config_get_array_value(MODULE_CONFIG, "PARTICIONES");
    LOG_LEVEL = config_get_string_value(MODULE_CONFIG, "LOG_LEVEL");
}

t_list *esquema_particiones()
{
    if (strcmp(SCHEME, "FIJAS") == 0)
    {
        log_debug(LOGGER, "La estructura de la memoria es de particiones fijas");
        int i = 0;
        t_list *particiones = list_create();

        int base_anterior = 0;
        int tamanio_anterior = 0;
        while (PARTITIONS[i] != NULL)
        {
            // log_debug(LOGGER, "Array posicion %d tiene el valor: %s", i, PARTITIONS[i]);
            particion_memoria *particion = malloc(sizeof(particion_memoria));
            // creo una lista de particiones para poder manejarlo con una estructura
            particion->tamanio = atoi(PARTITIONS[i]);
            particion->base = base_anterior + tamanio_anterior;
            particion->ocupado = false;
            list_add(particiones, particion);
            // me muevo al proximo elemento
            base_anterior = particion->base;
            tamanio_anterior = particion->tamanio;
            i++;
        }
        return particiones;

        // particion_memoria particiones[i];
        /*         for(int c=0; c<list_size(particiones); c++){
                    particion_memoria* particion = list_get(particiones, c);
                    log_info(LOGGER, "El valor de la base de la posicione %d es %d",c , particion->base);
                    log_info(LOGGER, "El valor de tamanio de la posicione %d es %d",c , particion->tamanio);
                    log_info(LOGGER, "La particion %d es %d",c , particion->ocupado);
                } */
    }
    else if (strcmp(SCHEME, "DINAMICAS") == 0)
    {
        log_debug(LOGGER, "La estructura de la memoria es de particiones dinamicas");
        t_list *particiones = list_create();
        particion_memoria *particion = malloc(sizeof(particion_memoria));
        particion->base = 0;
        particion->tamanio = MEMORY_SIZE;
        particion->ocupado = false;
        list_add(particiones, particion);
        return particiones;
    }
    else
    {
        log_error(LOGGER, "No se reconoce el esquema de particiones");
    }
}

bool esta_libre(particion_memoria *particion)
{
    return !particion->ocupado;
}

bool particion_menor(particion_memoria *a, particion_memoria *b)
{
    return a->tamanio <= b->tamanio;
}

bool particion_mayor(particion_memoria *a, particion_memoria *b)
{
    return a->tamanio >= b->tamanio;
}

particion_memoria *first_fit(int tamanio_a_asignar, t_list *particiones)
{
    for (int i = 0; i < list_size(particiones); i++)
    {
        particion_memoria *particion = list_get(particiones, i);
        if (!particion->ocupado && particion->tamanio >= tamanio_a_asignar)
        {
            return particion;
        }
    }
    // return -1;
    return NULL;
}

particion_memoria *best_fit(int tamanio_a_asignar, t_list *particiones)
{
    t_list *particiones_libres = list_filter(particiones, esta_libre);
    list_sort(particiones_libres, particion_menor);
    for (int i = 0; i < list_size(particiones_libres); i++)
    {
        particion_memoria *particion = list_get(particiones_libres, i);
        if (particion->tamanio >= tamanio_a_asignar)
        {
            return particion;
        }
    }
    // return -1;
    return NULL;
}

particion_memoria *worst_fit(int tamanio_a_asignar, t_list *particiones)
{
    t_list *particiones_libres = list_filter(particiones, esta_libre);
    list_sort(particiones_libres, particion_mayor);
    for (int i = 0; i < list_size(particiones_libres); i++)
    {
        particion_memoria *particion = list_get(particiones_libres, i);
        if (particion->tamanio >= tamanio_a_asignar)
        {
            return particion;
        }
    }
    // return -1;
    return NULL;
}

particion_memoria *algoritmo_busqueda(int tamanio, t_list *particiones)
{
    particion_memoria *realizado;
    if (strcmp(SEARCH_ALGORITHM, "BEST") == 0)
    {
        log_debug(LOGGER, "El algoritmo de búsqueda es BEST FIT");
        realizado = best_fit(tamanio, particiones);
        return realizado;
    }
    else if (strcmp(SEARCH_ALGORITHM, "WORST") == 0)
    {
        log_debug(LOGGER, "El algoritmo de búsqueda es WORST FIT");
        realizado = worst_fit(tamanio, particiones);
        return realizado;
    }
    else if (strcmp(SEARCH_ALGORITHM, "FIRST") == 0)
    {
        log_debug(LOGGER, "El algoritmo de búsqueda es FIRST FIT");
        realizado = first_fit(tamanio, particiones);
        return realizado;
    }
    else
    {
        log_error(LOGGER, "No se reconoce el algoritmo de busqueda");
        return -1;
    }
}

void inicializar_server_memoria(info_conexion_servidor *info)
{

    int err;
    size_t bytes;
    int32_t handshake;
    int32_t result_ok = 1;
    int32_t result_error = -1;
    struct addrinfo hints, *server_info;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    err = getaddrinfo(NULL, info->puerto, &hints, &server_info);
    if (err != 0)
    {
        log_error(info->logger, "No se pudo hacer el getaddrinfo");
    }

    int fd_listen = socket(server_info->ai_family,
                           server_info->ai_socktype,
                           server_info->ai_protocol);

    if (fd_listen == -1)
    {
        log_error(info->logger, "Error al hacer el socket");
    }

    err = setsockopt(fd_listen, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));

    if (err != 0)
    {
        log_error(info->logger, "No se pudo hacer el setsockopt");
    }

    err = bind(fd_listen, server_info->ai_addr, server_info->ai_addrlen);

    if (err != 0)
    {
        log_error(info->logger, "Error con el bind");
    }

    err = listen(fd_listen, SOMAXCONN);

    if (err != 0)
    {
        log_error(info->logger, "Error con el listen");
    }

    log_info(info->logger, "Servidor escuchando en el puerto %s", info->puerto);

    while (1)
    {
        pthread_t thread;
        int *fd_conexion_ptr = malloc(sizeof(int));
        *fd_conexion_ptr = accept(fd_listen, NULL, NULL);
        pthread_create(&thread,
                       NULL,
                       (void *)atender_cliente_memoria,
                       fd_conexion_ptr);
        pthread_detach(thread);
    }
    freeaddrinfo(server_info);
}

void atender_cliente_memoria(int *fd_conexion)
{
    int32_t handshake_cod = -1;
    int bytes = recv(*fd_conexion, &handshake_cod, sizeof(int32_t), MSG_WAITALL);

    if (handshake_cod == -1)
    {
        log_error(LOGGER, "No se envio el handshake");
    }
    else
    {
        int32_t handshake = 0;
        int32_t result_ok = 1;
        int32_t result_err = -1;
        bytes = recv(*fd_conexion, &handshake, sizeof(int32_t), MSG_WAITALL);

        switch (handshake)
        {
        case CLI_KERNEL:
            log_info(LOGGER, "## Kernel Conectado - FD del socekt: <%d>", *fd_conexion);
            bytes = send(*fd_conexion, &result_ok, sizeof(int32_t), 0);
            atender_peticion(fd_conexion);
            break;
        case CLI_CPU:
            log_debug(LOGGER, "Conexion a memoria recibida CPU");
            bytes = send(*fd_conexion, &result_ok, sizeof(int32_t), 0);
            while (1)
            {
                // log_info(LOGGER, "Voy a atender peticion de cpu");
                atender_peticion(fd_conexion);
            }
            break;
        default:
            log_debug(LOGGER, "No se reconoce cliente");
            bytes = send(*fd_conexion, &result_err, sizeof(int32_t), 0);
        }
    }
}

void atender_peticion(int *fd_conexion)
{
    int cod_op;
    int result;
    // log_debug(LOGGER, "Esperando para recibir codigo de operacion");
    recv(*fd_conexion, &cod_op, sizeof(int), MSG_WAITALL);
    // log_info(LOGGER, "codigo de operacion: %d", cod_op);
    switch (cod_op)
    {

    // Comunucacion con CPU
    case SOLICITUD_INSTRUCCION:
        log_debug(LOGGER, "La CPU solicitó una instrucción");
        usleep(RESPONSE_DELAY * 1000);
        procesar_solicitar_instruccion(fd_conexion);
        break;

    case GUARDAR_CONTEXTO:
        log_debug(LOGGER, "La CPU solicitó guardar el contexto de un hilo");
        usleep(RESPONSE_DELAY * 1000);
        result = procesar_guardar_contexto(fd_conexion);
        send(*fd_conexion, &result, sizeof(int), 0);
        break;

    case SOLICITAR_CONTEXTO:
        log_debug(LOGGER, "La CPU solicitó el contexto de un hilo");
        usleep(RESPONSE_DELAY * 1000);
        procesar_solicitar_contexto(fd_conexion);
        break;

    case SOLICITAR_BASE:
        log_debug(LOGGER, "La CPU solicitó la base de la particion del proceso");
        procesar_solicitar_base(fd_conexion);
        break;

    // Para los Read y Write primero solicitar base y offset. Despues enviar paquete con alguno de estos codigos
    case READ_MEM:
        log_debug(LOGGER, "La CPU solicitó leer la memoria");
        usleep(RESPONSE_DELAY * 1000);
        procesar_leer_memoria(fd_conexion);
        break;

    case WRITE_MEM:
        log_debug(LOGGER, "La CPU solicitó escribir en la memoria");
        usleep(RESPONSE_DELAY * 1000);
        result = procesar_escribir_memoria(fd_conexion);
        // log_info(LOGGER, "Resultado: %d", result);
        send(*fd_conexion, &result, sizeof(int), 0);
        break;

    // Comunicacion con kernel
    case PROCESS_CREATE:
        // log_info(LOGGER, "## Kernel Conectado - FD del socekt: <%d>", *fd_conexion);
        log_debug(LOGGER, "El kernel solicitó crear un proceso");
        result = procesar_process_create(fd_conexion); // Envio un 1 si se hace o un 0 si no se hace
        send(*fd_conexion, &result, sizeof(int), 0);
        break;

    case DUMP_MEMORY:
        // log_info(LOGGER, "## Kernel Conectado - FD del socekt: <%d>", *fd_conexion);
        log_debug(LOGGER, "El kernel solicitó dumpear la memoria");
        result = procesar_dump(fd_conexion);
        send(*fd_conexion, &result, sizeof(int), 0);
        break;

    case THREAD_CREATE:
        // log_info(LOGGER, "## Kernel Conectado - FD del socekt: <%d>", *fd_conexion);
        log_debug(LOGGER, "El kernel solicitó crear un hilo");
        result = procesar_crear_hilo(fd_conexion);
        send(*fd_conexion, &result, sizeof(int), 0);
        break;

    case THREAD_CANCEL:
        // log_info(LOGGER, "## Kernel Conectado - FD del socekt: <%d>", *fd_conexion);
        log_debug(LOGGER, "El kernel solicitó eliminar un hilo");
        result = procesar_eliminar_hilo(fd_conexion);
        send(*fd_conexion, &result, sizeof(int), 0);
        break;

    case THREAD_EXIT:
        // log_info(LOGGER, "## Kernel Conectado - FD del socekt: <%d>", *fd_conexion);
        log_debug(LOGGER, "El kernel solicitó eliminar un hilo");
        result = procesar_eliminar_hilo(fd_conexion);
        send(*fd_conexion, &result, sizeof(int), 0);
        break;

    case PROCESS_EXIT:
        // log_info(LOGGER, "## Kernel Conectado - FD del socekt: <%d>", *fd_conexion);
        log_debug(LOGGER, "El kernel solicitó eliminar un proceso");
        procesar_eliminar_proceso(fd_conexion);
        break;
    default:
        log_debug(LOGGER, "No se reconoce el codigo de operacion");
        break;
    }
}

int procesar_process_create(int *fd_conexion)
{
    int size_buffer = -1;
    uint32_t PID;
    uint32_t tamanio;
    uint32_t tamanio_archivo;

    t_list *lista_instrucciones = list_create();

    recv(*fd_conexion, &size_buffer, sizeof(int), MSG_WAITALL);

    recv(*fd_conexion, &PID, sizeof(uint32_t), MSG_WAITALL);

    recv(*fd_conexion, &tamanio, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &tamanio_archivo, sizeof(uint32_t), MSG_WAITALL);
    char *archivo_pseudocodigo = malloc(tamanio_archivo);
    recv(*fd_conexion, archivo_pseudocodigo, tamanio_archivo, MSG_WAITALL);

    log_debug(LOGGER, "El archivo de pseudocodigo es: %s", archivo_pseudocodigo);

    particion_memoria *particion_asignada = asignar_memoria(particiones, tamanio, PID);

    if (particion_asignada == NULL)
    {
        log_error(LOGGER, "No se pudo cargar el proceso a la memoria");
        return 0;
    }
    else
    {
        tcb_memoria *tcb = malloc(sizeof(tcb_memoria));
        tcb->PID = PID;
        tcb->TID = 0;
        tcb->AX = 0;
        tcb->BX = 0;
        tcb->CX = 0;
        tcb->DX = 0;
        tcb->EX = 0;
        tcb->FX = 0;
        tcb->GX = 0;
        tcb->HX = 0;
        tcb->PC = 0;

        char *instrucciones_de_proceso = concatenar_strings(PATH_INSTRUCTIONS, archivo_pseudocodigo);
        FILE *archivo_instrucciones = fopen(instrucciones_de_proceso, "r");

        if (archivo_instrucciones == NULL)
        {
            log_error(LOGGER, "Error al abrir el archivo de instrucciones");
            return 1;
        }

        leer_instrucciones(archivo_instrucciones, lista_instrucciones);

        tcb->lista_instrucciones = lista_instrucciones;
        tcb->base = particion_asignada->base;
        tcb->limite = particion_asignada->tamanio;

        list_add(lista_tcbs, tcb);
        // imprimir_instrucciones(lista_instrucciones, list_size(lista_instrucciones));

        // free(archivo_pseudocodigo);
        log_info(LOGGER, "## Proceso Creado - PID: <%d> - Tamaño: <%d>", PID, tamanio);
        for (int i = 0; i < list_size(particiones); i++)
        {

            particion_memoria *particion = list_get(particiones, i);
            log_debug(LOGGER, "\nparticion [%d], ocupado: %d, base: %d, tamanio: %d\n", i, particion->ocupado, particion->base, particion->tamanio);
        }

        return 1;
    }
    /* log_info(LOGGER, "size_buffer: %d", size_buffer);
    log_info(LOGGER, "pid: %d", pid);
    log_info(LOGGER, "tamanio proceso: %d", tamanio);
    log_info(LOGGER, "tamanio archivo: %d", tamanio_archivo);
    for (int i = 0; i < tamanio_archivo; i++)
    {

        log_info(LOGGER, "archivo byte %d dice %c", i, archivo_pseudocodigo[i]);
    } */
}

void procesar_solicitar_instruccion(int *fd_conexion)
{

    usleep(RESPONSE_DELAY * 1000);
    uint32_t PID = -1;
    uint32_t TID = -1;
    uint32_t PC = -1;
    int size_buffer = -1;
    recv(*fd_conexion, &size_buffer, sizeof(int), MSG_WAITALL);
    recv(*fd_conexion, &PID, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &TID, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &PC, sizeof(uint32_t), MSG_WAITALL);

    tcb_memoria *tcb_con_valores = malloc(sizeof(tcb_memoria));
    tcb_con_valores->PID = PID;
    tcb_con_valores->TID = TID;
    bool coincide_pid_tid(tcb_memoria * tcb)
    {
        return tcb->PID == tcb_con_valores->PID && tcb->TID == tcb_con_valores->TID;
    }

    tcb_memoria *tcb_existente = list_find(lista_tcbs, coincide_pid_tid);
    char *instruccion = list_get(tcb_existente->lista_instrucciones, PC);
    t_paquete *paquete = crear_paquete_con_codigo(ENVIO_PAQUETE);
    // serializar_tamanio(paquete->buffer, strlen(instruccion));
    uint32_t tamanio_instruccion = strlen(instruccion) + 1;
    agregar_a_paquete(paquete->buffer, &tamanio_instruccion, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, instruccion, tamanio_instruccion);
    enviar_paquete(paquete, *fd_conexion);

    log_info(LOGGER, "## Obtener instruccion - (PID:TID) - (<%d>:<%d>) - Instruccion: <%s>", PID, TID, instruccion);

    eliminar_paquete(paquete);

    free(tcb_con_valores);
}

int procesar_guardar_contexto(int *fd_conexion)
{

    usleep(RESPONSE_DELAY * 1000);
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

    recv(*fd_conexion, &size_buffer, sizeof(int), MSG_WAITALL);
    recv(*fd_conexion, &PID, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &TID, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &PC, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &AX, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &BX, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &CX, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &DX, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &EX, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &FX, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &GX, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &HX, sizeof(uint32_t), MSG_WAITALL);

    tcb_memoria *tcb_con_valores = malloc(sizeof(tcb_memoria));
    tcb_con_valores->PID = PID;
    tcb_con_valores->TID = TID;
    bool coincide_pid_tid(tcb_memoria * tcb)
    {
        return tcb->PID == tcb_con_valores->PID && tcb->TID == tcb_con_valores->TID;
    }

    tcb_memoria *tcb_existente = list_find(lista_tcbs, coincide_pid_tid);
    if (tcb_existente == NULL)
    {
        log_info(LOGGER, "No se halla un hilo en la lista");
        return 0;
    }

    tcb_existente->AX = AX;
    tcb_existente->BX = BX;
    tcb_existente->CX = CX;
    tcb_existente->DX = DX;
    tcb_existente->EX = EX;
    tcb_existente->FX = FX;
    tcb_existente->HX = HX;
    tcb_existente->GX = GX;
    tcb_existente->PC = PC;

    log_info(LOGGER, "## Contexto Actualizado - (PID:TID) - (<%d>:<%d>)", PID, TID);
    free(tcb_con_valores);
    return 1;
}

void procesar_solicitar_contexto(int *fd_conexion)
{
    usleep(RESPONSE_DELAY * 1000);
    int size_buffer = -1;
    uint32_t PID = -1;
    uint32_t TID = -1;

    recv(*fd_conexion, &size_buffer, sizeof(int), MSG_WAITALL);
    // log_info(LOGGER, "1 %d", size_buffer);
    recv(*fd_conexion, &PID, sizeof(uint32_t), MSG_WAITALL);
    // log_info(LOGGER, "2");
    recv(*fd_conexion, &TID, sizeof(uint32_t), MSG_WAITALL);
    // log_info(LOGGER, "3");

    // log_info(LOGGER, "EL Pid QUE solicito es: %d", PID);
    // log_info(LOGGER, "EL Tid QUE solicito es: %d", TID);

    tcb_memoria *tcb_con_valores = malloc(sizeof(tcb_memoria));
    tcb_con_valores->PID = PID;
    tcb_con_valores->TID = TID;
    bool coincide_pid_tid(tcb_memoria * tcb)
    {
        return tcb->PID == tcb_con_valores->PID && tcb->TID == tcb_con_valores->TID;
    }

    tcb_memoria *tcb_existente = list_find(lista_tcbs, coincide_pid_tid);

    if (tcb_existente == NULL)
    {
        log_info(LOGGER, "No se encontro el tcb que se buscaba");
        return 1;
    }

    t_paquete *paquete = crear_paquete_con_codigo(ENVIO_PAQUETE);

    agregar_a_paquete(paquete->buffer, &tcb_existente->PC, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &tcb_existente->AX, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &tcb_existente->BX, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &tcb_existente->CX, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &tcb_existente->DX, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &tcb_existente->EX, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &tcb_existente->FX, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &tcb_existente->GX, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &tcb_existente->HX, sizeof(uint32_t));

    log_info(LOGGER, "## Contexto Solicitado - (PID:TID) - (<%d>:<%d>)", PID, TID);

    enviar_paquete(paquete, *fd_conexion);
    eliminar_paquete(paquete);
    free(tcb_con_valores);
}

int procesar_crear_hilo(int *fd_conexion)
{
    // Espero PID, TID, tamaño del string archivo y el string del archivo en sí. Deberia buscar un PID igual y asignarle mismo base y tamaño
    int size_buffer = -1;
    uint32_t PID = -1;
    uint32_t TID = -1;
    uint32_t tamanio_archivo = -1;
    t_list *lista_instrucciones = list_create();
    tcb_memoria *nuevo_tcb = malloc(sizeof(tcb_memoria));

    recv(*fd_conexion, &size_buffer, sizeof(int), MSG_WAITALL);

    recv(*fd_conexion, &PID, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &TID, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &tamanio_archivo, sizeof(int), MSG_WAITALL);
    char *archivo_pseudocodigo = malloc(tamanio_archivo);
    recv(*fd_conexion, archivo_pseudocodigo, tamanio_archivo, MSG_WAITALL);

    nuevo_tcb->PID = PID;
    nuevo_tcb->TID = TID;
    nuevo_tcb->AX = 0;
    nuevo_tcb->BX = 0;
    nuevo_tcb->CX = 0;
    nuevo_tcb->DX = 0;
    nuevo_tcb->EX = 0;
    nuevo_tcb->FX = 0;
    nuevo_tcb->GX = 0;
    nuevo_tcb->HX = 0;
    nuevo_tcb->PC = 0;

    bool coincide_pid(tcb_memoria * tcb)
    {
        return tcb->PID == nuevo_tcb->PID;
    }
    tcb_memoria *tcb_existente = list_find(lista_tcbs, coincide_pid);

    nuevo_tcb->base = tcb_existente->base;
    nuevo_tcb->limite = tcb_existente->limite;

    char *instrucciones_de_proceso = concatenar_strings(PATH_INSTRUCTIONS, archivo_pseudocodigo);

    FILE *archivo_instrucciones = fopen(instrucciones_de_proceso, "r");
    if (archivo_instrucciones == NULL)
    {
        perror("Error al abrir el archivo de instrucciones");
        return 1;
    }
    leer_instrucciones(archivo_instrucciones, lista_instrucciones);

    nuevo_tcb->lista_instrucciones = lista_instrucciones;

    list_add(lista_tcbs, nuevo_tcb);

    // free(archivo_pseudocodigo);

    log_info(LOGGER, "## Hilo Creado - (PID:TID) - (<%d>:<%d>)", PID, TID);

    return 1;
}

int procesar_eliminar_hilo(int *fd_conexion)
{
    // Espero PID y TID para saber cual eliminar
    int size_buffer = -1;
    uint32_t PID = -1;
    uint32_t TID = -1;
    bool fue_borrado;

    recv(*fd_conexion, &size_buffer, sizeof(int), MSG_WAITALL);
    recv(*fd_conexion, &PID, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &TID, sizeof(uint32_t), MSG_WAITALL);

    tcb_memoria *tcb_con_valores = malloc(sizeof(tcb_memoria));
    tcb_con_valores->PID = PID;
    tcb_con_valores->TID = TID;
    bool coincide_pid_tid(tcb_memoria * tcb)
    {
        return tcb->PID == tcb_con_valores->PID && tcb->TID == tcb_con_valores->TID;
    }

    tcb_memoria *tcb_existente = list_find(lista_tcbs, coincide_pid_tid);

    fue_borrado = list_remove_element(lista_tcbs, tcb_existente);
    if (fue_borrado)
    {
        free(tcb_existente);
        free(tcb_con_valores);
        log_info(LOGGER, "## Hilo Destruido - (PID:TID) - (<%d>:<%d>)", PID, TID);
        return fue_borrado;
    }
    else
    {
        log_info(LOGGER, "No se pudo eliminar el hilo");
    }
}

int procesar_dump(int *fd_conexion)
{

    int size_buffer = -1;
    uint32_t PID = -1;
    uint32_t TID = -1;
    uint32_t tamanio_de_proceso; // se lo mando yo al file system
    int resultado;

    recv(*fd_conexion, &size_buffer, sizeof(int), MSG_WAITALL);
    recv(*fd_conexion, &PID, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &TID, sizeof(uint32_t), MSG_WAITALL);

    log_info(LOGGER, "## Memory Dump Solicitado - (PID:TID) - (<%d>:<%d>)", PID, TID);

    tcb_memoria *tcb_con_valores = malloc(sizeof(tcb_memoria));
    tcb_con_valores->PID = PID;

    bool coincide_pid_en_particiones(particion_memoria * particion)
    {
        return particion->PID == tcb_con_valores->PID;
    }

    particion_memoria *particion_ocupada = list_find(particiones, coincide_pid_en_particiones);
    tamanio_de_proceso = particion_ocupada->tamanio;
    char *contenido = malloc(tamanio_de_proceso);
    memcpy(contenido, MEMORIA + particion_ocupada->base, tamanio_de_proceso);

    // Inicio conexion con el FS
    int FD_SOCKET_FILESYSTEM;
    info_conexion *info_conexion_filesystem = crear_conexion_cliente(IP_FILESYSTEM, FILESYSTEM_PORT, LOGGER);
    FD_SOCKET_FILESYSTEM = inicializar_cliente(info_conexion_filesystem, CLI_MEMORIA);

    t_paquete *paquete = crear_paquete_con_codigo(DUMP_MEMORY);
    agregar_a_paquete(paquete->buffer, &PID, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &TID, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, &tamanio_de_proceso, sizeof(uint32_t));
    agregar_a_paquete(paquete->buffer, contenido, tamanio_de_proceso);
    enviar_paquete(paquete, FD_SOCKET_FILESYSTEM);

    eliminar_paquete(paquete);

    recv(FD_SOCKET_FILESYSTEM, &resultado, sizeof(int), MSG_WAITALL);

    liberar_conexion(FD_SOCKET_FILESYSTEM);

    free(tcb_con_valores);
    free(contenido);

    return resultado;
}

void procesar_eliminar_proceso(int *fd_conexion)
{
    int size_buffer = -1;
    uint32_t PID = -1;
    int verificador = 1;
    bool borrado;
    uint32_t tamanio_proceso;
    // int tamanio_particion;

    recv(*fd_conexion, &size_buffer, sizeof(int), MSG_WAITALL);
    recv(*fd_conexion, &PID, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &tamanio_proceso, sizeof(uint32_t), MSG_WAITALL);

    tcb_memoria *tcb_con_valores = malloc(sizeof(tcb_memoria));
    tcb_con_valores->PID = PID;

    bool coincide_pid_en_particiones(particion_memoria * particion)
    {
        // supongo que estaba ocupada la particion, por eso no chequeo su estado de ocupado
        return particion->PID == tcb_con_valores->PID;
    }

    particion_memoria *particion_ocupada = list_find(particiones, coincide_pid_en_particiones);
    particion_ocupada->ocupado = false;
    // tamanio_particion = particion_ocupada->tamanio;

    if (strcmp(SCHEME, "DINAMICAS") == 0)
    {
        // log_error(LOGGER, "Entre al dinamicas");
        compactar_particiones_libres();
    }

    while (verificador)
    {
        bool coincide_pid(tcb_memoria * tcb)
        {
            return tcb->PID == tcb_con_valores->PID;
        }
        tcb_memoria *tcb_existente = list_find(lista_tcbs, coincide_pid);

        if (tcb_existente != NULL)
        {
            borrado = list_remove_element(lista_tcbs, tcb_existente);
            free(tcb_existente);
        }
        else
        {
            verificador = 0;
        }
    }
    log_info(LOGGER, "## Proceso Destruido - PID: <%d> - Tamaño: <%d>", PID, tamanio_proceso);
    free(tcb_con_valores);
    for (int i = 0; i < list_size(particiones); i++)
    {

        particion_memoria *particion = list_get(particiones, i);
        log_debug(LOGGER, "\nparticion [%d], ocupado: %d, base: %d, tamanio: %d\n", i, particion->ocupado, particion->base, particion->tamanio);
    }
}

void compactar_particiones_libres()
{
    // log_error(LOGGER, "Entre a compactar");

    particion_memoria *particion1;
    particion_memoria *particion2;
    int i = 0;
    for (i; i < list_size(particiones) - 1; i++)
    {
        particion1 = list_get(particiones, i);
        particion2 = list_get(particiones, i + 1);
        if (!particion1->ocupado && !particion2->ocupado)
        {
            // log_error(LOGGER, "La particion %d y la siguiente no esta ocupada", i);

            particion1->tamanio = particion1->tamanio + particion2->tamanio;
            // log_error(LOGGER, "La particion ahora tiene un tamaño de: %d", particion1->tamanio);
            particion1->limite = particion2->limite;
            // log_error(LOGGER, "El nuevo limite de la particion es: %d", particion1->limite);
            list_remove_element(particiones, particion2);
            free(particion2);
            i = 0;
        }
    }
}

void procesar_leer_memoria(int *fd_conexion)
{
    int size_buffer = -1;
    uint32_t PID = -1;
    uint32_t TID = -1;
    uint32_t direccion;
    void *bytes_leidos = malloc(4);

    recv(*fd_conexion, &size_buffer, sizeof(int), MSG_WAITALL);
    recv(*fd_conexion, &PID, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &TID, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &direccion, sizeof(uint32_t), MSG_WAITALL);

    log_info(LOGGER, "## Lectura - (PID:TID) - (<%d>:<%d>) - Dir. Física: %d - Tamaño: 4", PID, TID, direccion);

    memcpy(bytes_leidos, MEMORIA + direccion, 4);

    // log_info(LOGGER, "Lo que se envia a memoria es: %s", bytes_leidos);
    send(*fd_conexion, bytes_leidos, 4, 0);

    free(bytes_leidos);
}

int procesar_escribir_memoria(int *fd_conexion)
{
    int size_buffer = -1;
    uint32_t PID = -1;
    uint32_t TID = -1;
    uint32_t direccion;
    void *bytes_recibidos = malloc(4);

    recv(*fd_conexion, &size_buffer, sizeof(int), MSG_WAITALL);
    recv(*fd_conexion, &PID, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &TID, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, &direccion, sizeof(uint32_t), MSG_WAITALL);
    recv(*fd_conexion, bytes_recibidos, 4, MSG_WAITALL);

    log_info(LOGGER, "## Escritura - (PID:TID) - (<%d>:<%d>) - Dir. Física: %d - Tamaño: 4", PID, TID, direccion);

    memcpy(MEMORIA + direccion, bytes_recibidos, 4);

    // mem_hexdump(MEMORIA, 256);

    free(bytes_recibidos);

    return 1;
}

void procesar_solicitar_base(int *fd_conexion)
{
    int size_buffer = -1;
    uint32_t PID = -1;

    recv(*fd_conexion, &size_buffer, sizeof(int), MSG_WAITALL);
    recv(*fd_conexion, &PID, sizeof(uint32_t), MSG_WAITALL);

    tcb_memoria *tcb_con_valores = malloc(sizeof(tcb_memoria));
    tcb_con_valores->PID = PID;

    bool coincide_pid_en_particiones(particion_memoria * particion)
    {
        // supongo que estaba ocupada la particion, por eso no chequeo su estado de ocupado
        return particion->PID == tcb_con_valores->PID;
    }

    particion_memoria *particion_ocupada = list_find(particiones, coincide_pid_en_particiones);

    t_paquete *paquete = crear_paquete_con_codigo(ENVIO_PAQUETE);
    agregar_a_paquete(paquete->buffer, &(particion_ocupada->base), sizeof(int));
    agregar_a_paquete(paquete->buffer, &(particion_ocupada->tamanio), sizeof(int));

    enviar_paquete(paquete, *fd_conexion);

    eliminar_paquete(paquete);

    free(tcb_con_valores);
}

void inicializar_cliente_memoria(info_conexion *info)
{

    int err;
    size_t bytes;
    int32_t codop = 1;
    int32_t handshake = 1;
    int32_t result;
    struct addrinfo hints, *server_info;
    // int* resultado = malloc(sizeof(int));

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    err = getaddrinfo(info->ip, info->puerto, &hints, &server_info);

    if (err != 0)
    {
        log_error(info->logger, "No se pudo hacer el getaddrinfo");
    }

    int fd_conexion = socket(server_info->ai_family,
                             server_info->ai_socktype,
                             server_info->ai_protocol);

    // resultado = fd_conexion;

    if (fd_conexion == -1)
    {
        log_error(info->logger, "Error al hacer el socket");
        return -1;
    }

    err = connect(fd_conexion, server_info->ai_addr, server_info->ai_addrlen);

    if (err != 0)
    {
        log_error(info->logger, "Error al hacer el connect");
        return -1;
    }
    else
    {
        log_debug(info->logger, "Se logró hacer la conexion como cliente");
    }

    void *buffer = malloc(sizeof(int32_t) * 2);
    memcpy(buffer, &codop, sizeof(uint32_t));
    memcpy(buffer + sizeof(uint32_t), &handshake, sizeof(uint32_t));
    bytes = send(fd_conexion, buffer, sizeof(int32_t) * 2, 0);
    // bytes = send(fd_conexion, &handshake, sizeof(int32_t), 0);
    bytes = recv(fd_conexion, &result, sizeof(int32_t), MSG_WAITALL);

    if (result == 1)
    {
        log_debug(info->logger, "El servidor aceptó la conexión");
        // t_paquete *paquete_prueba = crear_paquete();
        int pid = 3;
        int cod_op = 0;
        char *prueba = "hi";
        void *stream = malloc(8);
        memcpy(stream, &cod_op, sizeof(int));
        memcpy(stream + 4, &pid, sizeof(int));
        send(fd_conexion, stream, 8, 0);
        /*         log_info(LOGGER, "El size del paquete es: %d", paquete_prueba->buffer->size);

                agregar_a_buffer(paquete_prueba->buffer, &pid, sizeof(int));
                log_info(LOGGER, "El size del paquete es: %d", paquete_prueba->buffer->size);

                enviar_paquete(paquete_prueba, fd_conexion); */
    }
    else
    {
        log_debug(info->logger, "lo recibido fue: %d", result);
        log_error(info->logger, "No se pudo realizar el handshake");
        return -1;
    }

    freeaddrinfo(server_info);
    close(fd_conexion);
}

bool base_menor(particion_memoria *a, particion_memoria *b)
{
    return a->base < b->base;
}

particion_memoria *asignar_memoria(t_list *particiones, int tamanio_proceso, uint32_t PID)
{

    /*     for (int i = 0; i < list_size(particiones); i++)
        {

            particion_memoria *particion = list_get(particiones, i);
            log_debug(LOGGER, "\nparticion [%d], ocupado: %d, base: %d, tamanio: %d\n", i, particion->ocupado, particion->base, particion->tamanio);
        } */
    particion_memoria *resultado = algoritmo_busqueda(tamanio_proceso, particiones);

    if (resultado == NULL)
    {
        return resultado;
    }
    else
    {
        if (strcmp(SCHEME, "FIJAS") == 0)
        {

            resultado->ocupado = true;
            resultado->PID = PID;
        }
        else if (strcmp(SCHEME, "DINAMICAS") == 0)
        {
            int tamanio_aux = resultado->tamanio - tamanio_proceso;
            resultado->PID = PID;
            resultado->tamanio = tamanio_proceso;
            resultado->limite = resultado->base + resultado->tamanio - 1;
            resultado->ocupado = true;
            particion_memoria *nueva_particion = malloc(sizeof(particion_memoria));
            /*             if(tamanio_proceso == 0){
                            nueva_particion->base = resultado->base + 1;
                            nueva_particion->tamanio = tamanio_aux - 1;
                            nueva_particion->ocupado = false;
                            list_add_sorted(particiones, nueva_particion, base_menor);
                        }else{} */
            nueva_particion->base = resultado->base + resultado->tamanio;
            nueva_particion->tamanio = tamanio_aux;
            nueva_particion->ocupado = false;
            list_add_sorted(particiones, nueva_particion, base_menor);
        }

        /*char *prueba = "ocupado";
        memcpy(MEMORIA + resultado->base, prueba, strlen(prueba)); */

        return resultado;
    }
}

void leer_instrucciones(FILE *archivo, t_list *lista)
{
    char *linea = NULL;
    size_t len = 0;
    size_t read = 0;

    /*     char linea[50];
        while (fgets(linea, sizeof(linea), archivo) != NULL)
        {
            linea[strcspn(linea, "\n")] = '\0';
            char *linea_copia = strdup(linea);
            if (linea_copia == NULL)
            {
                perror("Error al duplicar la linea");
                exit(EXIT_FAILURE);
            }
            list_add(lista, linea_copia);
        }
        fclose(archivo);
        free(linea); */
    while ((read = getline(&linea, &len, archivo)) != -1)
    {
        if (linea[read - 1] == '\n')
        {
            linea[read - 1] = '\0';
        }
        list_add(lista, strdup(linea));
    }
    free(linea);
    fclose(archivo);
}

void imprimir_instrucciones(t_list *lista, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        char *instruccion = (char *)list_get(lista, i); // lista_get obtiene el elemento en la posición i
        printf("Instrucción %zu: %s\n", i + 1, instruccion);
    }
}