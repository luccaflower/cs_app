#include "csapp.h"
#include <bits/pthreadtypes.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_URL_LEN 2048

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
struct headers {
    char host[MAX_URL_LEN];
    char const *user_agent;
    char const *connection;
    char const *proxy_connection;
};

struct destination {
    char *host;
    char *port;
};

struct request {
    char method[MAXLINE];
    char uri[MAX_URL_LEN];
    char *version;
};

void echo(int connfd) {
    char buf[MAXLINE];
    rio_t rio;
    Rio_readinitb(&rio, connfd);
    ssize_t n;
    while ((n = rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        if (n < 0) {
            Close(connfd);
            return;
        }
        printf("server received %zu bytes \n", n);
        int written = rio_writen(connfd, buf, n);
        if (written < 0) {
            Close(connfd);
            return;
        }
        puts(buf);
    }
}

int parse_request(char *request_line, struct request *out) {
    char ignored[MAXLINE];
    char uri[MAXLINE];
    char *parsed;
    int num = sscanf(request_line, "%s %s %s", out->method, uri, ignored);
    parsed = strchr(uri, '/');
    parsed = strchr(parsed + 1, '/');
    parsed = strchr(parsed + 1, '/');
    strcpy(out->uri, parsed);
    return num;
}

void parse_host(char *host_header, struct destination *out) {
    out->host = strtok(host_header, ":");
    char *port = strtok(NULL, ":");
    out->port = port ? port : "80";
}

void read_requesthdr(rio_t *rio, struct headers *hdr) {
    char buf[MAXLINE];
    Rio_readlineb(rio, buf, MAXLINE);

    while (strcmp(buf, "\r\n")) {
        sscanf(buf, "Host: %s", hdr->host);
        Rio_readlineb(rio, buf, MAXLINE);
    }
    if (!*hdr->host) {
        strcpy(hdr->host, "www.cs.cmu.com");
    }
}

void forward(int clientfd) {
    // read request headers
    // if no host header, attach www.cmu.edu host header
    // always attach user-agent: <user_agent_hdr>
    // always attach connection: close
    // always attach proxy-connection: close
    // forward request as http/1.0 to server
    // forward server response to client
    char client_buf[MAXLINE], from_server_buf[MAXLINE], to_server_buf[MAXLINE];
    rio_t client_rio, server_rio;
    rio_readinitb(&client_rio, clientfd);
    if (rio_readlineb(&client_rio, client_buf, MAXLINE) < 0) {
        printf("Error while reading request from client: %s\n",
               strerror(errno));
        close(clientfd);
        return;
    }
    struct request request;
    if (parse_request(client_buf, &request) != 3) {
        printf("invalid format: %s", client_buf);
        Close(clientfd);
        return;
    }
    request.version = "HTTP/1.0";
    struct headers hdr;
    memset(&hdr, 0, sizeof(hdr));
    read_requesthdr(&client_rio, &hdr);
    hdr.connection = "close";
    hdr.proxy_connection = "close";
    hdr.user_agent = user_agent_hdr;

    struct destination dest;
    parse_host(hdr.host, &dest);
    int serverfd = open_clientfd(dest.host, dest.port);
    if (serverfd < 0) {
        printf("Error connecting to %s on port %s: %s\n", dest.host, dest.port,
               strerror(errno));
        close(clientfd);
        return;
    }
    puts("FROM CLIENT TO SERVER");
    sprintf(to_server_buf,
            "GET %s HTTP/1.0\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n"
            "Proxy-Connection: close\r\n"
            "User-Agent: %s\r\n"
            "\r\n",
            request.uri, hdr.host, user_agent_hdr);
    printf("%s", to_server_buf);
    if (rio_writen(serverfd, to_server_buf, strlen(to_server_buf)) < 0) {
        printf("Failed to write complete request to server: %s\n",
               strerror(errno));
        close(serverfd);
        close(clientfd);
    }

    rio_readinitb(&server_rio, serverfd);
    int read;
    while ((read = rio_readnb(&server_rio, from_server_buf, MAXLINE)) != 0) {
        if (rio_writen(clientfd, from_server_buf, read) < 0) {
            printf("Failed to write complete server response to client: %s\n",
                   strerror(errno));
            close(serverfd);
            close(clientfd);
            return;
        }
    }

    puts("Request complete");

    close(serverfd);
    close(clientfd);
}

void sigchld_handler(int sig) {
    while (waitpid(-1, 0, WNOHANG) > 0) {
    }
    return;
}

void *forward_thread(void *vargp) {
    int clientfd = *(int *)vargp;
    Free(vargp);
    Pthread_detach(pthread_self());
    forward(clientfd);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage; %s <port>\n", argv[0]);
    }
    int listenfd = Open_listenfd(argv[1]);
    sigset_t mask;
    Sigemptyset(&mask);
    Sigaddset(&mask, SIGPIPE);
    Sigprocmask(SIG_BLOCK, &mask, NULL);
    Signal(SIGCHLD, sigchld_handler);

    pthread_t tid;
    while (1) {
        struct sockaddr_storage clientaddr;
        unsigned clientlen = sizeof(clientaddr);
        int *connfdp = Malloc(sizeof(*connfdp));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, forward_thread, (void *)connfdp);
    }
}
