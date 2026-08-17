#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../../include/Config.h"

/*
 * Config.c
 * Server configuration loader and validator for startup settings,
 * static paths, and security checks.
 *
 * AI assistance: Claude by Anthropic was used as a guidance tool
 * during development — for discussing configuration validation,
 * security boundaries, and startup defaults.
 * All code was written and verified by the author.
 */


ServerConfig       _config;
const ServerConfig *cfg = &_config;


/// @brief Load server configuration from a configuration file
/// @param path The file path to load configuration from
/// @return 0 on success, or 1 if configuration is invalid or required parameters are missing
int loadConfig(const char *path)
{
    // Defaults
    strncpy(_config.port,      "3490", sizeof _config.port);
    _config.thread_count              = 16;
    _config.queue_size                = 50;
    _config.backlog                   = 10;
    _config.initial_timeout           = 5;
    _config.default_keepalive_timeout = 30;
    _config.max_keepalive_timeout     = 120;
    _config.max_keepalive_requests    = 100;
    _config.cgi_timeout               = 5;
    _config.cache_slots               = 1024;
    _config.max_cache_bytes           = 67108864;   // 64 MB
    strncpy(_config.data_root, "None", sizeof _config.data_root);
    _config.verbose                   = 0;

    if (!path)
    {
        fprintf(stderr, "config: no path given — data_root is mandatory\n");
        return 1;
    }

    FILE *f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "config: cannot open '%s'\n", path);
        return 1;
    }

    char line[512];
    int  lineno = 0;

    while (fgets(line, sizeof line, f))
    {
        lineno++;

        // skip blank lines and comments
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;

        char key[128], value[256];

        if (sscanf(line, "%127[^=]=%255[^\n]", key, value) != 2)
        {
            fprintf(stderr, "config: malformed line %d: %s", lineno, line);
            continue;
        }

        // trim trailing whitespace from key
        char *k = key + strlen(key) - 1;
        while (k >= key && (*k == ' ' || *k == '\t' || *k == '\r'))
            *k-- = '\0';

        // trim leading whitespace from value
        char *v = value + strspn(value, " \t");

        // trim trailing whitespace from value
        char *v_end = v + strlen(v) - 1;
        while (v_end >= v && (*v_end == ' ' || *v_end == '\t' || *v_end == '\r'))
            *v_end-- = '\0';

        if      (!strcmp(key, "port"))
            strncpy(_config.port, v, sizeof _config.port - 1);
        else if (!strcmp(key, "thread_count"))
            _config.thread_count              = atoi(v);
        else if (!strcmp(key, "queue_size"))
            _config.queue_size                = atoi(v);
        else if (!strcmp(key, "backlog"))
            _config.backlog                   = atoi(v);
        else if (!strcmp(key, "initial_timeout"))
            _config.initial_timeout           = atoi(v);
        else if (!strcmp(key, "default_keepalive_timeout"))
            _config.default_keepalive_timeout = atoi(v);
        else if (!strcmp(key, "max_keepalive_timeout"))
            _config.max_keepalive_timeout     = atoi(v);
        else if (!strcmp(key, "max_keepalive_requests"))
            _config.max_keepalive_requests    = atoi(v);
        else if (!strcmp(key, "cgi_timeout"))
            _config.cgi_timeout               = atoi(v);
        else if (!strcmp(key, "cache_slots"))
            _config.cache_slots               = atoi(v);
        else if (!strcmp(key, "max_cache_bytes"))
            _config.max_cache_bytes           = atol(v);
        else if (!strcmp(key, "data_root"))
            strncpy(_config.data_root, v, sizeof _config.data_root - 1);
        else if (!strcmp(key, "verbose"))
            _config.verbose                   = atoi(v);
        else
            fprintf(stderr, "config: unknown key '%s' on line %d\n", key, lineno);
    }

    fclose(f);

    
    // Security Check for data root 
    // must have been set
    if (strcmp(_config.data_root, "None") == 0)
    {
        fprintf(stderr, "config: data_root is mandatory\n");
        return 1;
    }

    // must be absolute path
    if (_config.data_root[0] != '/')
    {
        fprintf(stderr, "config: data_root must be an absolute path\n");
        return 1;
    }

    // block sensitive system directories
    const char *blocked[] = {
        "/", "/etc", "/home", "/root",
        "/usr", "/bin", "/sbin", "/var",
        "/tmp", "/dev", "/proc", "/sys",
        NULL
    };
    for (int i = 0; blocked[i] != NULL; i++)
    {
        if (strcmp(_config.data_root, blocked[i]) == 0)
        {
            fprintf(stderr, "config: data_root '%s' is a sensitive system directory\n",
                    _config.data_root);
            return 1;
        }
    }

    // must exist and be a directory
    struct stat st;
    if (stat(_config.data_root, &st) != 0)
    {
        fprintf(stderr, "config: data_root '%s' does not exist\n",
                _config.data_root);
        return 1;
    }
    if (!S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "config: data_root '%s' is not a directory\n",
                _config.data_root);
        return 1;
    }

    //  validate numeric fields 
    if (_config.thread_count <= 0)
    {
        fprintf(stderr, "config: thread_count must be > 0\n");
        return 1;
    }
    if (_config.queue_size <= 0)
    {
        fprintf(stderr, "config: queue_size must be > 0\n");
        return 1;
    }
    if (_config.cache_slots <= 0)
    {
        fprintf(stderr, "config: cache_slots must be > 0\n");
        return 1;
    }
    if (_config.max_cache_bytes <= 0)
    {
        fprintf(stderr, "config: max_cache_bytes must be > 0\n");
        return 1;
    }

    return 0;
}

/// @brief Print the current server configuration to standard output
/// @param conf The server configuration structure to display
void printConfig(const ServerConfig *conf)
{
    printf("port                     = %s\n",  conf->port);
    printf("thread_count             = %d\n",  conf->thread_count);
    printf("queue_size               = %d\n",  conf->queue_size);
    printf("backlog                  = %d\n",  conf->backlog);
    printf("initial_timeout          = %d\n",  conf->initial_timeout);
    printf("default_keepalive_timeout= %d\n",  conf->default_keepalive_timeout);
    printf("max_keepalive_timeout    = %d\n",  conf->max_keepalive_timeout);
    printf("max_keepalive_requests   = %d\n",  conf->max_keepalive_requests);
    printf("cgi_timeout              = %d\n",  conf->cgi_timeout);
    printf("cache_slots              = %d\n",  conf->cache_slots);
    printf("max_cache_bytes          = %ld\n", conf->max_cache_bytes);
    printf("data_root                = %s\n",  conf->data_root);
    printf("verbose                  = %d\n",  conf->verbose);
}