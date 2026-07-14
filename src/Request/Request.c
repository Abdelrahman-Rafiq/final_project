#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/Request.h"

// Create httpRequest & initialize it
httpRequest *newHttpRequest()
{
    httpRequest *new = (httpRequest *)malloc(sizeof(httpRequest));
    new->header_count = 0;
    return new;
}

// Print all fields of httpRequest
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

// Free Allocated Memory
void destroyRequest(httpRequest *req)
{
    free(req);
}
// Add header to the httpRequest structure
void addHeader(httpRequest *request, Header h)
{
    request->headers[request->header_count++] = h;
}

/// @brief Extract all data from the msg to create a valid httpRequest
/// @param request an initialized httpRequest
/// @param msg a string of the request
/// @return 0 in success , 1 in faliure
int parseRequestMessage(httpRequest *request, char *msg)
{
    char copy[strlen(msg) + 1];
    strcpy(copy, msg); // Get a copy for safe tokenization
    char *request_field;
    int tokens_count = 0;
    char *save_ptr1, *save_ptr2;
    char *token = strtok_r(copy, "\r\n", &save_ptr1);
    while (token)
    {
        if (!tokens_count) // request line to be stored
        {
            int count = 0;
            request_field = strtok_r(token, " ", &save_ptr2);

            while (request_field)
            {
                switch (count)
                {
                case 0:
                    strcpy(request->method, request_field);
                    // printf("Request line field:method:%s\n", request_field);
                    break;
                case 1:
                    strcpy(request->target, request_field);
                    // printf("Request line field:target:%s\n", request_field);
                    break;
                case 2:
                    strcpy(request->version, request_field);
                    // printf("Request line field:version:%s\n", request_field);
                    break;
                default:
                    printf("Error in Request Line !! additional field :%s\n", request_field);
                    free(request);
                    return 1;
                    break;
                }

                request_field = strtok_r(NULL, " ", &save_ptr2);
                count++;
            }
            if (count < 3)
                return 1;
        }
        else
        { // Headers only
            Header newHeader;
            char *colon = strchr(token, ':');
            colon[0] = '\0';
            colon++;
            char *name = token;
            char *value = colon + strspn(colon, " ");
            strcpy(newHeader.name, name);
            strcpy(newHeader.value, value);
            addHeader(request, newHeader);
        }
        tokens_count++;
        token = strtok_r(NULL, "\r\n", &save_ptr1);
    }
    return 0;
}

// Get status code from the request
int getStatusCode(httpRequest *request)
{
    // Validate method
    if (strcmp(request->method, "GET") != 0)
        return 405;

    // Validate version to be HTTP/1.1
    if (strcmp(request->version, "HTTP/1.1") != 0)
        return 505; // HTTP Version Not Supported

    // Validate target
    if (request->target[0] != '/')
        return 400; // Bad Request
    char path[MAX_PATH] = "/home/rafiq/final_project/src/data";
    strcat(path, request->target);
    FILE *f = fopen(path, "r");
    if (!f)
    {
        printf("Cannot read file : %s\n", path);
        return 404; // FILE NOT FOUND
    }

    fclose(f); // In case it's opened

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

// Return 0 in keep-alive -1 for close or timeout in seconds
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
