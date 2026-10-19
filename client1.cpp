#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int main() {
  int s, numbytes;
  char buf[MAXDATASIZE];
  struct addrinfo hints, *res, *rp;
  char sendLine[150];
  char addr_s[INET6_ADDRSTRLEN];
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
  inet_ntop(rp->ai_family, get_in_addr(rp->ai_addr), addr_s, sizeof(addr_s));
  printf("client: connected to %s\n", addr_s);
  freeaddrinfo(res);
  while (1) {
    scanf("%s", sendLine);
    int n = send(s, sendLine, sizeof sendLine, 0);
    // if (n == -1) {
    //   perror("send");
    // } else if (n == 0) {
    //   printf("server: client closed the connection\n");
    // } else {
    //   printf("Succesfully sent and received");
    // }
    if ((numbytes = recv(s, buf, MAXDATASIZE - 1, 0)) == -1) {
      perror("recv");
      exit(1);
    }
  }

  buf[numbytes] = '\0';

  printf("client: received '%s'\n", buf);

  close(s);

  return 0;
}