
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "include/sans.h"
#define SIZEOF_BUFFER 1024

// Handle Error by sending 404_Error
void handle_error(int RemoteClient)
{
  const char *httpResponse =
      "HTTP/1.1 404 Not Found\r\n"
      "Content-Length: 13\r\n"
      "Content-Type: text/html; charset=utf-8\r\n\r\n"
      "404 Not Found";
  sans_send_pkt(RemoteClient, httpResponse, strlen(httpResponse));
}

// Calculate size of the Requeste_File
int cal_filesize(const char *requested_File)
{
  struct stat file_stat;
  if (stat(requested_File, &file_stat) == 0)
  {
    return file_stat.st_size;
  }
  return -1;
}

// Check if File_Exists
int file_check(const char *requested_File)
{
  struct stat file_stat;
  return stat(requested_File, &file_stat) == 0;
}

// Send the HTTP response headers
void send_httpResponse_headers(int RemoteClient, int file_length)
{
  char httpResponse[SIZEOF_BUFFER];
  snprintf(httpResponse, sizeof(httpResponse),
           "HTTP/1.1 200 OK\r\n"
           "Content-Length: %d\r\n"
           "Content-Type: text/html; charset=utf-8\r\n\r\n",
           file_length);

  sans_send_pkt(RemoteClient, httpResponse, strlen(httpResponse));
}

// Send File Data to Client
void sendFileData(FILE *file, int RemoteClient)
{
  char fileBuffer[SIZEOF_BUFFER];
  size_t data_read;
  while ((data_read = fread(fileBuffer, 1, sizeof(fileBuffer), file)) > 0)
  {
    sans_send_pkt(RemoteClient, fileBuffer, data_read);
  }
}

// Managing the HTTP Request
void manage_httpReq(int RemoteClient)
{
  char input_buffer[SIZEOF_BUFFER];
  char method[8], path[256], http_version[16];

  int BytesReceived = sans_recv_pkt(RemoteClient, input_buffer, SIZEOF_BUFFER);
  if (BytesReceived <= 0)
  {
    return;
  }

  sscanf(input_buffer, "%s %s %s", method, path, http_version);

  // Check for GET method
  if (strcmp(method, "GET") != 0)
  {
    handle_error(RemoteClient);
    sans_disconnect(RemoteClient);
    return;
  }

  // Remove leading '/' from path
  if (path[0] == '/')
  {
    memmove(path, path + 1, strlen(path));
  }

  if (!file_check(path))
  {
    handle_error(RemoteClient);
    sans_disconnect(RemoteClient);
    return;
  }

  // Calculate the File Length
  int file_length = cal_filesize(path);
  if (file_length < 0)
  {
    handle_error(RemoteClient);
    sans_disconnect(RemoteClient);
    return;
  }

  // Open_File
  FILE *file = fopen(path, "rb");
  if (!file)
  {
    handle_error(RemoteClient);
    sans_disconnect(RemoteClient);
    return;
  }

  send_httpResponse_headers(RemoteClient, file_length);

  sendFileData(file, RemoteClient);

  // Close file and Disconnect the Client
  fclose(file);
  sans_disconnect(RemoteClient);
}

int http_server(const char *iface, int port)
{
  // Accept a Remote_Client connection
  int RemoteClient = sans_accept(iface, port, 0);
  if (RemoteClient < 0)
  {
    perror("Connection Failed");
    return -1;
  }
  manage_httpReq(RemoteClient);
  return 0;
}
