#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Request.h"

//Create httpRequest & initialize it
httpRequest *newHttpRequest()
{
    httpRequest *new = (httpRequest *)malloc(sizeof(httpRequest));
    new->header_count = 0;
    return new;
}

//Print all fields of httpRequest 
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

//Add header to the httpRequest structure
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
        }
        else
        { // Headers only
            Header newHeader;
            int count = 0;
            header_field = strtok_r(token, ": ", &save_ptr2);
            while (header_field)
            {

                switch (count)
                {
                case 0:
                    strcpy(newHeader.name, header_field);
                    // printf("header field :Name:%s\n", header_field);
                    break;
                case 1:
                    strcpy(newHeader.value, header_field);
                    // printf("header field :value:%s\n", header_field);
                    break;
                default:
                    printf("Error in header Line !! additional field :%s\n", header_field);
                    free(request);
                    return 1;
                    break;
                }
                header_field = strtok_r(NULL, " ", &save_ptr2);
                count++;
            }
            addHeader(request, newHeader);
        }
        tokens_count++;
        token = strtok_r(NULL, "\r\n", &save_ptr1);
    }
    return 0;
}

//Get status code or error numbers from these
int validateRequest(httpRequest* request)
{
    int host =0; 
    for(int i=0;i<request->header_count;i++)
    {
        //HTTP/1.1 requires host to exist
        if(stricmp(request->headers[i].name,"Host") == 0)
        {
            printf("Host is :%s\n",request->headers[i].value);
            host = 1;
        }
    }
}