/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT_DEFAULT_80 "80" // the port client will be connecting to changed to 80 as default

#define MAXDATASIZE 100 // max number of bytes we can get at once 

void parse_url(char *url, char **hostname, char **port, char **path) {
    *hostname = &url[7]; // skip 'http://'
    *port = PORT_DEFAULT_80; // default port
    *path = strchr(*hostname, '/');
    if (*path) {
        **path = '\0';
        (*path)++;
    } else {
        *path = "";
    }

    char *colon = strchr(*hostname, ':');
    if (colon) {
        *colon = '\0';
        *port = colon + 1;
    }
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	if (argc != 2) {
        fprintf(stderr, "usage: %s http://hostname[:port]/path/to/file\n", argv[0]);
        exit(1);
    }

    char *hostname, *port, *path;
    parse_url(argv[1], &hostname, &port, &path);

	// if (argc != 2) {
	//     fprintf(stderr,"usage: client hostname\n");
	//     exit(1);
	// }

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	
	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	char request[1024];
	sprintf(request, "GET /%s HTTP/1.1\r\nHost: %s:%s\r\n\r\n", path, hostname, port);
	send(sockfd, request, strlen(request), 0);

	freeaddrinfo(servinfo); // all done with this structure

	// if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	//     perror("recv");
	//     exit(1);
	// }

	// buf[numbytes] = '\0';

	// printf("client: received '%s'\n",buf);

    // Handle the response
	FILE *fp = fopen("output", "wb");
	if (!fp) {
		perror("Unable to open file for writing");
		close(sockfd);
		return 1;
	}

	int header_found = 0;
	char *header_end;
	char response_buf[MAXDATASIZE] = {0};

	while ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) > 0) {
		if (!header_found) {
			// Look for end of HTTP header in the received data
			buf[numbytes] = '\0'; // Null-terminate the received data
			header_end = strstr(buf, "\r\n\r\n");

			if (header_end) {
				header_found = 1;
				header_end += 4;  // Move pointer past the "\r\n\r\n" sequence
				fwrite(header_end, 1, buf + numbytes - header_end, fp);
			}
		} else {
			// Header already found, so write the whole buffer to the file
			fwrite(buf, 1, numbytes, fp);
		}
	}
	fclose(fp);


    close(sockfd);
    return 0;
}

