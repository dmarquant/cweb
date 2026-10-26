#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "http.h"

#define MAX_CONNECTIONS 64

typedef struct {
  int fd;
  int rp;
  char buffer[4096];
} Connection;

typedef struct {
  int sock;
  int epollfd;

  bool accepting_stopped;

  // TODO: Move it to the heap
  Connection connections[MAX_CONNECTIONS];
} HttpServer;

int HttpServer_init(HttpServer* s, int sockfd) {
  // TODO: Take in the address to listen to instead

  int epollfd = epoll_create1(0);
  if (epollfd == -1) {
    perror("failed to create epoll fd");
    return -1;
  }
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = -1;

  int ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev);
  if (ret == -1) {
    perror("failed to register epoll on fd");
    return -1;
  }

  memset(s, 0, sizeof(*s));
  s->accepting_stopped = false;
  s->sock = sockfd;
  s->epollfd = epollfd;
  return 0;
}

int HttpServer_close_connection(HttpServer* s, Connection* c) {
  epoll_ctl(s->epollfd, EPOLL_CTL_DEL, c->fd, 0);
  close(c->fd);
  memset(c, 0, sizeof(Connection));

  if (s->accepting_stopped) {
    s->accepting_stopped = false;
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = -1;
    epoll_ctl(s->epollfd, EPOLL_CTL_ADD, s->sock, &ev);
  }
}

// TODO: Return type should be different. It should contain the open connection, the current read buffer and the parsed http request.
int HttpServer_next_connection(HttpServer* s, Connection* new_connection) {
  struct epoll_event events[MAX_CONNECTIONS+1];

  while (true) {
    int nevents = epoll_wait(s->epollfd, events, MAX_CONNECTIONS+1, -1);
    assert(nevents > -1); // TODO: What errors can happen here?

    for (int e = 0; e < nevents; e++) {
      if (events[e].data.fd == -1) {
        struct epoll_event clientev;
        clientev.events = EPOLLIN;

        Connection* connection = NULL;
        for (int c = 0; c < MAX_CONNECTIONS; c++) {
          if (s->connections[c].fd == 0) {
            clientev.data.fd = c;
            connection = &s->connections[c];
            break;
          }
        }
        if (!connection) {
          printf("(server) no free connection. cannot accept\n");
          s->accepting_stopped = true;
          epoll_ctl(s->epollfd, EPOLL_CTL_DEL, s->sock, 0);
          continue;
        }

        struct sockaddr incoming;
        socklen_t incoming_len;
        // TODO: Loop if EINTR
        int client = accept(s->sock, &incoming, &incoming_len);
        if (client > 0) {
          printf("(server) New connection: %d!\n", client);

          memset(connection, 0, sizeof(Connection));
          connection->fd = client;
          epoll_ctl(s->epollfd, EPOLL_CTL_ADD, client, &clientev);

        } else {
          printf("(server) Error!\n");
        }
      } else {
        assert(events[e].data.fd < MAX_CONNECTIONS);
        int cix = events[e].data.fd;
        Connection* c = &s->connections[cix];

        // TODO: Loop if EINTR
        int nread = read(c->fd, c->buffer + c->rp, sizeof(c->buffer) - c->rp);
        if (nread <= 0) {
          HttpServer_close_connection(s, c);
        } else {
          c->rp += nread;

          HttpRequest request = {};
          int parsed = HttpRequest_parse(c->buffer, c->rp, &request);
          if (parsed == 0) {
            printf("HTTP method: %.*s\n", request.method.len, c->buffer + request.method.off);
            printf("HTTP url: %.*s\n", request.url.len, c->buffer + request.url.off);
            printf("HTTP version: %.*s\n", request.version.len, c->buffer + request.version.off);
            for (int h = 0; h < request.num_headers; h++) {
              printf("  %.*s: %.*s\n",
                  request.headers[h].key.len, c->buffer + request.headers[h].key.off,
                  request.headers[h].value.len, c->buffer + request.headers[h].value.off);
            }
            return 0;
          } else {
            HttpServer_close_connection(s, c);
          }
        }
      }
    }
  }
}

int main(int argc, char** argv) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  assert(sock > 0);

  int ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
  assert(ret == 0);

  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(8001),
    .sin_addr = htonl(INADDR_LOOPBACK),
  };

  ret = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
  assert(ret == 0);

  ret = listen(sock, 256);
  assert(ret == 0);

  HttpServer server = {};
  HttpServer_init(&server, sock);

  HttpServer_next_connection(&server, 0);
}
