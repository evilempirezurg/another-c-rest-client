//
// Created by jfranc11 on 2/23/2022.
//
#include <stdio.h>

#define CHUNK_SIZE  2048
#define RESPONSE_BODY_MAX 4096
#define TO_USEC     50000
#define TRUE        1
#define FALSE       0
//#define UNITTEST
//#define _DEBUG

#ifndef MINT_RESTCLIENT_H
#define MINT_RESTCLIENT_H

typedef struct {
    char http_ver[30];
    int status_code;
    int content_length;
    char date_time_str[30];
    char connection_type[30];
    char response_body[4096];
} HTTP_RESPONSE;

/**
 * Print value of global errno
 * @param error_num
 */
void print_error_details(const int error_num);

/**
 * creates connection to HTTP host
 * @param hostname
 * @param port
 * @return socket connection
 */
int connect_to_host(const char* hostname, const int port);

/**
 *
 * @param sockfd
 * @param argv - <method> <path> [<data> [<headers>]]
 * @param console
 * @return number of bytes sent to server
 */
int send_request(int sockfd, char *argv[], int argc);

/**
 * Receive response from server
 * @param socketfd
 * @param timeout
 * @return full HTTP response, use parse_response to select parts of response
 */
char* recv_response(int sockfd, int timeout);

/**
 * Parse the response, see type definition
 * @param full_http_response
 * @return
 */
HTTP_RESPONSE parse_response(char* full_http_response);

#endif //MINT_RESTCLIENT_H
