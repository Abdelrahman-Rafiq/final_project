#define _POSIX_C_SOURCE 200112L 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Request.h"

httpRequest *newHttpRequest()
{
    httpRequest *new = (httpRequest *)malloc(sizeof(httpRequest));
    new->header_count = 0;
    new->first_header = NULL;
    return new;
}

void addHeader(httpRequest *request, Header * h)
{
    Header *new = (Header *)malloc(sizeof(Header));
}

httpRequest *parseRequestMessage(char *msg)
{
    httpRequest *request = newHttpRequest();
    char copy[strlen(msg) + 1];
    strcpy(copy, msg); // Get a copy for safe tokenization
    char *request_field, *header_field;
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
                // printf("Request line field: %s\n", request_field);
                switch (count)
                {
                case 0:
                    strcpy(request->method, request_field);
                    printf("Request line field:method:%s\n", request_field);
                    break;
                case 1:
                    strcpy(request->target, request_field);
                    printf("Request line field:target:%s\n", request_field);
                    break;
                case 2:
                    strcpy(request->version, request_field);
                    printf("Request line field:version:%s\n", request_field);
                    break;
                default:
                    printf("Error in Request Line !! additional field :%s\n",request_field);
                    free(request);
                    return NULL;
                    break;
                }

                request_field = strtok_r(NULL, " ", &save_ptr2);
                count ++;
            }
        }
        else
        { // Headers only
            Header * newHeader = (Header *)malloc(sizeof(Header));
            int count = 0;
            header_field = strtok_r(token, ": ", &save_ptr2);
            while (header_field)
            {
                
                switch (count)
                {
                case 0:
                    strcpy(newHeader->name,header_field);
                    printf("header field :Name:%s\n", header_field);
                    break;
                case 1:
                    strcpy(newHeader->value, header_field);
                    printf("header field :value:%s\n", header_field);
                    break;
                default:
                    printf("Error in header Line !! additional field :%s\n",header_field);
                    free(request);
                    return NULL;
                    break;
                }
                header_field = strtok_r(NULL, " ", &save_ptr2);
                count++;
            }
            addHeader(request,newHeader);
        }
        tokens_count++;
        token = strtok_r(NULL, "\r\n", &save_ptr1);
    }
    return request;
}