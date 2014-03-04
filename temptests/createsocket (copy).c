/*#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netinet/tcp.h>

#define PORT_TIME       13              
#define PORT_FTP        21             
#define SERVER_ADDR     "127.0.0.1"     
#define MAXBUF          1024

int main(int argc, char **argv)
{   int sockfd;
    struct sockaddr_in dest;
    char buffer[MAXBUF];

	//if(-1){printf("if -1 success:\n");}
	//if(0){printf("if 0 success:\n");}
	//if(1){printf("if 1 success:\n");}

    //---Open socket for streaming---
    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        perror("Socket");
        return errno;
    }

printf("I have a socketfd: %i\n", sockfd);

///
printf("Press a key to setsockop:\n");
getchar();
int on = 9;
int status = setsockopt(sockfd,  IPPROTO_TCP , TCP_KEEPCNT,(const void *) &on, sizeof(on));
printf("status is: %i\n", status);
perror("setsockopt");
////

printf("Press a key to sendto:\n");
getchar();

struct sockaddr_in pin;
memset(&pin, 0, sizeof(pin));
pin.sin_family = AF_INET;
pin.sin_addr.s_addr = inet_addr(argv[1]);
pin.sin_port = htons(atoi(argv[2]));

printf("argv[3] is: %s\n", argv[3]);
printf("strlen(argv[3]) is: %zu\n", strlen(argv[3]));

printf("sizeof(pin) is: %lu\n", sizeof(pin));

int ret = sendto(sockfd, argv[3], strlen(argv[3]), 0, (struct sockaddr *)  &pin, sizeof(pin));
printf("Ret is: %i\n", ret);


printf("Press a key to close socket:\n");
getchar();
    
    //---Clean up---
    close(sockfd);
    return 0;
}



//printf("pin.sin_addr.s_addr is: %i\n", pin.sin_addr.s_addr);
//printf("pin.sin_port is: %i\n", pin.sin_port);
//printf("atoi(argv[2] is: %i\n", atoi(argv[2]));
//printf("argv[2] is: %s\n", argv[2]);
//printf("htonl(atoi(argv[2])) is: %i\n", htonl(atoi(argv[2])));

*/


//sudo iptables -t filter -I OUTPUT -p tcp tcp-flags RST RST -j DROP -- used to get kernel to stop sending RST


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    server = gethostbyname(argv[1]);

    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    
//bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);

serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

    serv_addr.sin_port = htons(portno);


if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

//printf("Press a key to sendon socket:\n");
//getchar();
  //  if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
  //      error("ERROR connecting");
  //  n = write(sockfd,argv[3],strlen(argv[3]));

//int ret = sendto(sockfd, argv[3], strlen(argv[3]), 0, (struct sockaddr *)  &serv_addr, sizeof(serv_addr));


int ret = send(sockfd, argv[3], strlen(argv[3]), 0);
printf("Ret is: %i\n", ret);

printf("OK:\n");

//printf("Press a key to close socket:\n");
//getchar();
    
    close(sockfd);
    return 0;
}
