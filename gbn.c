#include "gbn.h"

uint16_t checksum(uint16_t *buf, int nwords)
{
	uint32_t sum;

	for (sum = 0; nwords > 0; nwords--)
		sum += *buf++;
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return ~sum;
}

ssize_t gbn_send(int sockfd, char *buffer, size_t len, int flags, struct sockaddr *client, socklen_t *socklen){
	
	/* TODO: Your code here. */

	/* Hint: Check the data length field 'len'.
	 *       If it is > DATALEN, you will have to split the data
	 *       up into multiple packets - you don't have to worry
	 *       about getting more than N * DATALEN.
	 */
	char buf[1024];
	int n=0;
	fprintf(stdout,"Trying send %ld\n",len);

	n=sendto(sockfd, buffer, len, 0, client, socklen);


	fprintf(stdout,"Sent %d\n",n);

	if(n==-1){
		fprintf(stdout,"ERROR: sendTo()");
	}
	return(n);
}

ssize_t gbn_recv(int sockfd, void *buffer, size_t len, int flags, struct sockaddr * server, socklen_t *socklen){

	/* TODO: Your code here. */
	int n=0;
	n=recvfrom(sockfd, buffer, len, 0,server, socklen);

	return(n);
}

int gbn_close(int sockfd){

	/* TODO: Your code here. */
	close(sockfd);
	return(-1);
}

int gbn_connect(int sockfd, const struct sockaddr *server, socklen_t socklen){

	/* TODO: Your code here. */
	fprintf(stdout,"Trying connect()\n");
	char buf[10];
	sprintf(buf, "%d", SYN);

	int n=sendto(sockfd,buf, sizeof(buf), 0, server, socklen);
	if(n==-1){
		fprintf(stdout,"ERROR: sendTo()");
	}
	return(n);
}

int gbn_listen(int sockfd, int backlog){

	char buf[1024];
	/* TODO: Your code here. */
	int n=recvfrom( sockfd, buf, sizeof(buf), 0,0,0);
	if(n==-1){
		fprintf(stdout,"ERROR: listen()\n");
	}
	return(n);
}

int gbn_bind(int sockfd, const struct sockaddr *server, socklen_t socklen){

	/* TODO: Your code here. */
	fprintf(stdout,"Trying bind()\n");
	int t=bind(sockfd, server, socklen);
	if(t==-1) {
		fprintf(stdout, "ERROR: bind()");
	}

	return t;

}	

int gbn_socket(int domain, int type, int protocol){
		
	/*----- Randomizing the seed. This is used by the rand() function -----*/
	srand((unsigned)time(0));
	fprintf(stdout,"Trying socket()\n");
	/* TODO: Your code here. */
	int l=socket(domain,type, protocol);
	if(l==-1){
		fprintf(stdout,"ERROR: socket()\n");
		return (-1);
	}else{
		fprintf(stdout,"created socket:%d\n",l);
		return l;

	}

}

int gbn_accept(int sockfd, struct sockaddr *client, socklen_t *socklen){

	/* TODO: Your code here. */
	char buf[10];
	int n=0;
	sprintf(buf, "%d", SYNACK);
	n=sendto(sockfd, buf, sizeof(buf), 0, client, *socklen);

	return(sockfd);
}

ssize_t maybe_sendto(int  s, const void *buf, size_t len, int flags, \
                     const struct sockaddr *to, socklen_t tolen){

	char *buffer = malloc(len);
	memcpy(buffer, buf, len);
	
	
	/*----- Packet not lost -----*/
	if (rand() > LOSS_PROB*RAND_MAX){
		/*----- Packet corrupted -----*/
		if (rand() < CORR_PROB*RAND_MAX){
			
			/*----- Selecting a random byte inside the packet -----*/
			int index = (int)((len-1)*rand()/(RAND_MAX + 1.0));

			/*----- Inverting a bit -----*/
			char c = buffer[index];
			if (c & 0x01)
				c &= 0xFE;
			else
				c |= 0x01;
			buffer[index] = c;
		}

		/*----- Sending the packet -----*/
		int retval = sendto(s, buffer, len, flags, to, tolen);
		free(buffer);
		return retval;
	}
	/*----- Packet lost -----*/
	else
		return(len);  /* Simulate a success */
}
