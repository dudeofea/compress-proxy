/*
 * HTTP Compress Proxy, modified from:
 *
 *	Micro Proxy: http://acme.com/software/micro_proxy/
 *	Tiny TCP proxy server: Krzysztof Kliś <krzysztof.klis@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version with the following modification:
 *
 * As a special exception, the copyright holders of this library give you
 * permission to link this library with independent modules to produce an
 * executable, regardless of the license terms of these independent modules,
 * and to copy and distribute the resulting executable under terms of your choice,
 * provided that you also meet, for each linked independent module, the terms
 * and conditions of the license of that module. An independent module is a
 * module which is not derived from or based on this library. If you modify this
 * library, you may extend this exception to your version of the library, but
 * you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>
#include <netdb.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wait.h>
#include <time.h>

#include "structs.h"

#define SERVER_NAME "compress_proxy"
#define SERVER_URL "https://github.com/dudeofea"
#define PROTOCOL "HTTP/1.0"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define TIMEOUT 30

#define BUF_SIZE 1024

#define READ  0
#define WRITE 1

#define SERVER_SOCKET_ERROR -1
#define SERVER_SETSOCKOPT_ERROR -2
#define SERVER_BIND_ERROR -3
#define SERVER_LISTEN_ERROR -4
#define CLIENT_SOCKET_ERROR -5
#define CLIENT_RESOLVE_ERROR -6
#define CLIENT_CONNECT_ERROR -7
#define CREATE_PIPE_ERROR -8
#define BROKEN_PIPE_ERROR -9
#define SYNTAX_ERROR -10

typedef enum {TRUE = 1, FALSE = 0} bool;

int create_socket(int port);
int create_http_connection(int client_sock, char_list http_request, int* ssl);
void sigchld_handler(int signal);
void sigterm_handler(int signal);
void server_loop();
void handle_client(int client_sock, struct sockaddr_in client_addr);
void forward_data(int source_sock, int destination_sock);
void forward_char_list(char_list list, int destination_sock);
int parse_options(int argc, char *argv[]);
static void send_error(int client_sock, int status, char* title, char* extra_header, char* text);
static void proxy_ssl(char_list request, int client_sock, int remote_sock);

int server_sock, client_sock, remote_sock;
bool foreground = FALSE;

/* Program start */
int main(int argc, char *argv[]) {
    int c, local_port;
    pid_t pid;

    local_port = parse_options(argc, argv);

    if (local_port < 0) {
        printf("Syntax: %s -p local_port [-f (stay in foreground)]\n", argv[0]);
        return local_port;
    }

    if ((server_sock = create_socket(local_port)) < 0) { // start server
        perror("Cannot run server");
        return server_sock;
    }

    signal(SIGCHLD, sigchld_handler); // prevent ended children from becoming zombies
    signal(SIGTERM, sigterm_handler); // handle KILL signal

    if (foreground) {
        server_loop();
    } else {
        switch(pid = fork()) {
            case 0: // deamonized child
                server_loop();
                break;
            case -1: // error
                perror("Cannot daemonize");
                return pid;
            default: // parent
                close(server_sock);
        }
    }

    return EXIT_SUCCESS;
}

/* Parse command line options */
int parse_options(int argc, char *argv[]) {
    int c, local_port = 0;

    while ((c = getopt(argc, argv, "p:f")) != -1) {
        switch(c) {
            case 'p':
                local_port = atoi(optarg);
                break;
            case 'f':
                foreground = TRUE;
        }
    }

    if (local_port) {
        return local_port;
    } else {
        return SYNTAX_ERROR;
    }
}

/* Create server socket */
int create_socket(int port) {
    int server_sock, optval;
    struct sockaddr_in server_addr;

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return SERVER_SOCKET_ERROR;
    }

    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        return SERVER_SETSOCKOPT_ERROR;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        return SERVER_BIND_ERROR;
    }

    if (listen(server_sock, 20) < 0) {
        return SERVER_LISTEN_ERROR;
    }

    return server_sock;
}

