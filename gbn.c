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

ssize_t gbn_send(int sockfd, char *buffer, size_t len, int flags, struct sockaddr *client, socklen_t socklen){
	
	/* TODO: Your code here. */

	/* Hint: Check the data length field 'len'.
	 *       If it is > DATALEN, you will have to split the data
	 *       up into multiple packets - you don't have to worry
	 *       about getting more than N * DATALEN.
	 */
	int i=0;
	int reader=0;
	int n=0;
	char *buf=buffer;
	char ch;
	int times=len/DATALEN;
	int m;
	int seqnum=0;
	gbnhdr **packetarray=malloc(times*sizeof(gbnhdr));;

	/*Allocating packets and inserting them in my array*/
	for(m=0;m<times;m++) {
		gbnhdr *packet = malloc(sizeof(gbnhdr));
		packet->type = DATA;
		packet->checksum = 0;
		packet->seqnum = seqnum++;
		packet->data = malloc(sizeof(DATALEN));
		for(reader=0;reader<DATALEN;reader++){
			packet->data[reader]=buffer[reader];
			buffer++;
		}
		packetarray[m]=packet;

	}

		for (i = 0; i < W; i++) {
			n = sendto(sockfd, packetarray[i], sizeof(gbnhdr), 0, client, socklen);
			fprintf(stdout, "Sent packet:%u with content: %s\n",packetarray[i]->seqnum,packetarray[i]->data);
		}


	return(n);
}

ssize_t gbn_recv(int sockfd, void *buffer, size_t len, int flags, struct sockaddr * server, socklen_t socklen){

	/* TODO: Your code here. */
	/*Allocating FINACK packet*/
	gbnhdr *finackpacket= malloc(sizeof(gbnhdr));
	finackpacket->type=FINACK;
	finackpacket->checksum=0;
	finackpacket->seqnum=0;
	finackpacket->data=malloc(sizeof(DATALEN));
	int i=0;
	int out=0;
	int n=1;
	char *buf;

	gbnhdr *packet = malloc(sizeof(gbnhdr));
	while(1) {
		for (i = 0; i < W; i++) {
			n = recvfrom(sockfd, packet, sizeof(packet), 0, server, &socklen);
			fprintf(stdout, "Received packet %u with content %c Type:%u \n", packet->seqnum, packet->data,packet->type);
			if(packet->type==FIN) {
				/*SENDING SYNACK*/
				n=sendto(sockfd,finackpacket, sizeof(finackpacket), 0, server, socklen);
				if(n>0) {
					fprintf(stdout, "Sent SYNACK\n");
					return (0);
				}

			}
		}
	}

}

int gbn_close(int sockfd,const struct sockaddr *server, socklen_t socklen){

	/* TODO: Your code here. */
	/*Allocating FIN % FINACK packet*/
	gbnhdr *finpacket= malloc(sizeof(gbnhdr));
	gbnhdr *finackpacket= malloc(sizeof(gbnhdr));
	finpacket->type=FIN;
	finpacket->checksum=0;
	finpacket->seqnum=0;
	finpacket->data=malloc(sizeof(DATALEN));
	fprintf(stdout,"Sending FIN %u\n",finpacket->type);
	int n=sendto(sockfd,finpacket, sizeof(finpacket), 0, server, socklen);
	if(n==-1){
		fprintf(stdout,"ERROR: sendTo()");
	}
	n=recvfrom(sockfd, finackpacket, sizeof(gbnhdr), 0,server, &socklen);
	if(finackpacket->type==FINACK) {
		fprintf(stdout,"RECEIVED FINACK\n CLOSE CONNECTION\n");
		close(sockfd);
	}
	return(n);

}

int gbn_connect(int sockfd, const struct sockaddr *server, socklen_t socklen){

	/* TODO: Your code here. */
	/*Setup connection sending a SYN*/
	char buf[10];
	char buffer[10];
	/*Allocating SYN packet*/
	gbnhdr *synpacket= malloc(sizeof(gbnhdr));
	synpacket->type=SYN;
	synpacket->checksum=0;
	synpacket->seqnum=0;
	synpacket->data=malloc(sizeof(DATALEN));



	fprintf(stdout,"Sending SYN %u\n",synpacket->type);
	int n=sendto(sockfd,synpacket, sizeof(synpacket), 0, server, socklen);
	if(n==-1){
		fprintf(stdout,"ERROR: sendTo()");
	}

	/*Waiting to receive the SYNACK*/
	gbnhdr *asynpacket= malloc(sizeof(gbnhdr));
	fprintf(stdout,"Waiting SYNACK \n");
	n=recvfrom(sockfd, asynpacket, sizeof(gbnhdr), 0,server, &socklen);
	if(asynpacket->type!=SYNACK)
		return -1;
	fprintf(stdout, "Received SYNACK %u\n", asynpacket->type);

	return(n);
}

int gbn_listen(int sockfd, int backlog){

	char buf[1024];
	fprintf(stdout,"listening\n");
	/* TODO: Your code here. */
	int n=recvfrom( sockfd, buf, sizeof(buf), 0,0,0);
	fprintf("Received SYN:%s\n",buf);
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

int gbn_accept(int sockfd, struct sockaddr *client, socklen_t socklen){

	/* TODO: Your code here. */
	gbnhdr *synpacket= malloc(sizeof(gbnhdr));


	/*RECEIVE SYN*/
	int n=recvfrom(sockfd, synpacket, sizeof(synpacket), 0,client, &socklen);
	fprintf(stdout,"Received %u\n",synpacket->type);

	/*Allocating SYNACK packet*/
	gbnhdr *asynpacket= malloc(sizeof(gbnhdr));
	asynpacket->type=SYNACK;
	asynpacket->checksum=0;
	asynpacket->seqnum=0;
	asynpacket->data=malloc(sizeof(DATALEN));

	/*SENDING SYNACK*/
	n=sendto(sockfd,asynpacket, sizeof(asynpacket), 0, client, socklen);
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
