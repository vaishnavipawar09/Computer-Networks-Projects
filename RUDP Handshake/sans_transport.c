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
  };

  int bytes_send = sendto(socket, buf, sizeof(buf) + len, 0,
                          (struct sockaddr *)&rudp_skt.addr,
                          sizeof(rudp_skt.addr));
  return bytes_send;
}

int sans_recv_pkt(int socket, char *buf, int len)
{
  if (len < 0)
  {
    return -1;
  }
  socklen_t len_ofaddr = sizeof(rudp_skt.addr);
  int bytes_recv = recvfrom(socket, buf, sizeof(buf) + len, 0,
                            (struct sockaddr *)&rudp_skt.addr,
                            &len_ofaddr);

  return bytes_recv;
}