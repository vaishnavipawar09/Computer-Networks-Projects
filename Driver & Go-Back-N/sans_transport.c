#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include "include/rudpheader.h"

void enqueue_packet(int sock, rudp_packet_t* pkt, int len);

int sans_send_pkt(int socket, const char* buf, int len) 
{
    if (len < 0) 
    {
        return -1;
    }

    // Configure socket timeout to handle retransmissions
    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = 200000 
    };
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) 
    {
        return -1; 
    }

    // Prepare the packet with a sequence number
    int seqnum = rudp_skt.seqnumber;
    rudp_skt.seqnumber++;

    rudp_packet_t* rudp_pkt = malloc(sizeof(rudp_packet_t) + len);
    rudp_pkt->type = DAT;
    rudp_pkt->seqnum = seqnum;
    memcpy(rudp_pkt->payload, buf, len);

    // Add the packet to the send queue
    enqueue_packet(socket, rudp_pkt, len);

    // Free allocated memory for the packet
    free(rudp_pkt);  

    return len;
}

int sans_recv_pkt(int socket, char* buf, int len) 
{
    if (len < 0) 
    {
        return -1;
    }

    struct sockaddr* address = (struct sockaddr*)&rudp_skt.addr;
    socklen_t addr_len = sizeof(rudp_skt.addr);

    // Allocate memory for the response packet
    rudp_packet_t* rudp_response = malloc(sizeof(rudp_packet_t) + len);
    int bytes_recv = recvfrom(socket, rudp_response, sizeof(rudp_packet_t) + len, 0, address, &addr_len);

    while (bytes_recv < 0 || rudp_response->type != DAT) {
        bytes_recv = recvfrom(socket, rudp_response, sizeof(rudp_packet_t) + len, 0, address, &addr_len);
    }

    // Send ACK
    rudp_packet_t ack_pkt;
    ack_pkt.type = ACK;
    ack_pkt.seqnum = rudp_response->seqnum;
    sendto(socket, &ack_pkt, sizeof(rudp_packet_t), 0, address, addr_len);

    // Extract the payload from the received packet
    int pyld_len = bytes_recv - sizeof(rudp_packet_t);
    memcpy(buf, rudp_response->payload, pyld_len);
    buf[pyld_len] = '\0';

    // Free allocated memory for the response packet
    free(rudp_response);  

    return pyld_len;
}
