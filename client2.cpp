#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXDATASIZE 100

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int setnonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* The server sent something: read everything available (needed with EPOLLET) */
void do_use_fd(int fd) {
  char buf[512];
  while (1) {
    ssize_t count = read(fd, buf, sizeof(buf));
    if (count == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break; /* read everything for now */
      perror("read");
      close(fd);
      break;
    } else if (count == 0) { /* client closed the connection */
      printf("Client on fd %d disconnected\n", fd);
      close(fd); /* also removes it from epoll */
      break;
    }
    printf("Client %d: %.*s", fd, (int)count, buf);
    fflush(stdout);
  }
}

/* The client operator typed a line: send it to the server */
void do_use_stdin(int epll_fd,int s) {
  char line[512];
  ssize_t count = read(STDIN_FILENO, line, sizeof(line));
  if (count <= 0) { /* Ctrl+D: stop watching the keyboard */
    epoll_ctl(epll_fd, EPOLL_CTL_DEL, STDIN_FILENO, NULL);
    printf("(keyboard input closed, still serving clients)\n");
    return;
  }
    if (write(s, line, count) == -1)
      perror("write");
  printf("(sent to server\n" );
}


int main() {
  int s, numbytes;
  char buf[MAXDATASIZE];
  struct addrinfo hints, *res, *rp;
  char sendLine[150];
  char addr_s[INET6_ADDRSTRLEN];
  struct epoll_event ev, events[100];
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(NULL, "3490", &hints, &res) != 0) {
    fprintf(stderr, "Can t get adress");
    exit(EXIT_FAILURE);
  }

  for (rp = res; rp != NULL; rp = rp->ai_next) {
    s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (s == -1) {
      fprintf(stderr, "Can t make socket");
      continue;
    }
    inet_ntop(rp->ai_family, get_in_addr(rp->ai_addr), addr_s, sizeof(addr_s));
    printf("Attempting connection to %s\n", addr_s);
    if (connect(s, rp->ai_addr, rp->ai_addrlen) == -1) {
      perror("client: connect");
      close(s);
      continue;
    }
    break;
  }
  if (rp == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }

  int epll_fd = epoll_create1(0);
  if (epll_fd == -1) {
    perror("epoll_create1");
    exit(EXIT_FAILURE);
  }

  ev.events = EPOLLIN;
  ev.data.fd = STDIN_FILENO;
  if (epoll_ctl(epll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1) {
    perror("epoll_ctl: stdin");
    exit(EXIT_FAILURE);
  }

  ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = s;
  if (epoll_ctl(epll_fd, EPOLL_CTL_ADD, s, &ev) == -1) {
    perror("epoll_ctl: conn_sock");
    close(s);
  }

  inet_ntop(rp->ai_family, get_in_addr(rp->ai_addr), addr_s, sizeof(addr_s));
  printf("client: connected to %s\n", addr_s);
  freeaddrinfo(res);

  while (1) {
    int nfds = epoll_wait(epll_fd, events, 100, -1);
    if (nfds == -1) {
      if (errno == EINTR)
        continue;
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }

    for (int i = 0; i < nfds; i++) {
      int fd = events[i].data.fd;
      if (setnonblocking(s) == -1) {
        perror("setnonblocking");
        close(s);
        continue;
      }
        if (fd == STDIN_FILENO) { /* operator typed something */
          do_use_stdin(epll_fd,s);

        } else { /* a client sent something */
          do_use_fd(fd);
        }
        // scanf("%s", sendLine);
        // int n = send(s, sendLine, sizeof sendLine, 0);
        // if (n == -1) {
        //   perror("send");
        // } else if (n == 0) {
        //   printf("server: client closed the connection\n");
        // } else {
        //   printf("Succesfully sent and received");
        // }
      }
    }


    printf("client: received '%s'\n", buf);

    close(s);

    return 0;
  }
