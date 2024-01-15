/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT_DEFAULT_80 "80"  // the port users will be connecting to, changed to 80

#define BACKLOG 10	 // how many pending connections queue will hold

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]) //to receive port argument
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(1);
	}

	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			
			char buf[1024];
			int numbytes;

			// Read request from client
			if ((numbytes = recv(new_fd, buf, sizeof(buf)-1, 0)) == -1) {
				perror("recv");
				close(new_fd);
				exit(1);
			}
			buf[numbytes] = '\0';

			// Parse the request to get the filename
			char method[10], filepath[255], protocol[10];
			sscanf(buf, "%s %s %s", method, filepath, protocol);
			// If the method is not GET, return 400
			if (strcmp(method, "GET") != 0) {
				char response[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
				send(new_fd, response, sizeof(response)-1, 0);
				close(new_fd);
				exit(0);
			}
			char *filename = filepath + 1; // skip leading '/'

			// Check the file
			FILE *fp = fopen(filename, "r");
			if (fp) {
				char response[1024];
				sprintf(response, "HTTP/1.1 200 OK\r\n\r\n"); //string print formatted
				send(new_fd, response, strlen(response), 0);
				char filebuf[1024];
				while ((numbytes = fread(filebuf, 1, sizeof(filebuf), fp)) > 0) {
					send(new_fd, filebuf, numbytes, 0);
				}
				fclose(fp);
			} else {
				if (errno == ENOENT) { // File not found
					char response[] = "HTTP/1.1 404 Not Found\r\n\r\n";
					send(new_fd, response, sizeof(response)-1, 0);
				} else {
					char response[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
					send(new_fd, response, sizeof(response)-1, 0);
				}
			}

			
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

