#include <utils/utils.h>
#include <filesystem.h>
#include <sys/stat.h>  // Para mkdir
#include <sys/types.h> // Para mode_t
#include <errno.h>     // Para verificar errores

int main(int argc, char *argv[])
{

    pthread_t hilo_servidor_fileSystem;

    LOGGER = log_create(LOGGER_PATH, MODULE_NAME, true, LOG_LEVEL_DEBUG);
    // LOGGER = initialize_logger(LOGGER_PATH, MODULE_NAME);
    inicializar_config(MODULE_CONFIG_PATH);

    // Crear el directorio mount_dir si no existe
    if (crear_directorio(MOUNT_DIR) != 0)
    {
        // Manejar el error si no se pudo crear el directorio
        return EXIT_FAILURE;
    }

    crear_blocks_path();
    crear_bitmap_path();

    crear_archivo_de_bloques();
    crear_bitmap();

    info_conexion_servidor *info_servidor_fileSystem = crear_conexion_servidor(LISTENING_PORT, LOGGER);
    pthread_create(&hilo_servidor_fileSystem, NULL, (void *)inicializar_server_filesystem, info_servidor_fileSystem);

    pthread_join(hilo_servidor_fileSystem, NULL);

    return 0;
}

// la funcion read_config se ejecuta cuando se ejecuta initialize_config. La funcion initialize_config
// esta en el utils.c
void read_config(t_config *MODULE_CONFIG)
{
    LISTENING_PORT = config_get_string_value(MODULE_CONFIG, "PUERTO_ESCUCHA");
    MOUNT_DIR = config_get_string_value(MODULE_CONFIG, "MOUNT_DIR");
    BLOCK_SIZE = config_get_int_value(MODULE_CONFIG, "BLOCK_SIZE");
    BLOCK_COUNT = config_get_int_value(MODULE_CONFIG, "BLOCK_COUNT");
    BLOCK_ACCESS_DELAY = config_get_int_value(MODULE_CONFIG, "RETARDO_ACCESO_BLOQUE");
    LOG_LEVEL = config_get_string_value(MODULE_CONFIG, "LOG_LEVEL");
}

void inicializar_server_filesystem(info_conexion_servidor *info)
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
                       (void *)atender_cliente_filesystem,
                       fd_conexion_ptr);
        pthread_detach(thread);
    }
    freeaddrinfo(server_info);

    // log_info(logger, "file descriptor socket listener: %d", fd_listen);
    // return fd_listen;
}

void atender_cliente_filesystem(int *fd_conexion)
{
    int32_t handshake_cod = -1;
    int bytes = recv(*fd_conexion, &handshake_cod, sizeof(int32_t), MSG_WAITALL);

    if (handshake_cod == -1)
    {
        log_error(LOGGER, "No se envio el handshake");
    }
    else
    {
        uint32_t handshake = 0;
        uint32_t result_ok = 1;
        uint32_t result_err = -1;
        bytes = recv(*fd_conexion, &handshake, sizeof(uint32_t), MSG_WAITALL);
        switch (handshake)
        {
        case CLI_MEMORIA:
            bytes = send(*fd_conexion, &result_ok, sizeof(uint32_t), 0);

            log_info(LOGGER, "Conexion a filesystem recibida");

            atender_peticion(fd_conexion);

            break;

        default:
            bytes = send(*fd_conexion, &result_err, sizeof(uint32_t), 0);
            break;
        }
    }
}
// log_info(LOGGER, "codop: %d", codop);

