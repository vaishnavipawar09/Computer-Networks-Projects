#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "include/sans.h"
#include "include/rudpheader.h"

#define CONNECTION_QUEUE 5
#define RETRY_ATTEMPT_LIMIT 3
#define ITERATION_CAP 100

// Structure to store RUDP session state
struct rudp_sess rudp_skt;

// Configures the timeout settings for the given socket (20ms timeout)
static void configure_timeout(int rudp_socketfd)
{
    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = 20000};
    setsockopt(rudp_socketfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}
int sans_disconnect(int socket);

int sans_connect(const char *host, int port, int protocol)
{
    // Protocol check
    struct addrinfo sock_config, *sock_addr_res;

    if (protocol == IPPROTO_TCP)
    {
        memset(&sock_config, 0, sizeof(sock_config));
        sock_config.ai_family = AF_UNSPEC;
        sock_config.ai_socktype = SOCK_STREAM;
        sock_config.ai_protocol = IPPROTO_TCP;

        char str_port[6];
        snprintf(str_port, sizeof(str_port), "%d", port);

        // Returns error if resolution fails
        int check = getaddrinfo(host, str_port, &sock_config, &sock_addr_res);
        if (check != 0)
        {
            return -2;
        }

        int tcp_socketfd = -1;
        for (struct addrinfo *curr_skaddr = sock_addr_res; curr_skaddr != NULL; curr_skaddr = curr_skaddr->ai_next)
        {
            tcp_socketfd = socket(curr_skaddr->ai_family, curr_skaddr->ai_socktype, curr_skaddr->ai_protocol);
            if (tcp_socketfd == -1)
                continue;

            if (connect(tcp_socketfd, curr_skaddr->ai_addr, curr_skaddr->ai_addrlen) == 0)
            {
                return tcp_socketfd;
            }

            close(tcp_socketfd);
        }
        return -1;
    }
    // Check for RUDP protocol
    else if (protocol == IPPROTO_RUDP)
    {
        memset(&sock_config, 0, sizeof(sock_config));
        sock_config.ai_family = AF_INET;
        sock_config.ai_socktype = SOCK_DGRAM;
        sock_config.ai_protocol = IPPROTO_UDP;

        char str_port[6];
        snprintf(str_port, sizeof(str_port), "%d", port);

        // Returns error if resolution fails
        int check = getaddrinfo(host, str_port, &sock_config, &sock_addr_res);
        if (check != 0)
        {
            return -2;
        }

        int rudp_socketfd = -1;
        for (struct addrinfo *curr_skaddr = sock_addr_res; curr_skaddr != NULL; curr_skaddr = curr_skaddr->ai_next)
        {
            rudp_socketfd = socket(curr_skaddr->ai_family, curr_skaddr->ai_socktype, curr_skaddr->ai_protocol);
            if (rudp_socketfd == -1)
                continue;
            
            configure_timeout(rudp_socketfd); // Set timeout for RUDP

            struct sockaddr_in host_addr;
            memset(&host_addr, 0, sizeof(host_addr));
            rudp_packet_t syn_packet = {.type = SYN, .seqnum = 0};
            rudp_packet_t ack_packet = {0};
            int cnt = 0;

            // Retry loop for sending SYN packet and waiting for response
            while (cnt < RETRY_ATTEMPT_LIMIT)
            {
                printf("Sends SYN packets\n");
                sendto(rudp_socketfd, &syn_packet, sizeof(syn_packet), 0, (struct sockaddr *)&host_addr, sizeof(host_addr));

                socklen_t addr_len = sizeof(host_addr);
                int recv_bytes = recvfrom(rudp_socketfd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&host_addr, &addr_len);

                // Check if a valid SYN+ACK packet was received
                if (recv_bytes > 0 && ack_packet.type == (SYN | ACK))
                {
                    rudp_packet_t ack_response = {.type = ACK, .seqnum = ack_packet.seqnum};
                    sendto(rudp_socketfd, &ack_response, sizeof(ack_response), 0, (struct sockaddr *)&host_addr, sizeof(host_addr));
                    printf("Received the SYN+ACK packet and sent ACK. n");

                    // Store socket and address in session structure
                    rudp_skt.rudp_socketfd = rudp_socketfd;
                    rudp_skt.addr = host_addr;
                    return rudp_socketfd;
                }
                else
                {
                    printf("Encountered timeout / incorrect packet, will retransmit SYN\n");
                }
                cnt++;
            }
            close(rudp_socketfd);
        }
        return -1;
    }
    return -1;
}

