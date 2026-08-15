#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/Config.h"

ServerConfig _config;

const ServerConfig *cfg = &_config;

int loadConfig(const char *path)
{
    // set defaults first
    strncpy(_config.port, "3490", sizeof _config.port);
    _config.thread_count = 16;
    _config.queue_size = 50;
    _config.backlog = 10;
    _config.initial_timeout = 5;
    _config.default_keepalive_timeout = 30;
    _config.max_keepalive_timeout = 120;
    _config.max_keepalive_requests = 100;
    _config.cache_slots = 1024;
    _config.max_cache_bytes = 67108864;
    strncpy(_config.data_root,
            "/home/rafiq/final_project/src/data",
            sizeof _config.data_root);
    _config.verbose = 0;

    if (!path)
        return 0; // use defaults
        
    FILE *f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "config: cannot open %s, using defaults\n", path);
        return 0;
    }
    char line[256];
    while (fgets(line, sizeof line, f))
    {
        // skip comments and blank lines
        if (line[0] == '#' || line[0] == '\n') continue;

        char key[128], value[128];
        if (sscanf(line, "%127[^=]=%127s", key, value) != 2) continue;

        if      (!strcmp(key, "port"))                     strncpy(_config.port, value, sizeof _config.port);
        else if (!strcmp(key, "thread_count"))             _config.thread_count              = atoi(value);
        else if (!strcmp(key, "queue_size"))               _config.queue_size                = atoi(value);
        else if (!strcmp(key, "backlog"))                  _config.backlog                   = atoi(value);
        else if (!strcmp(key, "initial_timeout"))          _config.initial_timeout           = atoi(value);
        else if (!strcmp(key, "default_keepalive_timeout"))_config.default_keepalive_timeout = atoi(value);
        else if (!strcmp(key, "max_keepalive_timeout"))    _config.max_keepalive_timeout     = atoi(value);
        else if (!strcmp(key, "max_keepalive_requests"))   _config.max_keepalive_requests    = atoi(value);
        else if (!strcmp(key, "cache_slots"))              _config.cache_slots               = atoi(value);
        else if (!strcmp(key, "max_cache_bytes"))          _config.max_cache_bytes           = atol(value);
        else if (!strcmp(key, "data_root"))                strncpy(_config.data_root, value, sizeof _config.data_root);
        else if (!strcmp(key, "verbose"))                  _config.verbose                   = atoi(value);
        else fprintf(stderr, "config: unknown key '%s'\n", key);
    }

    return 0;
}

// Just for debugging
void printConfig(const ServerConfig *conf)
{
    printf("port=%s\n", conf->port);
    printf("thread_count=%d\n", conf->thread_count);
    printf("queue_size=%d\n", conf->queue_size);
    printf("backlog=%d\n", conf->backlog);
    printf("initial_timeout=%d\n", conf->initial_timeout);
    printf("default_keepalive_timeout=%d\n", conf->default_keepalive_timeout);
    printf("max_keepalive_timeout=%d\n", conf->max_keepalive_timeout);
    printf("max_keepalive_requests=%d\n", conf->max_keepalive_requests);
    printf("cache_slots=%d\n", conf->cache_slots);
    printf("max_cache_bytes=%ld\n", conf->max_cache_bytes);
    printf("data_root=%s\n", conf->data_root);
    printf("verbose=%d\n", conf->verbose);
}

int main(void)
{
    loadConfig("/home/rafiq/final_project/src/server.conf");
    printConfig(cfg);

}
