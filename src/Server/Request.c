// #define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/Request.h"

/// @brief Create and initialize a new HTTP request object
/// @return A pointer to a newly allocated httpRequest structure
httpRequest *newHttpRequest()
{
    httpRequest *new = (httpRequest *)malloc(sizeof(httpRequest));
    new->header_count = 0;
    new->body = NULL;
    new->bodyLen = 0;
    return new;
}

/// @brief Print the contents of an HTTP request structure
/// @param req The HTTP request to display
void printHttpRequest(httpRequest *req)
{
    printf("Method : %s\n", req->method);
    printf("target : %s\n", req->target);
    printf("version : %s\n", req->version);
    printf("Headers:%d \n", req->header_count);
    for (int i = 0; i < req->header_count; i++)
    {
        printf("%d :%s->%s\n", i + 1, req->headers[i].name, req->headers[i].value);
    }
    printf("End of Request!\n");
}

/// @brief Free all memory allocated for an HTTP request
/// @param req The HTTP request to release
void destroyRequest(httpRequest *req)
{
    if (req->body)
        free(req->body);
    free(req);
}

/// @brief Add a header to an HTTP request structure
/// @param request The HTTP request to update
/// @param h The header to append
void addHeader(httpRequest *request, Header h)
{
    request->headers[request->header_count++] = h;
}

/// @brief Extract all data from the msg to create a valid httpRequest
/// @param request an initialized httpRequest
/// @param msg a string of the request
/// @return 0 in success , 1 in faliure
int parseRequestMessage(httpRequest *request, const char *msg)
{

    char *copy = malloc(strlen(msg) + 1);
    if (!copy)
        return 1;
    strcpy(copy, msg);

    char *start_body = strstr(copy, "\r\n\r\n");
    if (!start_body)
    {
        free(copy);
        return 1;
    }
    start_body += 4;

    char *request_field;
    int tokens_count = 0;
    char *save_ptr1, *save_ptr2;
    char *token = strtok_r(copy, "\r\n", &save_ptr1);

    while (token)
    {
        if (!tokens_count) // request line
        {
            int count = 0;
            request_field = strtok_r(token, " ", &save_ptr2);
            while (request_field)
            {
                switch (count)
                {
                case 0:
                    strcpy(request->method, request_field);
                    break;
                case 1:
                    if (!strcmp(request_field, "/"))
                        strcpy(request->target, "/index.html");
                    else
                        strcpy(request->target, request_field);
                    break;
                case 2:
                    strcpy(request->version, request_field);
                    break;
                default:
                    // printf("Error in Request Line !! additional field: %s\n",
                    //        request_field);
                    free(copy);
                    return 1;
                }
                request_field = strtok_r(NULL, " ", &save_ptr2);
                count++;
            }
            if (count < 3)
            {
                free(copy);
                return 1;
            }
        }
        else // headers
        {

            if (token >= start_body)
                break;

            char *colon = strchr(token, ':');
            if (!colon)
            {
                token = strtok_r(NULL, "\r\n", &save_ptr1);
                tokens_count++;
                continue;
            }

            *colon = '\0';
            colon++;
            char *name = token;
            char *value = colon + strspn(colon, " ");

            Header newHeader;
            strcpy(newHeader.name, name);
            strcpy(newHeader.value, value);

            if (!strcmp(name, "Content-Length"))
            {
                int len = atoi(value);
                if (len > 0)
                {
                    request->body = malloc(len);
                    if (request->body)
                    {
                        memcpy(request->body, start_body, len);
                        request->bodyLen = len;
                    }
                }
            }

            addHeader(request, newHeader);
        }

        tokens_count++;
        token = strtok_r(NULL, "\r\n", &save_ptr1);
    }

    free(copy);
    return 0;
}

/// @brief Determine the HTTP status code for a parsed request
/// @param request The HTTP request to validate
/// @return The corresponding status code for the request
int getStatusCode(httpRequest *request)
{

    // Validate method
    if (strcmp(request->method, "GET") != 0 && strcmp(request->method, "POST") != 0)
        return 405;

    // Validate version to be HTTP/1.1
    if (strcmp(request->version, "HTTP/1.1") != 0)
        return 505; // HTTP Version Not Supported

    // Validate target
    if (request->target[0] != '/')
        return 400; // Bad Request

    // Validate there is no traversal
    if( strstr(request->target,"../") )
    {
        return 403; // Forbidden
    }
    char scriptPath[MAX_PATH];
    strncpy(scriptPath, request->target, MAX_PATH - 1);
    char *q = strchr(scriptPath, '?');
    if (q)
        *q = '\0'; // cut off query string

    char totalPath[MAX_PATH] = "/home/rafiq/final_project/src/data";
    strcat(totalPath, scriptPath);
    if (strcmp("/stats", request->target) != 0)
    {
        FILE *f = fopen(totalPath, "rb");
        if (!f)
        {
            // printf("Cannot read file : %s\n", totalPath);
            return 404; // FILE NOT FOUND
        }
        fclose(f); // In case it's opened
    }

    int host = 0;
    for (int i = 0; i < request->header_count; i++)
    {
        // HTTP/1.1 requires host to exist
        if (strcasecmp(request->headers[i].name, "Host") == 0)
        {
            // printf("Host is :%s\n", request->headers[i].value);
            if (!host)
                host = 1;
            else // More than one host headers!!
            {
                host = 0;
                break;
            }
        }
    }
    if (!host)
        return 400;

    return 200; // Success!
}

/// @brief Extract the connection timeout value from the request headers
/// @param request The HTTP request to inspect
/// @return 0 for keep-alive, -1 for close, or the timeout value in seconds
int getTimeout(httpRequest *request)
{
    for (int i = 0; i < request->header_count; i++)
    {
        if (strcasecmp(request->headers[i].name, "Connection") == 0)
        {
            if (strcmp(request->headers[i].value, "close") == 0)
            {
                return -1; // Close
            }
        }
        if (strcasecmp(request->headers[i].name, "Keep-Alive") == 0)
        {
            char *timeout_str = NULL;
            timeout_str = strstr(request->headers[i].value, "timeout=");
            if (timeout_str)
            {
                timeout_str += strlen("timeout=");
                int timeout = atoi(timeout_str);
                return timeout;
            }
        }
    }
    return 0;
}
