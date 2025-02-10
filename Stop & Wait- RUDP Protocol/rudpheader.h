#ifndef RUDP_H
#define RUDP_H
#define IPPROTO_RUDP 63
#include <netinet/in.h>

#define DAT 0
#define SYN 1
#define ACK 2
#define FIN 4
#define PLD_LEN 1024

typedef struct
{
  char type;
  int seqnum;
  char payload[PLD_LEN];
} rudp_packet_t;

struct rudp_sess
{
  int rudp_socketfd;
  struct sockaddr_in addr;
  size_t seqnumber;
};

extern struct rudp_sess rudp_skt;

#endif // RUDP_H