/* Handle finished child process */
void sigchld_handler(int signal) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* Handle term signal */
void sigterm_handler(int signal){
    close(client_sock);
    close(server_sock);
    exit(0);
}

/* Main server loop */
void server_loop() {
    struct sockaddr_in client_addr;
    int addrlen = sizeof(client_addr);
	char buffer[BUF_SIZE];
	char host_addr[BUF_SIZE];
	char service[20];
	int n, remote_sock, ssl;
	char_list request;

    while (TRUE) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addrlen);
		//handle the incoming request
		if(fork() == 0){
			request = char_list_init();
			//read everything out of the client socket
			while ((n = recv(client_sock, buffer, BUF_SIZE, 0)) > 0) {
				//add to request char list
				char_list_add(&request, buffer, n);
				//quit once we got all data
				if(n < BUF_SIZE){
					break;
				}
			}
			//create a connection to remote host
			remote_sock = create_http_connection(client_sock, request, &ssl);
			//quit if we don't have a socket
			if(remote_sock < 0){
				printf("error creating socket\n");
				return;
			}
			//handle an ssl request
			if(ssl){
				proxy_ssl(request, client_sock, remote_sock);
			//handle a regular request
			}else{
				alarm(TIMEOUT);
				forward_char_list(request, remote_sock);		//upload
				forward_data(remote_sock, client_sock);			//download
			}
			close(server_sock);
			close(remote_sock);
			close(client_sock);
			exit(0);
		}
	    close(client_sock);
	}
}

//Proxy connections to an HTTPS connection, by MITM'ing our browser
static void proxy_ssl(char_list request, int client_sock, int remote_sock){
	//first return an OK to browser
	char* ok = "HTTP/1.0 200 Connection established\r\n\r\n";
	if(send(client_sock, ok, strlen(ok), 0) == 0){
		//TODO: handle send error
		printf("nothing sent\n");
	}
	//get response from browser
	int n;
	char buffer[BUF_SIZE];
	char_list response = char_list_init();
	while ((n = recv(client_sock, buffer, BUF_SIZE, 0)) > 0) {
		char_list_add(&request, buffer, n);
		if(n < BUF_SIZE){
			break;
		}
	}
	printf("response: ");
	char_list_print(response);
}

/* Forward data between sockets */
void forward_data(int source_sock, int destination_sock) {
	char buffer[BUF_SIZE];
	int n;
	char_list list = char_list_init();

	while ((n = recv(source_sock, buffer, BUF_SIZE, 0)) > 0) { // read data from input socket
		//printf("receiving %d bytes\n", n);
		char_list_add(&list, buffer, n);
		send(destination_sock, buffer, n, 0); // send data to output socket
	}

	printf("received %d bytes\n", list.length);
	char_list_print(list);

	shutdown(destination_sock, SHUT_RDWR); // stop other processes from using socket
	close(destination_sock);

	shutdown(source_sock, SHUT_RDWR); // stop other processes from using socket
	close(source_sock);
}

/* Forward a char list to a socket */
void forward_char_list(char_list list, int destination_sock) {
	int chunk = 0, sent, chunk_size;
	int left = list.length;

	while (chunk < list.length) { // read data from input socket
		//figure out how much to send
		if(left < BUF_SIZE){
			chunk_size = left;
		}else{
			chunk_size = BUF_SIZE;
		}
		//take a potato chip...and eat it!
		sent = send(destination_sock, &list.data[chunk], left, 0); // send data to output socket
		printf("sent %d bytes\n", sent);
		chunk += BUF_SIZE;
		left -= BUF_SIZE;
	}
}

//trim whitespace on a string
static void trim(char* line){
    int l;
    l = strlen( line );
    while ( line[l-1] == '\n' || line[l-1] == '\r' )
	line[--l] = '\0';
}

