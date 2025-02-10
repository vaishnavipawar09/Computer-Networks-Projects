#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "include/sans.h"
#define SIZEOF_BUFFER 1024

// Connect to the SMTP server
int smtp_connect(const char *host, int port)
{
    int smtp_session = sans_connect(host, port, IPPROTO_TCP);
    if (smtp_session < 0)
    {
        printf("Failed connection to %s on port %d.\n", host, port);
        return -1;
    }
    return smtp_session;
}

// Check SMTP Response Code
int check_ResponseCode(const char *response)
{
    return strncmp(response, "250", 3) == 0 ? 0 : -1;
}

// Send an SMTP message
int send_SmtpMsg(int smtp_session, const char *msg)
{
    return sans_send_pkt(smtp_session, msg, strlen(msg));
}

// Receive an SMTP response
int receive_SmtpResponse(int smtp_session, char *smtp_data, int datasize)
{
    int res = sans_recv_pkt(smtp_session, smtp_data, datasize);
    if (res < 0)
    {
        printf("Receive response failed.\n");
        return -1;
    }
    smtp_data[res] = '\0';
    return 0;
}

// Checks for the initial "220" SMTP greeting.
int check_SmtpGreeting(int smtp_session)
{
    char smtp_data[SIZEOF_BUFFER];
    if (receive_SmtpResponse(smtp_session, smtp_data, sizeof(smtp_data)) < 0)
        return -1;

    if (strncmp(smtp_data, "220", 3) != 0)
        return -1;
    return 0;
}

// Send the HELO command
int send_HeloMsg(int smtp_session, const char *host)
{
    char smtp_data[SIZEOF_BUFFER];
    snprintf(smtp_data, sizeof(smtp_data), "HELO %s\r\n", host);
    if (send_SmtpMsg(smtp_session, smtp_data) < 0)
        return -1;

    if (receive_SmtpResponse(smtp_session, smtp_data, sizeof(smtp_data)) < 0)
        return -1;
    return check_ResponseCode(smtp_data);
}

// Send the MAIL FROM command
int send_MailfromMsg(int smtp_session, const char *smtp_emailaddr)
{
    char smtp_data[SIZEOF_BUFFER];
    snprintf(smtp_data, sizeof(smtp_data), "MAIL FROM: <%s>\r\n", smtp_emailaddr);
    if (send_SmtpMsg(smtp_session, smtp_data) < 0)
        return -1;

    if (receive_SmtpResponse(smtp_session, smtp_data, sizeof(smtp_data)) < 0)
        return -1;
    return check_ResponseCode(smtp_data);
}

// Send the RCPT TO command
int send_RcptToMsg(int smtp_session, const char *smtp_emailaddr)
{
    char smtp_data[SIZEOF_BUFFER];
    snprintf(smtp_data, sizeof(smtp_data), "RCPT TO: <%s>\r\n", smtp_emailaddr);
    if (send_SmtpMsg(smtp_session, smtp_data) < 0)
        return -1;

    if (receive_SmtpResponse(smtp_session, smtp_data, sizeof(smtp_data)) < 0)
        return -1;
    return check_ResponseCode(smtp_data);
}

// Send the DATA command
int send_DataMsg(int smtp_session)
{
    char smtp_data[SIZEOF_BUFFER];
    snprintf(smtp_data, sizeof(smtp_data), "DATA\r\n");
    if (send_SmtpMsg(smtp_session, smtp_data) < 0)
        return -1;

    if (receive_SmtpResponse(smtp_session, smtp_data, sizeof(smtp_data)) < 0)
        return -1;
    return strncmp(smtp_data, "354", 3) == 0 ? 0 : -1;
}

// Send the content of the email from the file
int send_content(int smtp_session, const char *PathTofile)
{
    FILE *file = fopen(PathTofile, "r");
    if (!file)
    {
        printf("Failed to open the file: %s\n", PathTofile);
        return -1;
    }

    char smtp_data[SIZEOF_BUFFER];
    while (fgets(smtp_data, sizeof(smtp_data), file))
    {
        if (send_SmtpMsg(smtp_session, smtp_data) < 0)
        {
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}

// Send the email termination string
int send_DataTermination(int smtp_session)
{
    const char *termination = "\r\n.\r\n";
    if (send_SmtpMsg(smtp_session, termination) < 0)
        return -1;

    char smtp_data[SIZEOF_BUFFER];
    if (receive_SmtpResponse(smtp_session, smtp_data, sizeof(smtp_data)) < 0)
        return -1;

    printf("%s\n", smtp_data);
    return 0;
}

// Send the QUIT command
int send_SmtpQuit(int smtp_session)
{
    char smtp_data[SIZEOF_BUFFER];
    snprintf(smtp_data, sizeof(smtp_data), "QUIT\r\n");
    if (send_SmtpMsg(smtp_session, smtp_data) < 0)
        return -1;

    if (receive_SmtpResponse(smtp_session, smtp_data, sizeof(smtp_data)) < 0)
        return -1;
    return 0;
}

int smtp_agent(const char *host, int port)
{
    char smtp_emailaddr[SIZEOF_BUFFER];
    char PathTofile[SIZEOF_BUFFER];
    scanf("%s", smtp_emailaddr);
    scanf("%s", PathTofile);

    int smtp_session = smtp_connect(host, port);
    if (smtp_session < 0)
        return -1;

    if (check_SmtpGreeting(smtp_session) < 0)
    {
        sans_disconnect(smtp_session);
        return -1;
    }

    if (send_HeloMsg(smtp_session, host) < 0)
    {
        sans_disconnect(smtp_session);
        return -1;
    }

    if (send_MailfromMsg(smtp_session, smtp_emailaddr) < 0)
    {
        sans_disconnect(smtp_session);
        return -1;
    }

    if (send_RcptToMsg(smtp_session, smtp_emailaddr) < 0)
    {
        sans_disconnect(smtp_session);
        return -1;
    }

    if (send_DataMsg(smtp_session) < 0)
    {
        sans_disconnect(smtp_session);
        return -1;
    }

    if (send_content(smtp_session, PathTofile) < 0)
    {
        sans_disconnect(smtp_session);
        return -1;
    }

    if (send_DataTermination(smtp_session) < 0)
    {
        sans_disconnect(smtp_session);
        return -1;
    }

    if (send_SmtpQuit(smtp_session) < 0)
    {
        sans_disconnect(smtp_session);
        return -1;
    }

    sans_disconnect(smtp_session);
    return 0;
}