void atender_peticion(int *fd_conexion)
{
    int cod_op;
    int resultado;

    recv(*fd_conexion, &cod_op, sizeof(int), MSG_WAITALL);
    // log_info(LOGGER, "Se recibio el paqueton");
    switch (cod_op)
    {
    case DUMP_MEMORY:

        log_debug(LOGGER, "ENTRO A DUMP");
        uint32_t pid, tid;
        uint32_t tamanio_archivo;
        int size_buffer = -1;

        recv(*fd_conexion, &size_buffer, sizeof(int), MSG_WAITALL);

        // Recibo datos memoria
        if (recv(*fd_conexion, &pid, sizeof(int), MSG_WAITALL) <= 0)
        {
            log_error(LOGGER, "Error al recibir PID");
            break;
        }
        // log_info(LOGGER, "El pid es: %d", pid);

        if (recv(*fd_conexion, &tid, sizeof(int), MSG_WAITALL) <= 0)
        {
            log_error(LOGGER, "Error al recibir TID");
            break;
        }
        // log_info(LOGGER, "El tid es: %d", tid);

        if (recv(*fd_conexion, &tamanio_archivo, sizeof(int), MSG_WAITALL) <= 0)
        {
            log_error(LOGGER, "Error al recibir tamaño de archivo");
            break;
        }
        // log_info(LOGGER, "El tamaño es: %d", tamanio_archivo);
        char *contenido = malloc(tamanio_archivo);

        recv(*fd_conexion, contenido, tamanio_archivo, MSG_WAITALL);

        int resultado_dump = dump_memory(pid, tid, tamanio_archivo, contenido);
        send(*fd_conexion, &resultado_dump, sizeof(int), 0);
        free(contenido);
        break;

    default:
        log_info(LOGGER, "No se reconoce el codigo de operacion");
    }
}

// inicializo estructuras

void crear_blocks_path()
{
    block_path = string_new();
    string_append(&block_path, MOUNT_DIR);
    string_append(&block_path, "/bloques.dat");
}

void crear_bitmap_path()
{
    bitmap_path = string_new();
    string_append(&bitmap_path, MOUNT_DIR);
    string_append(&bitmap_path, "/bitmap.dat");
}

char *crear_metadata_path(char *nombre_archivo)
{
    char *files_dir = string_new();
    string_append(&files_dir, MOUNT_DIR);
    string_append(&files_dir, "/files");

    struct stat st = {0};
    if (stat(files_dir, &st) == -1)
    {
        if (mkdir(files_dir, 0700) != 0)
        {
            perror("Error al crear la carpeta 'files'");
            return NULL;
        }
    }

    metadata_path = string_new();
    string_append(&metadata_path, files_dir);
    string_append(&metadata_path, "/");
    string_append(&metadata_path, nombre_archivo);

    free(files_dir);

    return metadata_path;
}

void crear_archivo_de_bloques()
{
    printf("Creando Archivo de Bloques\n");

    int chequeo = access(block_path, F_OK);
    int fd_bloque_de_datos = open(block_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd_bloque_de_datos == -1)
    {
        perror("Error al abrir bloques.dat");
        exit(EXIT_FAILURE);
    }

    if (chequeo == -1)
    {
        log_info(LOGGER, "Entre a truncar");
        if (ftruncate(fd_bloque_de_datos, BLOCK_COUNT * BLOCK_SIZE) == -1)
        {
            perror("Error al truncar bloques.dat");
            close(fd_bloque_de_datos);
            exit(EXIT_FAILURE);
        }
    }

    bloques_datos_addr = malloc(BLOCK_COUNT * BLOCK_SIZE);
    puntero_auxiliar_mmap = mmap(NULL, BLOCK_COUNT * BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bloque_de_datos, 0);

    if (puntero_auxiliar_mmap == MAP_FAILED)
    {
        perror("Error al mapear bloques.dat");
        close(fd_bloque_de_datos);
        exit(EXIT_FAILURE);
    }
}