#if defined(AF_INET6) && defined(IN6_IS_ADDR_V4MAPPED)
#define USE_IPV6
#endif
//Open a connection (socket) to a given hostname and port
static int create_connection(int client_sock, char* hostname, unsigned short port )
    {
#ifdef USE_IPV6
    struct addrinfo hints;
    char portstr[10];
    int gaierr;
    struct addrinfo* ai;
    struct addrinfo* ai2;
    struct addrinfo* aiv4;
    struct addrinfo* aiv6;
    struct sockaddr_in6 sa_in;
#else /* USE_IPV6 */
    struct hostent *he;
    struct sockaddr_in sa_in;
#endif /* USE_IPV6 */
    int sa_len, sock_family, sock_type, sock_protocol;
    int sockfd;

    (void) memset( (void*) &sa_in, 0, sizeof(sa_in) );

#ifdef USE_IPV6

    (void) memset( &hints, 0, sizeof(hints) );
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    (void) snprintf( portstr, sizeof(portstr), "%d", (int) port );
    if ( (gaierr = getaddrinfo( hostname, portstr, &hints, &ai )) != 0 )
	send_error(client_sock, 404, "Not Found", (char*) 0, "Unknown host." );

    /* Find the first IPv4 and IPv6 entries. */
    aiv4 = (struct addrinfo*) 0;
    aiv6 = (struct addrinfo*) 0;
    for ( ai2 = ai; ai2 != (struct addrinfo*) 0; ai2 = ai2->ai_next )
	{
	switch ( ai2->ai_family )
	    {
	    case AF_INET:
	    if ( aiv4 == (struct addrinfo*) 0 )
		aiv4 = ai2;
	    break;
	    case AF_INET6:
	    if ( aiv6 == (struct addrinfo*) 0 )
		aiv6 = ai2;
	    break;
	    }
	}

    /* If there's an IPv4 address, use that, otherwise try IPv6. */
    if ( aiv4 != (struct addrinfo*) 0 )
	{
	if ( sizeof(sa_in) < aiv4->ai_addrlen )
	    {
	    (void) fprintf(
		stderr, "%s - sockaddr too small (%lu < %lu)\n",
		hostname, (unsigned long) sizeof(sa_in),
		(unsigned long) aiv4->ai_addrlen );
	    exit( 1 );
	    }
	sock_family = aiv4->ai_family;
	sock_type = aiv4->ai_socktype;
	sock_protocol = aiv4->ai_protocol;
	sa_len = aiv4->ai_addrlen;
	(void) memmove( &sa_in, aiv4->ai_addr, sa_len );
	goto ok;
	}
    if ( aiv6 != (struct addrinfo*) 0 )
	{
	if ( sizeof(sa_in) < aiv6->ai_addrlen )
	    {
	    (void) fprintf(
		stderr, "%s - sockaddr too small (%lu < %lu)\n",
		hostname, (unsigned long) sizeof(sa_in),
		(unsigned long) aiv6->ai_addrlen );
	    exit( 1 );
	    }
	sock_family = aiv6->ai_family;
	sock_type = aiv6->ai_socktype;
	sock_protocol = aiv6->ai_protocol;
	sa_len = aiv6->ai_addrlen;
	(void) memmove( &sa_in, aiv6->ai_addr, sa_len );
	goto ok;
	}

    send_error(client_sock, 404, "Not Found", (char*) 0, "Unknown host." );

    ok:
    freeaddrinfo( ai );

#else /* USE_IPV6 */

    he = gethostbyname( hostname );
    if ( he == (struct hostent*) 0 )
	send_error(client_sock, 404, "Not Found", (char*) 0, "Unknown host." );
    sock_family = sa_in.sin_family = he->h_addrtype;
    sock_type = SOCK_STREAM;
    sock_protocol = 0;
    sa_len = sizeof(sa_in);
    (void) memmove( &sa_in.sin_addr, he->h_addr, he->h_length );
    sa_in.sin_port = htons( port );

#endif /* USE_IPV6 */

    sockfd = socket( sock_family, sock_type, sock_protocol );
    if ( sockfd < 0 )
	send_error(client_sock, 500, "Internal Error", (char*) 0, "Couldn't create socket." );

    if ( connect( sockfd, (struct sockaddr*) &sa_in, sa_len ) < 0 )
	send_error(client_sock, 503, "Service Unavailable", (char*) 0, "Connection refused." );

    return sockfd;
}