int sans_accept(const char *addr, int port, int protocol)
{
    // Protocol check for TCP
    if (protocol == IPPROTO_TCP)
    {
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
        int check = getaddrinfo(addr, str_port, &sock_config, &sock_addr_res);
        if (check != 0)
        {
            return -2;
        }

        int tcp_socketfd = -1;
        for (struct addrinfo *curr_skaddr = sock_addr_res; curr_skaddr != NULL; curr_skaddr = curr_skaddr->ai_next)
        {
            tcp_socketfd = socket(curr_skaddr->ai_family, curr_skaddr->ai_socktype, curr_skaddr->ai_protocol);
            if (tcp_socketfd == -1)
                continue;

            // Bind the socket to the address and wait for incoming connections.
            if (bind(tcp_socketfd, curr_skaddr->ai_addr, curr_skaddr->ai_addrlen) == 0)
            {
                if (listen(tcp_socketfd, CONNECTION_QUEUE) == 0)
                {
                    break;
                }
            }

            close(tcp_socketfd);
        }
        return -1;
    }
    // Protocol check for RUDP
    else if (protocol == IPPROTO_RUDP)
    {
        int rudp_socketfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (rudp_socketfd == -1)
            return -1;

        configure_timeout(rudp_socketfd); // Set timeout for RUDP

        struct sockaddr_in host_addr;
        memset(&host_addr, 0, sizeof(host_addr));
        host_addr.sin_family = AF_INET;
        host_addr.sin_addr.s_addr = INADDR_ANY;
        host_addr.sin_port = htons(port);

        // Bind the socket
        if (bind(rudp_socketfd, (struct sockaddr *)&host_addr, sizeof(host_addr)) < 0)
        {
            close(rudp_socketfd);
            return -1;
        }

        struct sockaddr_in client_sktaddr;
        socklen_t client_sktaddr_len = sizeof(client_sktaddr);
        rudp_packet_t syn_packet = {0};
        rudp_packet_t syn_ackpacket = {0};
        rudp_packet_t ack_packet = {0};
        int cnt = 0;

        //Wait for SYN
        while (1)
        {
            int recv_bytes = recvfrom(rudp_socketfd, &syn_packet, sizeof(syn_packet), 0,
                                      (struct sockaddr *)&client_sktaddr, &client_sktaddr_len);
            if (recv_bytes > 0 && syn_packet.type == SYN)
            {
                // Prepare and send SYN+ACK in response
                syn_ackpacket.type = SYN | ACK;
                syn_ackpacket.seqnum = syn_packet.seqnum + 1;
                printf("SYN Received & sending SYN+ACK\n");

                while (cnt < RETRY_ATTEMPT_LIMIT)
                {
                    sendto(rudp_socketfd, &syn_ackpacket, sizeof(syn_ackpacket), 0,
                           (struct sockaddr *)&client_sktaddr, client_sktaddr_len);
                    recv_bytes = recvfrom(rudp_socketfd, &ack_packet, sizeof(ack_packet), 0,
                                          (struct sockaddr *)&client_sktaddr, &client_sktaddr_len);

                    // Store socket and address in session structure
                    rudp_skt.rudp_socketfd = rudp_socketfd;
                    rudp_skt.addr = client_sktaddr;

                    if (recv_bytes > 0 && ack_packet.type == ACK)
                    {
                        printf("ACK Received\n");
                        return rudp_socketfd;
                    }
                    else
                    {
                        printf("Encountered timeout / incorrect packet, will retransmit SYN\n");
                    }
                    cnt++;
                }
                close(rudp_socketfd);
                return -1;
            }
            else
            {
                printf("Discard incorrect packets, expected SYN.\n");
                cnt++;
                if (cnt >= RETRY_ATTEMPT_LIMIT)
                {
                    printf("Abort, too many packets.\n");
                    close(rudp_socketfd);
                    return -1;
                }
                continue;
            }
        }
    }

    return -1;
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