void crear_bitmap()
{
    bitmap = malloc(sizeof(t_bitmap));
    printf("Creando el Bitmap\n");

    int fd_bitarray = open(bitmap_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd_bitarray == -1)
    {
        printf("No se pudo abrir/crear el archivo bitmap.dat\n");
        return;
    }

    bitmap->tamanio = (uint32_t)ceil((double)BLOCK_COUNT / 8);

    log_trace(LOGGER, "BITMAP TAMANIO: %d", bitmap->tamanio);

    if (ftruncate(fd_bitarray, bitmap->tamanio) == -1)
    {
        printf("Error al establecer el tamaño del archivo bitmap.dat\n");
        close(fd_bitarray);
        return;
    }

    bitmap->direccion = mmap(NULL, bitmap->tamanio, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bitarray, 0);
    if (bitmap->direccion == MAP_FAILED)
    {
        printf("Error al mapear el archivo bitmap.dat en memoria\n");
        close(fd_bitarray);
        return;
    }

    // memset(bitmap->direccion, 0, bitmap->tamanio);

    bitmap->bitarray = bitarray_create_with_mode(bitmap->direccion, bitmap->tamanio, LSB_FIRST);
    if (!bitmap->bitarray)
    {
        printf("Error al asignar memoria para el objeto bitarray\n");
        munmap(bitmap->direccion, bitmap->tamanio);
        close(fd_bitarray);
        return;
    }
    if (msync(bitmap->direccion, bitmap->tamanio, MS_SYNC) == -1)
    {
        printf("Error en la sincronización con msync()\n");
    }

    close(fd_bitarray);
}

int crear_directorio(const char *path)
{
    struct stat st = {0};

    if (stat(path, &st) == -1)
    {
        if (mkdir(path, 0700) != 0)
        {
            perror("Error al crear el directorio");
            return -1;
        }
    }
    return 0;
}

char *generar_nombre_archivo(uint32_t pid, uint32_t tid)
{
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);

    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

    char *nombre_archivo = malloc(50 * sizeof(char));
    if (nombre_archivo == NULL)
    {
        perror("Error al asignar memoria");
        exit(EXIT_FAILURE);
    }

    snprintf(nombre_archivo, 50, "%d-%d-%s.dmp", pid, tid, timestamp);
    return nombre_archivo;
}

