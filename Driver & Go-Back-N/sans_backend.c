#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "include/rudpheader.h"

#define SWND_SIZE 10
#define TIMEOUT_USEC 20000
#define RETRY_ATTEMPT_LIMIT 5

typedef struct {
    int socket;
    int packetlen;
    rudp_packet_t* packet;
} swnd_entry_t;

swnd_entry_t send_window[SWND_SIZE]; /* Ring buffer containing packets awaiting acknowledgment */

static int sent = 0;  
static int count = 0; 
static int head = 0;  
static int rear = 0;  

//Push a packet onto the ring buffer, can be called from the main thread
void enqueue_packet(int sock, rudp_packet_t* pkt, int len) 
{
    rudp_packet_t* rudp_pkt = malloc(sizeof(rudp_packet_t) + len);
    if (!rudp_pkt) 
    {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }

    rudp_pkt->type = pkt->type;
    rudp_pkt->seqnum = pkt->seqnum;
    memcpy(rudp_pkt->payload, pkt->payload, len);

    // Wait until there is space in the buffer
    while (count - sent >= SWND_SIZE);

    swnd_entry_t pkt_entry;
    pkt_entry.socket = sock;
    pkt_entry.packetlen = sizeof(rudp_packet_t) + len;
    pkt_entry.packet = rudp_pkt;

    send_window[rear] = pkt_entry;
    rear = (rear + 1) % SWND_SIZE;
    count++;
}

//Remove a completed packet from the ring buffer, can be called from the backend thread
static void dequeue_packet(void) {
    free(send_window[head].packet);
    sent++;
    head = (head + 1) % SWND_SIZE;
}

/*
 * Asynchronous runner for the sans protocol driver.
 * This function checks packets in the `send_window` and handles
 * reliable transmission in a Stop-and-Wait or Go-Back-N manner.
 */
void* sans_backend(void* unused) 
{
    while (1) 
    {
        if (count - sent > 0) 
        {
            // Send every packet in the send window.
            int i = head;
            while (i != rear) 
            {
                sendto(send_window[i].socket, send_window[i].packet, send_window[i].packetlen, 0,
                       (struct sockaddr*)&rudp_skt.addr, sizeof(rudp_skt.addr));
                fprintf(stderr, "Sent packet with seqnum: %d\n", send_window[i].packet->seqnum);
                i = (i + 1) % SWND_SIZE;
            }

            struct timeval timeout = { .tv_sec = 0, .tv_usec = TIMEOUT_USEC };
            setsockopt(send_window[head].socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

            int attempt = 0;

            // Handle acknowledgments or retransmit on timeout
            while (attempt < RETRY_ATTEMPT_LIMIT && count - sent > 0) 
            {
                i = head;
                int all_pkts_acked = 1;

                while (i != rear) 
                {
                    rudp_packet_t rudp_response;
                    int bytes_recv = recvfrom(send_window[i].socket, &rudp_response, sizeof(rudp_response), 0,
                                                  (struct sockaddr*)&rudp_skt.addr, &(socklen_t){sizeof(rudp_skt.addr)});

                    if (bytes_recv > 0 && (rudp_response.type & ACK) && rudp_response.seqnum == send_window[i].packet->seqnum) 
                    {
                        // Valid acknowledgment, dequeue the packet
                        dequeue_packet();
                        i = (i + 1) % SWND_SIZE;
                    } else if (bytes_recv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) 
                    {
                        // If Timeout occurres, retransmit all unacknowledged packets
                        int j = head;
                        while (j != rear) 
                        {
                            sendto(send_window[j].socket, send_window[j].packet, send_window[j].packetlen, 0,
                                   (struct sockaddr*)&rudp_skt.addr, sizeof(rudp_skt.addr));
                            fprintf(stderr, "Retransmitting packet with seqnum: %d\n", send_window[j].packet->seqnum);
                            j = (j + 1) % SWND_SIZE;
                        }
                        all_pkts_acked = 0;
                        break; 
                    }
                }

                // All packets acknowledged
                if (all_pkts_acked) 
                {
                    break; 
                }

                attempt++;
            }

            if (attempt == RETRY_ATTEMPT_LIMIT) 
            {
                fprintf(stderr, "Max attempts reached for some packets in the send window.\n");
            }
        }
    }

    return NULL;
}
