//
// Created by jfranc11 on 2/23/2022.
//

#include "restclient.h"
#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <errno.h>

/*
 * Helper Functions only
 */
void parse_http_status(char *status, HTTP_RESPONSE *httpResponse);
void parse_http_cont_len(char *status, HTTP_RESPONSE *httpResponse);
void parse_http_conn_type(char *status, HTTP_RESPONSE *httpResponse);
void parse_http_resp_dt(char *status, HTTP_RESPONSE *httpResponse);

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void print_error_details(const int error_num)
{
    fprintf(stderr, "ERRCODE[%d]: %s\n", error_num, strerror(error_num));
    exit(1);
}

int connect_to_host(const char *hostname, const int port)
{

    struct hostent *server;
    struct sockaddr_in serv_addr;

    /* create the socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        print_error_details(errno);

    server = gethostbyname(hostname);
    if (server == NULL)
        print_error_details(errno);

    /* fill in the structure */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    /* connect the socket */
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        print_error_details(errno);
    }

    return sockfd;
}

/*
 * <method> <path> [<data> [<headers>]]
 */
int send_request(int sockfd, char *argv[], int argc)
{

    int i;
    int bytes, sent, received, total, message_size;
    char *message;

#ifdef _DEBUG
    printf("*** DUMPING PARAMETERS ***\n");
    for (i = 0; i < argc; i++)
    {
        printf("param[%d] := %s\n", i, argv[i]);
    }

    printf("*** END DUMPING PARAMETERS ***\n");
#endif //_DEBUG

    if (argc < 1)
    {
        fprintf(stderr, "Missing required parameters. Usage: <method> <path> [<data> [<headers>]]");
        return -1;
    }

    message_size = 0;
    if (!strcmp(argv[0], "GET"))
    {
        message_size += strlen("%s %s%s%s HTTP/1.0\r\n"); /* method         */
        message_size += strlen(argv[0]);                  /* path           */
        message_size += strlen(argv[1]);                  /* headers        */
        if (argc > 2)
            message_size += strlen(argv[2]); /* query string   */
        for (i = 3; i < argc; i++)           /* headers        */
            message_size += strlen(argv[i]) + strlen("\r\n");
        message_size += strlen("\r\n"); /* blank line     */
    }
    else
    {
        message_size += strlen("%s %s HTTP/1.0\r\n");
        message_size += strlen(argv[0]); /* method         */
        message_size += strlen(argv[1]); /* path           */
        for (i = 3; i < argc; i++)       /* headers        */
            message_size += strlen(argv[i]) + strlen("\r\n");
        if (argc > 2)
            message_size += strlen("Content-Length: %d\r\n") + 10; /* content length */
        message_size += strlen("\r\n");                            /* blank line     */
        if (argc > 2)
            message_size += strlen(argv[2]); /* body           */
    }

    /* initialize the message */
    message = malloc(message_size + 1);
    memset(message, '\0', message_size + 1);

    /* fill in the parameters */
    if (!strcmp(argv[0], "GET"))
    {
        if (argc > 2)
            sprintf(message, "%s %s%s%s HTTP/1.0\r\n",
                    strlen(argv[0]) > 0 ? argv[0] : "GET", /* method         */
                    strlen(argv[1]) > 0 ? argv[1] : "/",   /* path           */
                    strlen(argv[2]) > 0 ? "?" : "",        /* ?              */
                    strlen(argv[2]) > 0 ? argv[2] : "");   /* query string   */
        else
            sprintf(message, "%s %s HTTP/1.0\r\n",
                    strlen(argv[0]) > 0 ? argv[0] : "GET", /* method         */
                    strlen(argv[1]) > 0 ? argv[1] : "/");  /* path           */
        for (i = 3; i < argc; i++)                         /* headers        */
        {
            strcat(message, argv[i]);
            strcat(message, "\r\n");
        }
        strcat(message, "\r\n"); /* blank line     */
    }
    else
    {
        sprintf(message, "%s %s HTTP/1.0\r\n",
                strlen(argv[0]) > 0 ? argv[0] : "POST", /* method         */
                strlen(argv[1]) > 0 ? argv[1] : "/");   /* path           */
        for (i = 3; i < argc; i++)                      /* headers        */
        {
            strcat(message, argv[i]);
            strcat(message, "\r\n");
        }
        if (argc > 2)
            sprintf(message + strlen(message), "Content-Length: %lu\r\n", strlen(argv[2]));
        strcat(message, "\r\n"); /* blank line     */
        if (argc > 2)
            strcat(message, argv[2]); /* body           */
    }

    // printf("Request:\n%s\n", message);

    /* send the request */
    total = strlen(message);
    sent = 0;
    do
    {
        bytes = write(sockfd, message + sent, total - sent);
        if (bytes < 0)
            print_error_details(errno);
        if (bytes == 0)
            break;
        sent += bytes;
    } while (sent < total);

    return sent;
}

