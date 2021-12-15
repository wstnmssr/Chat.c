#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define PORT "1701"
#define BACKLOG 10
#define MAXDATASIZE 145

// version binary string, length binary string
// put both in char* buffer, append message in ascii

void usage(){
	fprintf(stderr, "Usage: ./chat [-h] [-p p_arg] [-s s_arg]\n");
}

int digits_only(const char *s){
	while (*s) {
		if (isdigit(*s++) == 0){
			return 0;
		}
	}
	return 1;
}

int validate_ip(char* ipAddress){
	struct sockaddr_in sa;
	int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
	return result != 0;
}

void encode_packet(char* message, char* packet){
	char version0 = htons(457) >> 8;
	char version1 = htons(457) & 0b0000000011111111;
	packet[0] = version0;
	packet[1] = version1;

	char length0 = htons(strlen(message)) >> 8;
	char length1 = htons(strlen(message) & 0b0000000011111111);
	packet[2] = length0;
	packet[3] = length1;

	for(int i = 0; i < strlen(message); i++){
		packet[i+4] = message[i];
	}
}

char* decode_packet(char* packet){
	char version0 = packet[0];
	char version1 = packet[1];
	uint16_t version_n;
	if((version_n = ntohs((version0 << 8) | version1)) != 457){
		perror("version");
	}
	char length0 = packet[2];
	char length1 = packet[3];
	uint16_t length_n = ntohs((length0 << 8) | length1);

	char* message;
	message = (char*)malloc(length_n * sizeof(char));

	for(int i = 0; i < length_n-1; i++){
		message[i] = packet[i+4];
	}
	return message;
}

void client(char* port, char* ip){
	
	struct addrinfo hints, *servinfo, *p;
	int sockfd, rv;
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(ip, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("client: connect");
			close(sockfd);
			continue;
		}

		break;
	}
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		exit(1);
	}
	freeaddrinfo(servinfo); // all done with this structure
	printf("Connected!\nConnected to a friend! You send first.\n");

	int numbytes;
	char *buffer, packet[MAXDATASIZE];
	buffer = (char*)malloc(MAXDATASIZE * sizeof(char));
	size_t bufsize = 172;
	size_t characters;


	char buf[MAXDATASIZE];
	char* message_received;

	while(1){

		printf("You: ");
		characters = getline(&buffer,&bufsize,stdin);
		while(strlen(buffer) > 140){
			printf("Error: Input too long.\nYou:");
			characters = getline(&buffer,&bufsize,stdin);
		}
		encode_packet(buffer, packet);

		if ((numbytes = send(sockfd, packet, MAXDATASIZE, 0)) == -1){
			perror("send");
		}

		if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
			perror("recv");
			exit(1);
		}
		if(numbytes == 0){
			break;
		}

		message_received = decode_packet(buf);

		printf("Friend: %s\n",message_received);


	}

	close(sockfd);
}

void server(){
	printf("Welcome to chat!\n");

	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	int yes=1;
	char hostbuffer[256];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
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

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	struct hostent *host;
	gethostname(hostbuffer, sizeof(hostbuffer));
	host = gethostbyname(hostbuffer);
	struct in_addr* ip = (struct in_addr*)host->h_addr;

	char* ip_addr = inet_ntoa(*ip);

	printf("Waiting for a connection on %s port %d\n", ip_addr, 1701);

	sin_size = sizeof their_addr;
	new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
	if (new_fd == -1) {
		perror("accept");
		exit(1);
	}
	close(sockfd);


	int numbytes;
	char buf[MAXDATASIZE];

	printf("Found a friend! You receive first.\n");

	char *buffer = "", packet[145];
	size_t bufsize = 172;
	size_t characters;

	buffer = (char *)malloc(bufsize * sizeof(char));

	char* message_received;

	while(1) {

		if ((numbytes = recv(new_fd, buf, MAXDATASIZE, 0)) == -1) {
			perror("recv");
			exit(1);
		}
		if(numbytes == 0){
			break;
		}

		message_received = decode_packet(buf);

		printf("Friend: %s\nYou: ", message_received);

		characters = getline(&buffer,&bufsize,stdin);
		while(strlen(buffer) > 140){
			printf("Error: Input too long.\nYou:");
			characters = getline(&buffer,&bufsize,stdin);
		}

		encode_packet(buffer, packet);

		if (send(new_fd, packet, MAXDATASIZE, 0) == -1){
			perror("send");
		}
	}
	close(new_fd);
}

int main(int argc, char* argv[]){
	if(argc > 1){
		int opt;
		char* optvalue = NULL;
		char* port = NULL;
		char* ip_addr = NULL;

		if(argc >= 2 && argc < 5){
			usage();
			return 1;
		}
		while((opt = getopt(argc, argv, "hp:s:")) != -1){
			switch(opt){
				case 'p':
					port = optarg;
					break;
				case 's':
					ip_addr = optarg;
					break;
				case 'h':
				default:
					usage();
					return 1;
			}
		}
		if(digits_only(port)){
			if(atoi(port) > 65535){
				fprintf(stderr, "Invalid Port Number\n");
				return 1;
			}
		}
		else{
			fprintf(stderr, "Invalid Port Number\n");
			return 1;
		}
		if(!validate_ip(ip_addr)){
			fprintf(stderr, "Invalid IP Address\n");
			return 1;
		}
		client(port, ip_addr);
	}
	else if(argc){
		server();
	}
	return 0;
}