//parse an http request and create a connection
int create_http_connection(int client_sock, char_list http_request, int* ssl){
	char method[BUF_SIZE], url[BUF_SIZE], protocol[BUF_SIZE], path[BUF_SIZE], host[BUF_SIZE];
	unsigned short port;
	int iport;
    if (sscanf(http_request.data, "%[^ ] %[^ ] %[^\n]", method, url, protocol) != 3){
		//TODO: handle bad request, invalid # of parameters
	}
	//a regular http connection
	if (strncasecmp(url, "http://", 7) == 0){
		//make sure it's lowercase
		strncpy(url, "http", 4);	/* make sure it's lower case */
		//if port is in url, with a path
		if (sscanf(url, "http://%[^:/]:%d%s", host, &iport, path) == 3){
		    port = (unsigned short) iport;
		//for a regular url, default to 80
		}else if(sscanf(url, "http://%[^/]%s", host, path) == 2){
		    port = 80;
		//if port is in url, without a path
		}else if (sscanf(url, "http://%[^:/]:%d", host, &iport) == 2){
			port = (unsigned short) iport;
		    *path = '\0';
		}else if (sscanf(url, "http://%[^/]", host) == 1){
		    port = 80;
		    *path = '\0';
		}else{
		    send_error(client_sock, 400, "Bad Request", (char*) 0, "Can't parse URL." );
		}
		*ssl = 0;
		printf("ssl: false, %s %s %d\n", method, host, port);
	//an ssl connection
	}else if(strcmp(method, "CONNECT" ) == 0){
		//get the port to connect to
		if (sscanf( url, "%[^:]:%d", host, &iport ) == 2 ){
		    port = (unsigned short) iport;
		//port not in url, default to 443
		}else if (sscanf( url, "%s", host ) == 1){
		    port = 443;
		}else{
			send_error(client_sock, 400, "Bad Request", (char*) 0, "Can't parse URL." );
		}
		*ssl = 1;
		printf("ssl: true, %s %s %d\n", method, host, port);
	}else{
		send_error(client_sock, 400, "Bad Request", (char*) 0, "Unknown URL type." );
	}
	//TODO: add timeout possibly
	return create_connection(client_sock, host, port);
}

static void send_error(int client_sock, int status, char* title, char* extra_header, char* text){
	time_t now;
	char timebuf[128], response[2048];
	int i = 0;
	//send the HTTP headers
	i += sprintf(&response[i], "%s %d %s\r\n", PROTOCOL, status, title );
    i += sprintf(&response[i], "Server: %s\r\n", SERVER_NAME );
    now = time((time_t*) 0);
    strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );
    i += sprintf(&response[i], "Date: %s\r\n", timebuf );
    if ( extra_header != (char*) 0 ){
		i += sprintf(&response[i], "%s\r\n", extra_header );
	}
    i += sprintf(&response[i], "Content-Type: text/html\r\n");
    i += sprintf(&response[i], "Connection: close\r\n" );
    i += sprintf(&response[i], "\r\n" );
	//send the content
    i += sprintf(&response[i], "\
<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n\
<html>\n\
  <head>\n\
    <meta http-equiv=\"Content-type\" content=\"text/html;charset=UTF-8\">\n\
    <title>%d %s</title>\n\
  </head>\n\
  <body bgcolor=\"#cc9999\" text=\"#000000\" link=\"#2020ff\" vlink=\"#4040cc\">\n\
    <h4>%d %s</h4>\n\n",
	status, title, status, title );
    i += sprintf(&response[i], "%s\n\n", text );
    i += sprintf(&response[i], "\
    <hr>\n\
    <address><a href=\"%s\">%s</a></address>\n\
  </body>\n\
</html>\n",
	SERVER_URL, SERVER_NAME);
    //send the data to the client
	send(client_sock, response, i, 0);
    exit(1);
}
