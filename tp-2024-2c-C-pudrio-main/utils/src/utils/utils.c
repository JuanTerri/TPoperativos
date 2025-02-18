#include <utils/utils.h>

void inicializar_server(info_conexion_servidor *info)
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
                   (void *)atender_cliente,
                   fd_conexion_ptr);
    pthread_detach(thread);
  }
  freeaddrinfo(server_info);

  // log_info(logger, "file descriptor socket listener: %d", fd_listen);
  // return fd_listen;
}

int inicializar_cliente(info_conexion *info, tipo_cliente cliente)
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
  /*else
  {
    log_info(info->logger, "Se logró hacer la conexion como cliente");
  }*/

  void *buffer = malloc(sizeof(int32_t) * 2);
  memcpy(buffer, &codop, sizeof(uint32_t));
  memcpy(buffer + sizeof(uint32_t), &cliente, sizeof(int));
  bytes = send(fd_conexion, buffer, sizeof(int32_t) + sizeof(int), 0);
  // bytes = send(fd_conexion, &handshake, sizeof(int32_t), 0);
  bytes = recv(fd_conexion, &result, sizeof(int32_t), MSG_WAITALL);

  if (result != 1)
  {
    log_error(info->logger, "No se pudo realizar el handshake");
    return -1;
  }
  /*else
  {
    log_info(info->logger, "El servidor aceptó la conexión");
  }*/
  freeaddrinfo(server_info);

  return fd_conexion;
  // close(fd_conexion);
}

/* t_log* initialize_logger(char* logger_path, char* module_name){
    t_log* LOGGER = log_create(logger_path, module_name, true, LOG_LEVEL_DEBUG);
    return LOGGER;
} */

void inicializar_config(char *path)
{
  MODULE_CONFIG = config_create(path);
  if (MODULE_CONFIG == NULL)
  {
    log_error(LOGGER, "No se pudo abrir el config %s", path);
  }
  read_config(MODULE_CONFIG);
}

void atender_cliente(int *fd_conexion)
{
  int32_t codop = 0;
  int bytes = recv(*fd_conexion, &codop, sizeof(int32_t), MSG_WAITALL);

  // log_info(LOGGER, "codop: %d", codop);
  switch (codop)
  {
  case 1: // caso handshake
    int32_t handshake = 0;
    int32_t result_ok = 1;
    int32_t result_err = -1;

    bytes = recv(*fd_conexion, &handshake, sizeof(int32_t), MSG_WAITALL);
    if (handshake == 1)
    {
      bytes = send(*fd_conexion, &result_ok, sizeof(int32_t), 0);
      log_info(LOGGER, "Conexion a server recibida");
    }
    else
    {
      bytes = send(*fd_conexion, &result_err, sizeof(int32_t), 0);
    }
    close(*fd_conexion);
    free(fd_conexion);
    break;

  default:
    log_error(LOGGER, "No se conoce el codigo de operacion recibido");
    break;
  }
}

info_conexion *crear_conexion_cliente(char *ip, char *puerto, t_log *logger)
{

  info_conexion *conexion = malloc(sizeof(info_conexion));

  conexion->ip = ip;
  conexion->puerto = puerto;
  conexion->logger = logger;

  return conexion;
}

info_conexion_servidor *crear_conexion_servidor(char *puerto, t_log *logger)
{

  info_conexion_servidor *conexion = malloc(sizeof(info_conexion_servidor));

  conexion->puerto = puerto;
  conexion->logger = logger;

  return conexion;
}

void agregar_a_paquete(t_buffer *buffer, void *fuente, int tamanio_fuente)
{
  if (buffer == NULL || fuente == NULL || tamanio_fuente == 0)
    return;

  void *nuevo_stream = realloc(buffer->stream, buffer->size + tamanio_fuente);
  if (nuevo_stream == NULL)
  {
    log_error(LOGGER, "realloc: No se pudo agregar el stream al buffer");
    ;
  }

  buffer->stream = nuevo_stream;

  memcpy((void *)(((uint8_t *)buffer->stream) + buffer->size), fuente, tamanio_fuente);
  buffer->size += tamanio_fuente;
}

void serializar_tamanio(t_buffer *buffer, uint32_t source)
{
  if (buffer == NULL)
    return;

  agregar_a_paquete(buffer, &source, sizeof(uint32_t));
}

void enviar_paquete(t_paquete *paquete, int socket_cliente)
{
  int bytes = paquete->buffer->size + 2 * sizeof(int);
  void *a_enviar = serializar_paquete(paquete, bytes);

  send(socket_cliente, a_enviar, bytes, 0);

  free(a_enviar);
}
void eliminar_paquete(t_paquete *paquete)
{
  free(paquete->buffer->stream);
  free(paquete->buffer);
  free(paquete);
}
void liberar_conexion(int socket_cliente)
{
  close(socket_cliente);
}
t_paquete *crear_paquete_con_codigo(op_code codigo_operacion)
{
  t_paquete *paquete = malloc(sizeof(t_paquete));
  paquete->codigo_operacion = codigo_operacion;
  crear_buffer(paquete);
  return paquete;
}
void crear_buffer(t_paquete *paquete)
{
  paquete->buffer = malloc(sizeof(t_buffer));
  paquete->buffer->size = 0;
  paquete->buffer->stream = NULL;
}
void *serializar_paquete(t_paquete *paquete, int bytes)
{
  void *magic = malloc(bytes);
  int desplazamiento = 0;

  memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
  desplazamiento += sizeof(int);
  memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
  desplazamiento += sizeof(int);
  memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
  desplazamiento += paquete->buffer->size;

  return magic;
}

/* t_paquete *crear_paquete()
{
  t_paquete *paquete = malloc(sizeof(t_paquete));
  crear_buffer(paquete);
  return paquete;
} */

char *concatenar_strings(char *a, char *b)
{
  size_t tamanio_total = strlen(a) + strlen(b) + 2;
  char *ruta_instrucciones = malloc(tamanio_total);
  strcpy(ruta_instrucciones, a);
  strcat(ruta_instrucciones, "/");
  strcat(ruta_instrucciones, b);
  return ruta_instrucciones;
}

int recibir_operacion(int socket_cliente) // DEVUELVE SOLAMENTE EL CODIGO DE OPERACION
{
  int cod_op;

  if (recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
    return cod_op;
  else
  {
    close(socket_cliente);
    return -1;
  }
}