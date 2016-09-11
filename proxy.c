/*
 * Tiny TCP proxy server
 *
 * Author: Krzysztof Kliś <krzysztof.klis@gmail.com>
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

typedef struct {
  int length;
  int mem_size;
  char * data;
} char_list;

int create_socket(int port);
int create_connection();
void sigchld_handler(int signal);
void sigterm_handler(int signal);
void server_loop();
void handle_client(int client_sock, struct sockaddr_in client_addr);
void forward_data(int source_sock, int destination_sock);
void forward_char_list(char_list *list, int destination_sock);
int parse_options(int argc, char *argv[]);

int server_sock, client_sock, remote_sock, remote_port = 0;
bool foreground = FALSE;

/* Program start */
int main(int argc, char *argv[]) {
    int c, local_port;
    pid_t pid;

    local_port = parse_options(argc, argv);

    if (local_port < 0) {
        printf("Syntax: %s -l local_port -p remote_port [-f (stay in foreground)]\n", argv[0]);
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

    while ((c = getopt(argc, argv, "l:h:p:i:o:f")) != -1) {
        switch(c) {
            case 'l':
                local_port = atoi(optarg);
                break;
            case 'p':
                remote_port = atoi(optarg);
                break;
            case 'f':
                foreground = TRUE;
        }
    }

    if (local_port && remote_port) {
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

    while (TRUE) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addrlen);
        if (fork() == 0){
			//for the child process: accept the burden of your parent and die
            close(server_sock);
            handle_client(client_sock, client_addr);
            exit(0);
        }
		//for the parent: just chill yo
        close(client_sock);
    }
}

/* dynamically sized char buffer */
char_list char_list_init(){
	char_list list;
	list.length = 0;
	list.mem_size = 2;
	list.data = NULL;
	return list;
}
void char_list_add(char_list *list, char * new, int new_len){
	while(list->mem_size - list->length < new_len){
		list->mem_size *= 2;
	}
	list->data = realloc(list->data, list->mem_size);
	strncpy(&list->data[list->length], new, new_len);
	list->length += new_len;
}

void print_char_list(char_list list){
	for (int i = 0; i < list.length; i++) {
		printf("%c", list.data[i]);
	}
	printf("\n");
}

/* Handle client connection */
void handle_client(int client_sock, struct sockaddr_in client_addr)
{
	//find out where to connect
	char buffer[BUF_SIZE];
	char host_addr[BUF_SIZE];
	char service[20];
	int n, remote_sock;
	char_list request = char_list_init();
	//read everything out of the client socket
	while ((n = recv(client_sock, buffer, BUF_SIZE, 0)) > 0) { // read data from input socket
		char_list_add(&request, buffer, n);
		if(strncmp("GET", buffer, 3) == 0){
			//get address
			sscanf(buffer, "GET %[^:]://%[^/] %*s\n", service, host_addr);
			remote_sock = create_connection(service, host_addr);
		}else{
			printf("Couldn't parse request: %s\n", buffer);
		}
		if(n < BUF_SIZE){
			break;
		}
	}
	print_char_list(request);
	//quit if we don't have a socket
	if(remote_sock < 0){
		printf("error creating socket\n");
		return;
	}
	//fork a child to handle client -> remote (upload)
    if (fork() == 0) {
        forward_char_list(&request, remote_sock);
        exit(0);
    }
	//fork a child to handle remote -> client (download)
    if (fork() == 0) {
        forward_data(remote_sock, client_sock);
        exit(0);
    }
	//clean up
    close(remote_sock);
    close(client_sock);
}

/* Forward data between sockets */
void forward_data(int source_sock, int destination_sock) {
	char buffer[BUF_SIZE];
	int n;
	char_list list = char_list_init();

	while ((n = recv(source_sock, buffer, BUF_SIZE, 0)) > 0) { // read data from input socket
		printf("receiving %d\n", n);
		char_list_add(&list, buffer, n);
		send(destination_sock, buffer, n, 0); // send data to output socket
	}

	printf("done receiving\n");
	print_char_list(list);

	shutdown(destination_sock, SHUT_RDWR); // stop other processes from using socket
	close(destination_sock);

	shutdown(source_sock, SHUT_RDWR); // stop other processes from using socket
	close(source_sock);
}

/* Forward a char list to a socket */
void forward_char_list(char_list *list, int destination_sock) {
	int chunk = 0, sent, chunk_size;
	int left = list->length;

	while (chunk < list->length) { // read data from input socket
		//figure out how much to send
		if(left < BUF_SIZE){
			chunk_size = left;
		}else{
			chunk_size = BUF_SIZE;
		}
		//take a potato chip...and eat it!
		sent = send(destination_sock, &list->data[chunk], left, 0); // send data to output socket
		printf("send %d bytes\n", sent);
		chunk += BUF_SIZE;
		left -= BUF_SIZE;
	}

	//shutdown(destination_sock, SHUT_RDWR); // stop other processes from using socket
	//close(destination_sock);
}

/* Create client connection */
int create_connection(char *service, char *remote_host) {
    struct sockaddr server_ip;
    struct hostent *server;
	struct addrinfo hints, *server_info, *p;
    int sock, rv;
	//init some hints about our address
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	// --- perform hostname lookup
	if ((rv = getaddrinfo(remote_host, service, &hints, &server_info)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}
	// loop through all the results and connect to the first we can
	for(p = server_info; p != NULL; p = p->ai_next) {
		if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("socket");
			continue;
		}
		if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
			perror("connect");
			close(sock);
			continue;
		}
		break; // if we get here, we must have connected successfully
	}
	if (p == NULL) {
		// looped off the end of the list with no connection
		fprintf(stderr, "failed to connect\n");
		exit(2);
	}
	freeaddrinfo(server_info); // all done with this structure
	return sock;
}
