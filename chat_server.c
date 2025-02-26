#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 1024
#define BUFFER_SIZE 4096

typedef struct s_client {
  int fd;
  int id;
} t_client;

typedef struct {
  t_client clients[MAX_CLIENTS];
  int server_fd;
  int max_fd;
} server_state;

// Função 1: Configuração inicial do servidor
int setup_server(int port) {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_addr.s_addr = INADDR_ANY,
                             .sin_port = htons(port)};

  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 10) < 0) {
    perror("listen failed");
    exit(EXIT_FAILURE);
  }
  return server_fd;
}

// Função 2: Aceitar nova conexão
void accept_connection(server_state *state) {
  int client_fd = accept(state->server_fd, NULL, NULL);
  if (client_fd < 0)
    return;

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (state->clients[i].fd == 0) {
      state->clients[i].fd = client_fd;
      state->clients[i].id = i;
      dprintf(client_fd, "Welcome! Your ID: %d\n", i);
      printf("Client %d connected\n", i);
      return;
    }
  }
  close(client_fd);
  printf("Server full! Rejected connection\n");
}

// Função 3: Processar mensagem do cliente
void handle_client(server_state *state, int client_index) {
  char buffer[BUFFER_SIZE];
  t_client *client = &state->clients[client_index];

  int bytes = recv(client->fd, buffer, BUFFER_SIZE - 1, 0);
  if (bytes <= 0) {
    printf("Client %d disconnected\n", client->id);
    close(client->fd);
    client->fd = 0;
    return;
  }

  buffer[bytes] = '\0';
  char msg[BUFFER_SIZE + 20];
  snprintf(msg, sizeof(msg), "Client %d: %s", client->id, buffer);

  // Função 4: Broadcast
  for (int j = 0; j < MAX_CLIENTS; j++) {
    if (state->clients[j].fd > 0 && j != client_index) {
      send(state->clients[j].fd, msg, strlen(msg), 0);
    }
  }
}

// Função principal
int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  server_state state = {0};
  state.server_fd = setup_server(atoi(argv[1]));
  state.max_fd = state.server_fd;

  printf("Server running on port %s\n", argv[1]);

  fd_set read_fds;
  while (1) {
    FD_ZERO(&read_fds);
    FD_SET(state.server_fd, &read_fds);
    state.max_fd = state.server_fd;

    // Configurar file descriptors
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (state.clients[i].fd > 0) {
        FD_SET(state.clients[i].fd, &read_fds);
        if (state.clients[i].fd > state.max_fd)
          state.max_fd = state.clients[i].fd;
      }
    }

    // Esperar por atividade
    if (select(state.max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
      perror("select error");
      exit(EXIT_FAILURE);
    }

    // Nova conexão
    if (FD_ISSET(state.server_fd, &read_fds)) {
      accept_connection(&state);
    }

    // Verificar clientes
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (state.clients[i].fd > 0 && FD_ISSET(state.clients[i].fd, &read_fds)) {
        handle_client(&state, i);
      }
    }
  }

  close(state.server_fd);
  return 0;
}
