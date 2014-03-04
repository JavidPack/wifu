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


//sudo iptables -t filter -I OUTPUT -p tcp --tcp-flags RST RST -j DROP -- used to get kernel to stop sending RST

/*
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
}*/

#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#define PORT 		0x8855
#define DIRSIZE 	8192

main()
{
        char     dir[DIRSIZE];  /* used for incomming dir name, and
					outgoing data */
	int 	 sd, sd_current, cc, fromlen, tolen;
	int 	 addrlen;
	struct   sockaddr_in sin;
	struct   sockaddr_in pin;
 
	/* get an internet domain socket */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}


	int reuse_opt = 1;

    	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &reuse_opt, sizeof(int));


printf("1\n");

	/* complete the socket structure */
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(PORT);
printf("2\n");

	/* bind the socket to the port number */
	if (bind(sd, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
		perror("bind");
		exit(1);
	}
printf("3 - Bind success\n");

	/* show that we are willing to listen */
	if (listen(sd, 5) == -1) {
		perror("listen");
		exit(1);
	}
printf("4 - Listen success\n");
	/* wait for a client to talk to us */
        addrlen = sizeof(pin); 
	if ((sd_current = accept(sd, (struct sockaddr *)  &pin, &addrlen)) == -1) {
		perror("accept");
		exit(1);
	}
printf("5 - accept success, sd_current is %i\n", sd_current);

/* if you want to see the ip address and port of the client, uncomment the 
    next two lines */

       
//printf("Hi there, from  %s#\n",inet_ntoa(pin.sin_addr));
//printf("Coming from port %d\n",ntohs(pin.sin_port));
        

	/* get a message from the client */
	if (recv(sd_current, dir, sizeof(dir), 0) == -1) {
		perror("recv");
		exit(1);
	}
printf("6\n");

        /* get the directory contents */
       //  read_dir(dir);
    printf("Received:  %s\n", dir);
      /* strcat (dir," DUDE");
       */
	/* acknowledge the message, reply w/ the file names */
	if (send(sd_current, dir, strlen(dir), 0) == -1) {
		perror("send");
		exit(1);
	}
printf("7\n");

        /* close up both sockets */
	close(sd_current); close(sd);
        
printf("8\n");
        /* give client a chance to properly shutdown */
        sleep(1);
printf("9\n");
}


