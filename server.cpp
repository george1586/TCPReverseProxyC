#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
  struct addrinfo hint, *res, *rp;
  struct sockaddr_storage their_addr;
  char addr_p[INET6_ADDRSTRLEN];
  socklen_t addr_size;
  int s, l, new_fd;
  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_UNSPEC; 
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags = AI_PASSIVE;
  if (getaddrinfo(NULL, "3490", &hint, &res) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    exit(EXIT_FAILURE);
  }
  for (rp = res; rp != NULL; rp = rp->ai_next) {
    s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (s == -1) {
      fprintf(stderr, "socket descriptor error");
      continue;
    }
    if (bind(s, rp->ai_addr, rp->ai_addrlen) == 0)
      break;
    close(s);
  }
  l = listen(s, 5);
  if (l == -1) {
    fprintf(stderr, "listen descriptor error");
    exit(EXIT_FAILURE);
  }
  addr_size = sizeof their_addr;

  while (1) {
    new_fd = accept(s, (struct sockaddr *)&their_addr, &addr_size);
    if (new_fd == -1) {
      fprintf(stderr, "Couldn t accept connection");
    }
    while (1) {
      char msgr[1025];
      ssize_t n = recv(new_fd, msgr, sizeof(msgr), 0);
      if (n == -1) {
        perror("recv");
      } else if (n == 0) {
        printf("server: client closed the connection\n");
      } else {
        msgr[n] = '\0';
        printf("server: got %zd bytes: %s\n", n, msgr);
        const char *msgs = "Hello spaffy \n";
        if (send(new_fd, msgs, strlen(msgs), 0) == -1) {
          perror("send");
        } else if (n == 0) {
          printf("server: client closed the connection\n");
        } else {
          printf("Succesfully sent and received");
        }
      }
    }
    close(new_fd);
  }
  close(s);
  return 0;
}