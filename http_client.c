#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include "include/sans.h"
#define SIZEOF_BUFFER 1024

void get_response(int socketID)
{
  char Responsebuffer[SIZEOF_BUFFER];
  int ReceivedBytes;

  do
  {
    ReceivedBytes = sans_recv_pkt(socketID, Responsebuffer, sizeof(Responsebuffer) - 1);
    if (ReceivedBytes > 0)
    {
      Responsebuffer[ReceivedBytes] = '\0';
      printf("%s", Responsebuffer);
    }
  } while (ReceivedBytes > 0);
}

int http_client(const char *host, int port)
{
  char method[10];
  char path[256];
  char httprequest[SIZEOF_BUFFER];

  // Get the User Input
  scanf("%9s", method);
  if (strcmp(method, "GET") != 0)
  {
    fprintf(stderr, "Invalid method: ONLY GET method request are allowed\n");
    return -1;
  }

  scanf("%255s", path);
  char path_request[257];
  snprintf(path_request, sizeof(path_request), "/%s", path);

  // Prepare HTTP_Request before connection
  snprintf(httprequest, sizeof(httprequest),
           "%s %s HTTP/1.1\r\n"
           "Host: %s:%d\r\n"
           "User-Agent: sans/1.0\r\n"
           "Cache-Control: no-cache\r\n"
           "Connection: close\r\n"
           "Accept: */*\r\n\r\n",
           method, path_request, host, port);

  // Establishing the connection
  int socketID = sans_connect(host, port, IPPROTO_TCP);
  if (socketID < 0)
  {
    fprintf(stderr, "Connection Error, can't connect to server\n");
    return -1;
  }

  // Sending the request
  if (sans_send_pkt(socketID, httprequest, strlen(httprequest)) < 0)
  {
    fprintf(stderr, "Request error, failed to send request\n");
    sans_disconnect(socketID);
    return -1;
  }

  // Get the server response & close the connection
  get_response(socketID);
  sans_disconnect(socketID);
  return 0;
}