int dump_memory(uint32_t pid, uint32_t tid, int tamanio_archivo, char *contenido)
{

    // log_info(LOGGER, "contenido es: %s", contenido);
    //  Verifico si hay suficiente espacio disponible
    int bloques_necesarios = (tamanio_archivo + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int bloques_disponibles = contar_bloques_libres();
    if (bloques_necesarios + 1 > bloques_disponibles)
    {
        log_error(LOGGER, "Error: No hay suficientes bloques disponibles.");
        return 0;
    }
    char *nombre_archivo = generar_nombre_archivo(pid, tid);
    // Reservo los bloques en el bitmap
    int bloque_indice = reservar_bloque();
    if (bloque_indice == -1)
    {
        log_error(LOGGER, "Error al reservar bloque de índice.");
        return 0;
    }

    log_info(LOGGER, "## Bloque asignado: %d - Archivo: %s - Bloques Libres: %d",
             bloque_indice, nombre_archivo, contar_bloques_libres());

    int bloques[bloques_necesarios];
    for (int i = 0; i < bloques_necesarios; i++)
    {
        bloques[i] = reservar_bloque();
        if (bloques[i] == -1)
        {
            log_error(LOGGER, "Error al reservar bloques de datos.");
            return 0;
        }
        log_info(LOGGER, "## Bloque asignado: %d - Archivo: %s - Bloques Libres: %d",
                 bloques[i], nombre_archivo, contar_bloques_libres());
    }

    // Creo archivo de metadata
    char *path_metadata = crear_metadata_path(nombre_archivo);
    FILE *archivo_metadata = fopen(path_metadata, "w");
    if (!archivo_metadata)
    {
        log_error(LOGGER, "Error al crear archivo de metadata.");
        return 0;
    }
    fprintf(archivo_metadata, "SIZE = %d INDEX_BLOCK = %d", tamanio_archivo, bloque_indice);
    fclose(archivo_metadata);
    log_info(LOGGER, "## Archivo Creado: %s - Tamaño: %d", nombre_archivo, tamanio_archivo);

    // Escribo en el bloque índice
    int *punteros_bloques = (int *)(puntero_auxiliar_mmap + bloque_indice * BLOCK_SIZE);
    for (int i = 0; i < bloques_necesarios; i++)
    {
        punteros_bloques[i] = bloques[i];
    }
    msync(puntero_auxiliar_mmap + bloque_indice * BLOCK_SIZE, BLOCK_SIZE, MS_SYNC);
    log_info(LOGGER, "## Acceso Bloque - Archivo: %s - Tipo Bloque: ÍNDICE - Bloque File System %d",
             nombre_archivo, bloque_indice);

    // Escribo el contenido en los bloques de datos
    int bytes_escritos = 0;
    for (int i = 0; i < bloques_necesarios; i++)
    {
        int bytes_a_escribir = (tamanio_archivo - bytes_escritos > BLOCK_SIZE) ? BLOCK_SIZE : (tamanio_archivo - bytes_escritos);
        memset(puntero_auxiliar_mmap + bloques[i] * BLOCK_SIZE, 0, BLOCK_SIZE);
        // log_info(LOGGER, "Bytes escritos: %d y tamaño contenido: %d", bytes_escritos, strlen(contenido));
        if (bytes_escritos < tamanio_archivo)
        {
            int bytes_validos = (tamanio_archivo - bytes_escritos > bytes_a_escribir) ? bytes_a_escribir : (tamanio_archivo - bytes_escritos);
            memcpy(puntero_auxiliar_mmap + bloques[i] * BLOCK_SIZE, contenido + bytes_escritos, bytes_validos);
        }
        // memcpy(puntero_auxiliar_mmap + bloques[i] * BLOCK_SIZE, contenido + bytes_escritos, bytes_a_escribir);
        msync(puntero_auxiliar_mmap + bloques[i] * BLOCK_SIZE, BLOCK_SIZE, MS_SYNC);

        // Logueo acceso a bloque de datos
        log_info(LOGGER, "## Acceso Bloque - Archivo: %s - Tipo Bloque: DATOS - Bloque File System %d",
                 nombre_archivo, bloques[i]);

        bytes_escritos += bytes_a_escribir;

        // Aplico el retardo después de cada acceso a bloque
        usleep(BLOCK_ACCESS_DELAY * 1000);
    }

    // Log de fin de solicitud
    log_info(LOGGER, "## Fin de solicitud - Archivo: %s", nombre_archivo);
    return 1;
}

void escribir_en_bloques(const char *contenido, int tamanio_contenido, int bloque_inicial)
{
    FILE *archivo = fopen(block_path, "r+b"); // Abrir en modo lectura/escritura binaria
    if (archivo == NULL)
    {
        perror("Error al abrir el archivo");
        exit(EXIT_FAILURE);
    }

    int bytes_escritos = 0;
    int bloque_actual = bloque_inicial;

    while (bytes_escritos < tamanio_contenido)
    {
        // Calcular la cantidad de bytes a escribir en este bloque
        int bytes_restantes = tamanio_contenido - bytes_escritos;
        int bytes_a_escribir = (bytes_restantes > BLOCK_SIZE) ? BLOCK_SIZE : bytes_restantes;

        // Mover el puntero del archivo al inicio del bloque actual
        fseek(archivo, bloque_actual * BLOCK_SIZE, SEEK_SET);

        // Escribir en el bloque
        fwrite(contenido + bytes_escritos, 1, bytes_a_escribir, archivo);

        // Actualizar el contador de bytes escritos y el bloque actual
        bytes_escritos += bytes_a_escribir;
        bloque_actual++;
    }

    fclose(archivo); // Cerrar el archivo
    printf("Escritura completada: %zu bytes escritos desde el bloque %d.\n", bytes_escritos, bloque_inicial);
}

int contar_bloques_libres()
{
    int bloques_libres = 0;

    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        if (!bitarray_test_bit(bitmap->bitarray, i))
        {
            bloques_libres++;
        }
    }

    return bloques_libres;
}

int reservar_bloque()
{
    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        if (!bitarray_test_bit(bitmap->bitarray, i))
        {
            bitarray_set_bit(bitmap->bitarray, i);

            if (msync(bitmap->direccion, bitmap->tamanio, MS_SYNC) == -1)
            {
                perror("Error al sincronizar bitmap.dat con msync()");
            }

            return i;
        }
    }

    return -1;
}
