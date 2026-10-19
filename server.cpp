#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int setnonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void do_use_fd(int fd) {
  char buf[512];
  // Deoarece socketul este non-blocant (EPOLLET), trebuie să citim
  // într-o buclă până când primim EAGAIN, altfel riscăm să pierdem date.
  while (1) {
    ssize_t count = read(fd, buf, sizeof(buf));
    if (count == -1) {
      // Dacă errno este EAGAIN, înseamnă că am citit toate datele disponibile
      // momentan
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }
      perror("Eroare la citire");
      close(fd);
      break;
    } else if (count == 0) {
      // Clientul a închis conexiunea (EOF)
      printf("Clientul s-a deconectat pe fd: %d\n", fd);
      close(fd); // Închiderea fd-ului îl scoate automat și din epoll
      break;
    }
    // Procesăm datele primite (le afișăm în consolă în acest exemplu)
    printf("Date primite: %.*s", (int)count, buf);
    // Opțional: trimitem un răspuns înapoi
    write(fd, "Mesaj primit\n", 13);
  }
}

int main() {
  struct addrinfo hint, *res, *rp;
  struct sockaddr_storage their_addr;
  struct epoll_event ev, events[10];
  char addr_p[INET6_ADDRSTRLEN];
  socklen_t addr_size;
  int s, l, new_fd, epollfd, i = 0;
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

  while (1) {
    int nfds = epoll_wait(epollfd, events, 10, -1);
    // if (epoll_ctl(epollfd, EPOLL_CTL_ADD, s, &ev) == -1) {
    //   perror("epoll_ctl: listen_sock");
    //   exit(EXIT_FAILURE);
    // }
    for (i = 0; i < nfds; ++i) {
      if (events[i].data.fd == s) {
        new_fd = accept(s, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1) {
          fprintf(stderr, "Couldn t accept connection");
          exit(EXIT_FAILURE);
        }
        setnonblocking(new_fd);
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = new_fd;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, new_fd, &ev) == -1) {
          perror("epoll_ctl: conn_sock");
          exit(EXIT_FAILURE);
        }
      } else {
        do_use_fd(events[i].data.fd);
      }
    }
  }
  // char msgr[1025];
  // ssize_t n = recv(new_fd, msgr, sizeof(msgr), 0);
  // if (n == -1) {
  //   perror("recv");
  // } else if (n == 0) {
  //   printf("server: client closed the connection\n");
  // } else {
  //   msgr[n] = '\0';
  //   printf("server: got %zd bytes: %s\n", n, msgr);
  //   const char *msgs = "Hello spaffy \n";
  //   if (send(new_fd, msgs, strlen(msgs), 0) == -1) {
  //     perror("send");
  //   } else if (n == 0) {
  //     printf("server: client closed the connection\n");
  //   } else {
  //     printf("Succesfully sent and received");
  //   }
  //}
  close(new_fd);
  close(s);
  return 0;
}