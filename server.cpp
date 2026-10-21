#include <arpa/inet.h>
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

#define PORT "3490"
#define MAX_EVENTS 10
#define MAX_CLIENTS 100

/* Our own list of connected clients (epoll keeps them too, but won't give us a
 * list) */
int clients[MAX_CLIENTS];
int nclients = 0;

void add_client(int fd) {
  if (nclients < MAX_CLIENTS)
    clients[nclients++] = fd;
}

void remove_client(int fd) {
  for (int i = 0; i < nclients; i++) {
    if (clients[i] == fd) {
      clients[i] = clients[--nclients]; /* move the last one into the gap */
      return;
    }
  }
}

int setnonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* A client sent something: read everything available (needed with EPOLLET) */
void do_use_fd(int fd) {
  char buf[512];
  while (1) {
    ssize_t count = read(fd, buf, sizeof(buf));
    if (count == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break; /* read everything for now */
      perror("read");
      remove_client(fd);
      close(fd);
      break;
    } else if (count == 0) { /* client closed the connection */
      printf("Client on fd %d disconnected\n", fd);
      remove_client(fd);
      close(fd); /* also removes it from epoll */
      break;
    }
    printf("Client %d: %.*s", fd, (int)count, buf);
    fflush(stdout);
  }
}

/* The server operator typed a line: send it to every connected client */
void do_use_stdin(int epollfd) {
  char line[512];
  ssize_t count = read(STDIN_FILENO, line, sizeof(line));
  if (count <= 0) { /* Ctrl+D: stop watching the keyboard */
    epoll_ctl(epollfd, EPOLL_CTL_DEL, STDIN_FILENO, NULL);
    printf("(keyboard input closed, still serving clients)\n");
    return;
  }
  for (int i = 0; i < nclients; i++) {
    if (write(clients[i], line, count) == -1)
      perror("write");
  }
  printf("(sent to %d client%s)\n", nclients, nclients == 1 ? "" : "s");
}

int main(void) {
  struct addrinfo hint, *res, *rp;
  struct sockaddr_storage their_addr;
  struct epoll_event ev, events[MAX_EVENTS];
  socklen_t addr_size;
  int s = -1, new_fd, epollfd, status, yes = 1;

  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(NULL, PORT, &hint, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    exit(EXIT_FAILURE);
  }
  for (rp = res; rp != NULL; rp = rp->ai_next) {
    s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (s == -1) {
      perror("socket");
      continue;
    }
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    if (bind(s, rp->ai_addr, rp->ai_addrlen) == 0)
      break;
    perror("bind");
    close(s);
  }
  if (rp == NULL) {
    fprintf(stderr, "failed to bind\n");
    exit(EXIT_FAILURE);
  }
  freeaddrinfo(res);

  if (listen(s, 5) == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  epollfd = epoll_create1(0);
  if (epollfd == -1) {
    perror("epoll_create1");
    exit(EXIT_FAILURE);
  }

  ev.events = EPOLLIN;
  ev.data.fd = s;
  
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, s, &ev) == -1) {
    perror("epoll_ctl: listen_sock");
    exit(EXIT_FAILURE);
  }

  ev.events = EPOLLIN;
  ev.data.fd = STDIN_FILENO;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) == -1) {
    perror("epoll_ctl: stdin");
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %s. Type a message to send it to all "
         "clients.\n",
         PORT);

  while (1) {
    int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
      if (errno == EINTR)
        continue;
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }

    for (int i = 0; i < nfds; i++) {
      int fd = events[i].data.fd;
      //printf("fd este %d, nfds este %d",fd,nfds);

      if (fd == s) {                   /* new client */
        addr_size = sizeof their_addr; /* reset before every accept */
        new_fd = accept(s, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1) {
          perror("accept");
          continue; /* don't kill the server */
        }
        if (setnonblocking(new_fd) == -1) {
          perror("setnonblocking");
          close(new_fd);
          continue;
        }
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = new_fd;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, new_fd, &ev) == -1) {
          perror("epoll_ctl: conn_sock");
          close(new_fd);
          continue;
        }
        add_client(new_fd);
        printf("New client on fd %d (%d connected)\n", new_fd, nclients);

      } else if (fd == STDIN_FILENO) { /* operator typed something */
        do_use_stdin(epollfd);

      } else { /* a client sent something */
        do_use_fd(fd);
      }
    }
  }
}