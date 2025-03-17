#include <stdio.h>
#include <stdlib.h>

#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <string.h>

#include <unistd.h>

int g_max_fd;
int g_id = 0;
char g_buf[400000];

fd_set g_read_set, g_write_set;

typedef struct s_client {
  int id;
  char *buf;
} t_client;

int extract_message(char **buf, char **msg) {
  char *newbuf;
  int i;

  *msg = 0;
  if (*buf == 0)
    return (0);
  i = 0;
  while ((*buf)[i]) {
    if ((*buf)[i] == '\n') {
      newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
      if (newbuf == 0)
        return (-1);
      strcpy(newbuf, *buf + i + 1);
      *msg = *buf;
      (*msg)[i + 1] = 0;
      *buf = newbuf;
      return (1);
    }
    i++;
  }
  return (0);
}

char *str_join(char *buf, char *add) {
  char *newbuf;
  int len;

  if (buf == 0)
    len = 0;
  else
    len = strlen(buf);
  newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
  if (newbuf == 0)
    return (0);
  newbuf[0] = 0;
  if (buf != 0)
    strcat(newbuf, buf);
  free(buf);
  strcat(newbuf, add);
  return (newbuf);
}

void err(const char *msg) {
  write(STDERR_FILENO, msg, strlen(msg));
  exit(EXIT_FAILURE);
}

void broadcast(t_client *clients, char *msg, int exclude_fd) {
  for (int fd = 0; fd < 1024; fd++) {
    if (clients[fd].id != -1 && fd != exclude_fd) {
      if (FD_ISSET(fd, &g_write_set)) {
        send(fd, msg, strlen(msg), MSG_NOSIGNAL);
      }
    }
  }
}

// Função principal
int main(int argc, char **argv) {
  if (argc != 2) {
    err("Wrong number of arguments\n");
  }

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    err("Fatal error\n");
  }

  struct sockaddr_in sockaddr;
  bzero(&sockaddr, sizeof(sockaddr));

  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sockaddr.sin_port = htons(atoi(argv[1]));

  if (bind(sockfd, (const struct sockaddr *)&sockaddr, sizeof(sockaddr)) != 0) {
    err("Fatal error\n");
  }
  if (listen(sockfd, 10) != 0) {
    err("Fatal error\n");
  }

  // Init clients array
  t_client clients[1024];
  for (int fd; fd < 1024; fd++) {
    clients[fd].id = -1;
    clients[fd].buf = NULL;
  }

  while (1) {
    FD_ZERO(&g_read_set);
    FD_ZERO(&g_write_set);

    FD_SET(sockfd, &g_read_set);
    g_max_fd = sockfd;
    for (int fd = 0; fd < 1024; fd++) {
      if (clients[fd].id != -1) {
        FD_SET(fd, &g_read_set);
        FD_SET(fd, &g_write_set);
        g_max_fd = fd;
      }
    }

    if (select(g_max_fd + 1, &g_read_set, &g_write_set, NULL, NULL) == -1) {
      err("Fatal error\n");
    }

    // Handle new connection
    if (FD_ISSET(sockfd, &g_read_set)) {
      int fd = accept(sockfd, NULL, NULL);
      if (fd == -1) {
        err("Fatal error\n");
      }
      clients[fd].id = g_id++;
      sprintf(g_buf, "server: client %d just arrived\n", clients[fd].id);
      broadcast(clients, g_buf, fd);
    }

    // Handle socket read and write events
    for (int fd = 0; fd < 1024; fd++) {

      if (clients[fd].id != -1 && FD_ISSET(fd, &g_read_set)) {
        int bytes = recv(fd, g_buf, sizeof(g_buf), 0);
        if (bytes <= 0) {
          sprintf(g_buf, "server: client %d just left\n", clients[fd].id);
          broadcast(clients, g_buf, fd);
          clients[fd].id = -1;
          close(fd);
        } else {
          g_buf[bytes] = '\0';
          clients[fd].buf = str_join(clients[fd].buf, g_buf);
          char *msg;
          while (extract_message(&clients[fd].buf, &msg) == 1) {
            sprintf(g_buf, "client %d: %s", clients[fd].id, msg);
            broadcast(clients, g_buf, fd);
            free(msg);
            if (clients[fd].buf[0] == '\0') {
              free(clients[fd].buf);
              clients[fd].buf = NULL;
            }
          }
        }
      }

    }
  }
}
