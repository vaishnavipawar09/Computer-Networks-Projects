#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include "include/sans.h"
#include "include/rudpheader.h"

extern struct rudp_sess rudp_skt;

int sans_send_pkt(int socket, const char *buf, int len) 
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

    size_t totalpackets = len / PLD_LEN;
    rudp_packet_t rudp_pkt = { .type = DAT };

    // Iterate over each packet
    for (int i = 0; i <= totalpackets; ++i) 
    {
        size_t curr_seqnum = rudp_skt.seqnumber + i;
        rudp_pkt.seqnum = curr_seqnum;

        size_t payload_offset = i * PLD_LEN;
        size_t pkt_data_len = PLD_LEN;

        // Handle the partially full packet
        if (i == totalpackets) 
        {
            if (len % PLD_LEN == 0) 
            {
                break; 
            }
            pkt_data_len = len % PLD_LEN;
        }

        memcpy(rudp_pkt.payload, buf + payload_offset, pkt_data_len);

        rudp_packet_t ack_pkt;
        while (1) 
        {
            // Send the data packet
            if (sendto(socket, &rudp_pkt, sizeof(rudp_pkt), 0, (struct sockaddr *)&rudp_skt.addr,sizeof(rudp_skt.addr)) < 0) 
            {
                return -1; 
            }

            // Wait for an ACK from the receiver
            socklen_t addr_len = sizeof(rudp_skt.addr);
            int ack_res = recvfrom(socket, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&rudp_skt.addr, &addr_len);

            // Check if ACK is valid and matches the current sequence number
            if (ack_res > 0 && ack_pkt.type == ACK && ack_pkt.seqnum == curr_seqnum) 
            {
                rudp_skt.seqnumber = curr_seqnum + 1;
                break; 
            }
        }
    }

    return len;
}

int sans_recv_pkt(int socket, char *buf, int len) 
{
    if (len < 0) 
    {
        return -1;
    }

    size_t totalpackets = len / PLD_LEN;
    rudp_packet_t pkt_recv;
    rudp_packet_t ack_pkt = { .type = ACK };

    // Iterate over each packet
    for (int i = 0; i <= totalpackets; ++i)
    {
        size_t payload_offset = i * PLD_LEN;
        size_t pkt_data_len = PLD_LEN;
        size_t exp_seqnum = rudp_skt.seqnumber + i;

        // Handle the partially full packet
        if (i == totalpackets) 
        {
            if (len % PLD_LEN == 0) 
            {
                break;
            }
            pkt_data_len = len % PLD_LEN;
        }

        while (1) 
        {
            socklen_t addr_len = sizeof(rudp_skt.addr);
            int bytes_recv = recvfrom(socket, &pkt_recv, sizeof(pkt_recv), 0, (struct sockaddr *)&rudp_skt.addr, &addr_len);

            // Check if a valid data packet is received
            if (bytes_recv > 0 && pkt_recv.type == DAT) 
            {
                if (pkt_recv.seqnum == exp_seqnum) 
                {
                    // Correct packet received
                    memcpy(buf + payload_offset, pkt_recv.payload, pkt_data_len);

                    // Send ACK
                    ack_pkt.seqnum = exp_seqnum;
                    if (sendto(socket, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&rudp_skt.addr, sizeof(rudp_skt.addr)) < 0) 
                    {
                        return -1;
                    }

                    rudp_skt.seqnumber = exp_seqnum + 1;
                    break; 
                }

                // If an out-of-order packet is received, send the final ACK
                if (exp_seqnum > 0) 
                {
                    ack_pkt.seqnum = exp_seqnum - 1;
                    if (sendto(socket, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&rudp_skt.addr,sizeof(rudp_skt.addr)) < 0) 
                    {
                        return -1;
                    }
                }
            }
        }
    }

    return len;
}
