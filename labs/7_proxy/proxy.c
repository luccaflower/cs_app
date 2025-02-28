#include "csapp.h"
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

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

    puts("Request headers:");
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
    Rio_readinitb(&client_rio, clientfd);
    Rio_readlineb(&client_rio, client_buf, MAXLINE);
    puts(client_buf);
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
    int serverfd = Open_clientfd(dest.host, dest.port);
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
    Rio_writen(serverfd, to_server_buf, strlen(to_server_buf));

    Rio_readinitb(&server_rio, serverfd);
    puts("FROM SERVER TO CLIENT");
    printf("%s", from_server_buf);
    while (Rio_readlineb(&server_rio, from_server_buf, MAXLINE) != 0) {
        Rio_writen(clientfd, from_server_buf, strlen(from_server_buf));
        printf("%s", from_server_buf);
    }

    Close(serverfd);
    Close(clientfd);
}
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage; %s <port>\n", argv[0]);
    }
    int listenfd = Open_listenfd(argv[1]);
    while (1) {
        struct sockaddr_storage clientaddr;
        unsigned clientlen = sizeof(clientaddr);
        int connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        char client_hostname[MAXLINE], client_port[MAXLINE];
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE,
                    client_port, MAXLINE, 0);
        printf("connected to %s:%s\n", client_hostname, client_port);
        forward(connfd);
    }
}