/*
    Receive data in multiple chunks by checking a non-blocking socket
    Timeout in seconds
*/
char *recv_response(int socketfd, int timeout)
{
    int size_recv, total_size = 0;
    char chunk[CHUNK_SIZE];

    struct timeval tv = {timeout, TO_USEC};
    setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char *response = (char *)malloc(CHUNK_SIZE * sizeof(char));
    bzero(response, CHUNK_SIZE);
    bzero(chunk, CHUNK_SIZE);
    int inc = 1;

    while ((size_recv = read(socketfd, chunk, CHUNK_SIZE - 1)) > 0)
    {

        int len = strlen(response);
        // printf("current length: %d, received %d \n", len, size_recv);

        if (len + size_recv >= CHUNK_SIZE)
        {
            inc++;
            response = (char *)realloc(response, (CHUNK_SIZE * inc) * sizeof(char));
            // printf("To be length: %d , new buffer size %d\n", (len + size_recv), (CHUNK_SIZE * inc));
        }
        strcat(response, chunk);
        bzero(chunk, CHUNK_SIZE);
        total_size += size_recv;
    }

    if (size_recv < 0)
    {
        print_error_details(errno);
    }

    // printf("Response: size: %d \n%s\n", total_size, response);
    return response;
}

HTTP_RESPONSE parse_response(char *full_http_response)
{

    HTTP_RESPONSE httpResponse;

    char *token = NULL;
    const char http_status[15] = "HTTP";
    const char http_cont_len[16] = "Content-Length:";
    const char http_conn_type[15] = "Connection:";
    const char http_resp_dt[10] = "Date: ";

    int is_start_of_body = FALSE;
    int is_done_http_status = FALSE;
    int is_done_cont_len = FALSE;
    int is_done_resp_dt = FALSE;

    // TODO must grow dynamically, responses may be truncated to 4k
    char response_body[RESPONSE_BODY_MAX];
    memset(response_body, '\0', RESPONSE_BODY_MAX);

    token = strtok(full_http_response, "\r\n");
    // TODO, buggy when comparing to Content-Length vs strlen !
    while (token)
    {
        // printf("Current token: %s\n", token);
        if (is_start_of_body == TRUE)
        {
            if (strlen(response_body) + strlen(token) < RESPONSE_BODY_MAX)
            {
                strcat(response_body, token);
                strcat(response_body, "\n");
            }
            // else reallocate
        }

        if (strstr(token, http_status) && is_done_http_status == FALSE)
        {
            parse_http_status(token, &httpResponse);
            is_done_http_status = TRUE;
        }
        else if (strstr(token, http_cont_len) && is_done_cont_len == FALSE)
        {
            parse_http_cont_len(token, &httpResponse);
            is_done_cont_len = TRUE;
        }
        else if (strstr(token, http_conn_type) && is_start_of_body == FALSE)
        {
            parse_http_conn_type(token, &httpResponse);
            is_start_of_body = TRUE; // after Connection: begins the Message Body...
        }
        else if (strstr(token, http_resp_dt) && is_done_resp_dt == FALSE)
        {
            parse_http_resp_dt(token, &httpResponse);
            is_done_resp_dt = TRUE;
        }

        token = strtok(NULL, "\r\n");
    }

    free(token);

    strcpy(httpResponse.response_body, response_body);
    return httpResponse;
}

