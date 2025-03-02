#include "csapp.h"
#include <assert.h>
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

struct cache_entry {
    char *url;
    void *content;
    unsigned int content_len;
    struct cache_entry *next;
    struct cache_entry *prev;
};
struct cache {
    unsigned int capacity_bytes;
    unsigned int used_bytes;
    unsigned int max_object_size;
    struct cache_entry *head;
    struct cache_entry *tail;
};

static struct cache cache;

void init_cache(struct cache *cache, unsigned int capacity,
                unsigned int max_object_size) {
    cache->capacity_bytes = capacity;
    cache->used_bytes = 0;
    cache->max_object_size = max_object_size;
    cache->head = NULL;
    cache->tail = NULL;
}

void move_up(struct cache_entry *entry, struct cache *cache) {
    puts("Move up");
    puts(entry->url);
    if (!entry->next && !entry->prev) {
        // only entry in list, nothing to do
        return;
    } else if (!entry->next) {
        // tail of the list
        entry->prev->next = NULL;
        cache->tail = entry->prev;
        entry->next = cache->head;
        cache->head->prev = entry;
        cache->head = entry;
    } else if (!entry->prev) {
        // already at the head of the list, nothing to do
        return;
    }
}
void free_entry(struct cache_entry *entry) {
    free(entry->url);
    free(entry->content);
    free(entry);
}
struct cache_entry *new_entry(char *url, void *content, size_t content_len) {
    struct cache_entry *entry = Malloc(sizeof(*entry));
    entry->url = Malloc(strlen(url));
    strcpy(entry->url, url);
    entry->content = Malloc(content_len);
    memcpy(entry->content, content, content_len);
    entry->content_len = content_len;
    entry->next = NULL;
    entry->prev = NULL;
    return entry;
}
void insert(struct cache_entry *entry, struct cache *cache) {
    assert(entry);
    puts("Inserting new entry for URL");
    puts(entry->url);
    if (!cache->head) {
        puts("Empty cache, inserting first object");
        // no head means empty.
        // this means the entry is the new tail and head
        cache->head = entry;
        cache->tail = entry;
        cache->used_bytes += entry->content_len;
        assert(cache->head);
        assert(cache->tail);
        return;
    }
    while (cache->used_bytes + entry->content_len > cache->capacity_bytes) {
        puts("Reducing cache size");
        cache->used_bytes -= cache->tail->content_len;
        struct cache_entry *old_tail = cache->tail;
        cache->tail = cache->tail->prev;
        cache->tail->next = NULL;
        free_entry(old_tail);
    }
    puts("Inserting new");
    cache->head->prev = entry;
    entry->next = cache->head;
    cache->head = entry;
    assert(cache->head);
    assert(cache->tail);
}

struct cache_entry *get(char *url, struct cache *cache) {
    puts("Trying to find URL");
    puts(url);
    for (struct cache_entry *entry = cache->head; entry; entry = entry->next) {
        puts(entry->url);
        if (!strcmp(entry->url, url)) {
            move_up(entry, cache);
            return entry;
        }
    }
    puts("No entries found");
    return NULL;
}

int parse_request(char *request_line, struct request *out) {
    char ignored[MAXLINE];
    int num = sscanf(request_line, "%s %s %s", out->method, out->uri, ignored);
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
    hdr->connection = "close";
    hdr->proxy_connection = "close";
    hdr->user_agent = user_agent_hdr;
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
    struct cache_entry *entry = get(request.uri, &cache);
    if (entry) {
        puts("Cached entry found!");
        rio_writen(clientfd, entry->content, entry->content_len);
        close(clientfd);
        return;
    }

    char *uri_path = strchr(request.uri, '/');
    uri_path = strchr(uri_path + 1, '/');
    uri_path = strchr(uri_path + 1, '/');
    request.version = "HTTP/1.0";
    struct headers hdr;
    memset(&hdr, 0, sizeof(hdr));
    read_requesthdr(&client_rio, &hdr);

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
            uri_path, hdr.host, user_agent_hdr);
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
        if (read <= cache.capacity_bytes) {
            struct cache_entry *entry =
                new_entry(request.uri, from_server_buf, read);
            insert(entry, &cache);
        }
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
    init_cache(&cache, 1 << 20, 100 * (1 << 10));

    pthread_t tid;
    while (1) {
        struct sockaddr_storage clientaddr;
        unsigned clientlen = sizeof(clientaddr);
        int *connfdp = Malloc(sizeof(*connfdp));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        forward(*connfdp);
        free(connfdp);
    }
}
