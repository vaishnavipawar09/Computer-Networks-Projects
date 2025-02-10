#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define MAX_SOCKETS 10
#define PKT_LEN 1400
#define CONNECTION_QUEUE 5

int sans_connect(const char *host, int port, int protocol)
{
    // Protocol check
    if (protocol != IPPROTO_TCP)
    {
        return -1;
    }

    // Create an addrinfo structure to hold connection configurations
    struct addrinfo sock_config, *sock_addr_res;
    memset(&sock_config, 0, sizeof(sock_config));
    sock_config.ai_family = AF_UNSPEC;
    sock_config.ai_socktype = SOCK_STREAM;
    sock_config.ai_protocol = IPPROTO_TCP;

    // Conversion of port number to a valid string
    char str_port[6];
    snprintf(str_port, sizeof(str_port), "%d", port);

    // Returns error if resolution fails
    int check = getaddrinfo(host, str_port, &sock_config, &sock_addr_res);
    if (check != 0)
    {
        return -2;
    }

    struct addrinfo *curr_skaddr = sock_addr_res;
    int tcp_socketfd = -1;
    for (curr_skaddr = sock_addr_res; curr_skaddr != NULL; curr_skaddr = curr_skaddr->ai_next)
    {
        // Create a socket using the current addrinfo configurations.
        tcp_socketfd = socket(curr_skaddr->ai_family, curr_skaddr->ai_socktype,
                              curr_skaddr->ai_protocol);
        if (tcp_socketfd == -1)
        {
            continue;
        }

        // Connect to host using current socket
        if (connect(tcp_socketfd, curr_skaddr->ai_addr, curr_skaddr->ai_addrlen) == 0)
        {
            break;
        }

        close(tcp_socketfd);
        tcp_socketfd = -1;
    }

    return tcp_socketfd;
}

int sans_accept(const char *addr, int port, int protocol)
{
    // Protocol check
    if (protocol != IPPROTO_TCP)
    {
        return -1;
    }

    // Create an addrinfo structure to hold connection configurations
    struct addrinfo *sock_addr_res, sock_config;
    memset(&sock_config, 0, sizeof(sock_config));
    sock_config.ai_family = AF_UNSPEC;
    sock_config.ai_socktype = SOCK_STREAM;
    sock_config.ai_protocol = IPPROTO_TCP;

    // Conversion of port number to a valid string
    char str_port[6];
    snprintf(str_port, sizeof(str_port), "%d", port);

    // Returns error if resolution fails
    int check = getaddrinfo(addr, str_port, &sock_config, &sock_addr_res);
    if (check != 0)
    {
        return -2;
    }

    struct addrinfo *curr_skaddr = sock_addr_res;
    int tcp_socketfd = -1;
    for (curr_skaddr = sock_addr_res; curr_skaddr != NULL; curr_skaddr = curr_skaddr->ai_next)
    {
        // Create a socket using the current addrinfo configurations.
        tcp_socketfd = socket(curr_skaddr->ai_family, curr_skaddr->ai_socktype, curr_skaddr->ai_protocol);
        if (tcp_socketfd == -1)
        {
            continue;
        }

        // Bind the socket to the address and wait for incoming connections.
        if (bind(tcp_socketfd, curr_skaddr->ai_addr, curr_skaddr->ai_addrlen) == 0)
        {
            if (listen(tcp_socketfd, CONNECTION_QUEUE) == 0)
            {
                break;
            }
        }

        close(tcp_socketfd);
        tcp_socketfd = -1;
    }

    // Return an error if no socket could be bound or listened to.
    if (tcp_socketfd == -1)
        return -2;

    // Accept incoming connections and return connected socket descriptors.
    return accept(tcp_socketfd, NULL, NULL);
}

int sans_disconnect(int socket)
{
    // Check whether the socket was successfully closed or not.
    if (!close(socket))
    {
        return -1;
    }
    return 0;
}