// helper function
void parse_http_status(char *status, HTTP_RESPONSE *httpResponse)
{

    char *ptr;
    size_t idx;

    ptr = strchr(status, ' ');
    if (ptr == NULL)
    {
        printf("parse_http_status: Character not found\n");
        return;
    }

    idx = ptr - status;
    char outputString[10];
    memset(outputString, '\0', 10);
    strncpy(outputString, status + idx, 4); // 3 digit code status + \0

    httpResponse->status_code = atoi(outputString);

    memset(httpResponse->http_ver, '\0', 10);
    strncpy(httpResponse->http_ver, status, idx);
}

// helper function
void parse_http_cont_len(char *status, HTTP_RESPONSE *httpResponse)
{

    char *ptr;
    size_t idx;

    ptr = strchr(status, ' ');
    if (ptr == NULL)
    {
        printf("parse_http_cont_len: Character not found\n");
        return;
    }

    idx = ptr - status;
    char outputString[10];
    memset(outputString, '\0', 10);
    strncpy(outputString, status + idx, strlen(status) - idx);
    httpResponse->content_length = atoi(outputString);
}

void parse_http_conn_type(char *status, HTTP_RESPONSE *httpResponse)
{

    char *ptr;
    size_t idx;

    ptr = strchr(status, ' ');
    if (ptr == NULL)
    {
        fprintf(stderr, "parse_http_conn_type: Character not found\n");
        return;
    }

    idx = ptr - status;
    strncpy(httpResponse->connection_type, status + idx + 1, strlen(status) - idx);
}

void parse_http_resp_dt(char *status, HTTP_RESPONSE *httpResponse)
{
    char *ptr;
    size_t idx;

    ptr = strchr(status, ' ');
    if (ptr == NULL)
    {
        fprintf(stderr, "parse_http_resp_dt: Character not found\n");
        return;
    }

    idx = ptr - status;
    strncpy(httpResponse->date_time_str, status + idx + 1, strlen(status) - idx);
}

#ifdef UNITTEST

int main(int argc, char *argv[])
{

    if (argc < 3)
    {
        error("missing required args");
    }

    // CONNECT
    int socketfd = connect_to_host(argv[1], atoi(argv[2]));
    int size_of_params, i, k;

    size_of_params = argc - 3;
    char *params[size_of_params];

    for (i = 3; i < argc; i++)
    {
        k = i - 3;

        params[k] = malloc(strlen(argv[i]) + 1);
        memset(params[k], '\0', strlen(argv[i]) + 1);
        strcpy(params[k], argv[i]);
        // printf("got k=%d, i=%d: %s\n", k, i, params[k]);
    }

    // SEND
    int sent_bytes = send_request(socketfd, params, size_of_params);
    if (sent_bytes < 1)
        perror("failed to send message");

    // EXPECT A RESPONSE FROM SERVER
    char *response = (char *)malloc(CHUNK_SIZE * sizeof(char));
    bzero(response, CHUNK_SIZE);

    response = recv_response(socketfd, 30);
    if (strlen(response) < 1)
        error("no response from server");

    // WE'RE DONE WITH CONNECTION
    if (socketfd)
        close(socketfd);

    // printf("*** Begin Response Received ***\r\n %s\n", response);
    // printf("*** End Response Received ***\n");

    // parse
    HTTP_RESPONSE httpResponse = parse_response(response);

    printf("RESPONSE BODY: %s\n", httpResponse.response_body);
    printf("RESPONSE STAT: %d\n", httpResponse.status_code);
    printf("RESPONSE VERS: %s\n", httpResponse.http_ver);
    printf("RESPONSE TYPE: %s\n", httpResponse.connection_type);
    printf("RESPONSE DATE: %s\n", httpResponse.date_time_str);

    /** BUGGY
    int len = strlen(httpResponse.response_body);
    if (len == httpResponse.http_cont_len) {
       printf("PASSED\n");
    } else {
       printf("FAILED: %d %d\n", len, httpResponse.http_cont_len);
    }
    **/

    free(response);

    if (httpResponse.status_code != 200)
    {
        fprintf(stderr, "HTTP FAIL\n");
    }

    return 0;
}

#endif // UNITTEST
