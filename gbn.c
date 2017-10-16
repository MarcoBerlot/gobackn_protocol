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
static void sig_alrm(int signo) {
	fprintf(stdout,"Signal handler\n");
	return;
}

int sendWindow(int sockfd,int base, int w,gbnhdr **packetarray, struct sockaddr *client, socklen_t socklen, int total){
	if (signal(SIGALRM, sig_alrm) == SIG_ERR)
		return (-1);
	int i=0;
	int n=0;
	alarm(TIMEOUT);
	fprintf(stdout,"Alarm set\n");
	for(i=base;i<base+w;i++){
		if(packetarray[i]==NULL)
			break;
	n=sendto(sockfd, packetarray[i], sizeof(gbnhdr),0,client,socklen);
	fprintf(stdout,"Sending packet %u content %u\n", packetarray[i]->seqnum,packetarray[i]->data);
	}
	return i;

}
ssize_t gbn_send(int sockfd, char *buffer, size_t len, int flags, struct sockaddr *client, socklen_t socklen){
	
	/* TODO: Your code here. */

	/* Hint: Check the data length field 'len'.
	 *       If it is > DATALEN, you will have to split the data
	 *       up into multiple packets - you don't have to worry
	 *       about getting more than N * DATALEN.
	 */
	int reader=0;
	int n=1;
	int  times=len/DATALEN;
	int  m;
	int  seqnum=0;
	int  w=W;
	int  base=0;
	int  tnumberofpackets;
	gbnhdr **packetarray=malloc(times*sizeof(gbnhdr));
	gbnhdr *ack=malloc(sizeof(gbnhdr));
	gbnhdr *fin=malloc(sizeof(gbnhdr));
	fin->type=FIN;

	ack->seqnum=-42;
	ack->type=0;
	ack->data=NULL;
	ack->checksum=0;

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
	tnumberofpackets=m;
	seqnum=0;
	w=2;
	if (signal(SIGALRM, sig_alrm) == SIG_ERR)
		return (-1);

	seqnum=sendWindow(sockfd, base,w, packetarray, client,socklen, m);
	while(base<tnumberofpackets-1){
		fprintf(stdout,"waiting for ack %d\n",base);
		n=recvfrom(sockfd,ack,sizeof(ack),0,client, &socklen);
		fprintf(stdout,"Received ack %d n=%d\n",ack->seqnum, n);

		if(n!=-1 && ack->seqnum>=base && ack->seqnum<base+w ){
			/*Woken up by ack*/
			base=ack->seqnum+1;
			w=2;
			if(seqnum<tnumberofpackets)
				seqnum=sendWindow(sockfd,base,w, packetarray, client,socklen,m);

		}else{
			/*Woken up by the signal, need to send again*/
			w=1;
			fprintf(stdout,"TIMEOUT\n");
			seqnum=sendWindow(sockfd,base,w, packetarray, client,socklen,m);
		}

	}
	sendto(sockfd, fin, sizeof(gbnhdr),0,client,socklen);
	free(packetarray);
	free(ack);
	free(fin);

	return(n);
}

ssize_t gbn_recv(int sockfd, void *buffer, size_t len, int flags, struct sockaddr * server, socklen_t socklen){

	/* TODO: Your code here. */
	/*Allocating FINACK packet*/
	gbnhdr *finackpacket= malloc(sizeof(gbnhdr));
	gbnhdr *packet= malloc(sizeof(gbnhdr));
	gbnhdr *ack= malloc(sizeof(gbnhdr));
	if(ack==NULL){
		fprintf(stdout,"ERROR in Malloc\n");
		return(-1);
	}
	ack->seqnum=0;
	ack->type=DATAACK;
	ack->checksum=0;
	ack->data=NULL;

	finackpacket->type=FINACK;
	finackpacket->checksum=0;
	finackpacket->seqnum=0;
	finackpacket->data=malloc(sizeof(DATALEN));

	packet->type=DATA;
	packet->checksum=0;
	packet->seqnum=0;
	packet->data=malloc(sizeof(DATALEN));

	char buf[DATALEN];
	int expected=0;
	int n=0;

	while(1) {
		n = recvfrom(sockfd, packet, sizeof(packet), 0, server, &socklen);
		if((packet->seqnum)==expected) {
			    fprintf(stdout,"Reading %u\n",packet->data);
			sprintf(buf, "%u", packet->data);
			fprintf(stdout,"Sending ack %d n:%d sockfd %d, server %u socklen %u \n",ack->seqnum,n,sockfd,server,socklen);
			n=sendto(sockfd, ack, sizeof(ack), 0, server, 16);
				ack->seqnum=expected;
				expected++;

			}else{
			ack->seqnum=expected-1;
			n=sendto(sockfd, ack, sizeof(ack), 0, server, 16);

			fprintf(stdout,"Sending ack %d n:%d\n",ack->seqnum,n);

			}
			if((packet->type)==FIN){
				break;
			}
		}
	fprintf(stdout,"Freeing Memory\n");
	free(ack);
	free(packet);
	free(finackpacket);


	return n;

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


	return(42);
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

	/* TODO: Your code here. */
	fprintf(stdout,"Trying socket()\n");
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
