#ifndef CONFIG_H
#define CONFIG_H

typedef struct
{
    char    port[8];
    int     thread_count;
    int     queue_size;
    int     backlog;
    int     initial_timeout;
    int     default_keepalive_timeout;
    int     max_keepalive_timeout;
    int     max_keepalive_requests;
    int     cgi_timeout;
    int     cache_slots;
    long    max_cache_bytes;
    char    data_root[1024];
    int     verbose;
} ServerConfig;

extern const ServerConfig *cfg;

int loadConfig(const char *path);
void printConfig(const ServerConfig *conf);

#endif