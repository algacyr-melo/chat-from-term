#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS 1024

typedef struct s_client {
  int id;
  int fd;
} t_client;

t_client clients[MAX_CLIENTS];
int next_id = 0;

void broadcast(const char *msg, int exclude_fd) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].fd != -1 && clients[i].fd != exclude_fd) {
      send(clients[i].fd, msg, strlen(msg), 0);
    }
  }
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // Create a TCP/Ipv4 socket
  int server = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
                             .sin_port = htons(atoi(argv[1]))};

  // Bind & Listen
  bind(server, (struct sockaddr *)&addr, sizeof(addr));
  listen(server, SOMAXCONN);

  // Initialize clients array
  for (int i = 0; i < MAX_CLIENTS; i++) {
    clients[i].fd = -1;
  }

  fd_set read_fds;
  int max_fd = server;

  printf("Server running on port %s\n", argv[1]);

  while (1) {
    FD_ZERO(&read_fds);
    FD_SET(server, &read_fds);

    // Add active clients to the read_fds
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clients[i].fd != -1) {
        FD_SET(clients[i].fd, &read_fds);
        if (clients[i].fd > max_fd) {
          max_fd = clients[i].fd;
        }
      }
    }

    if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
      perror("select");
      exit(1);
    }

    // Handle new connection
    if (FD_ISSET(server, &read_fds)) {
      int client_fd = accept(server, NULL, NULL);
      printf("New client connected: %d\n", client_fd);

      for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == -1) {
          clients[i].fd = client_fd;
          clients[i].id = next_id++;

          // Broadcast join message
          char msg[50];
          sprintf(msg, "server: client %d joined\n", clients[i].id);
          broadcast(msg, client_fd);
          break;
        }
      }
    }

    // Handle client data
    for (int i = 0; i < MAX_CLIENTS; i++) {
      int fd = clients[i].fd;
      if (fd != -1 && FD_ISSET(fd, &read_fds)) {
        char buf[1024];
        int n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) {
          // Broadcast leave message
          char msg[50];
          sprintf(msg, "server: client %d left\n", clients[i].id);
          broadcast(msg, fd);

          close(fd);
          clients[i].fd = -1;
        } else {
          send(fd, buf, n, 0); // Echo
        }
      }
    }
  }
  close(server);
  return 0;
}
