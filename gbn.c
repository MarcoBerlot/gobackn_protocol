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

uint16_t gbnhdr_checksum(gbnhdr *packet) {
	int totalLength =  sizeof(packet->seqnum) + sizeof(packet->type) + sizeof(packet->data);
	fprintf(stdout,"totalLength%d\n",totalLength);
	uint32_t temp = packet->checksum;
	packet->checksum = 0 ;
	uint32_t returnval = checksum(packet, (totalLength / sizeof(uint16_t)));
	packet->checksum = temp ;
	return returnval;

}


static void sig_alrm(int signo) {
	fprintf(stdout,"Signal handler\n");
	return;
}

int sendWindow(int sockfd,int base, int w,gbnhdr **packetarray, struct sockaddr *client, socklen_t socklen, int times){
	if (signal(SIGALRM, sig_alrm) == SIG_ERR)
		return (-1);
	int i=0;
	int n=0;
	alarm(TIMEOUT);
	fprintf(stdout,"Alarm set\n");
	for(i=base;i<base+w;i++) {
		if (i>=times)
			break;

		n = sendto(sockfd, packetarray[i], sizeof(gbnhdr), 0, client, socklen);
		fprintf(stdout, "Sending packet %u content %u type %u checksum %u n= %d\n", packetarray[i]->seqnum, packetarray[i]->data,packetarray[i]->type, packetarray[i]->checksum, n);
	}
	return n;

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
	int  seqnum = 0;
	int  w=W;
	int  base=0;
	int  tnumberofpackets;
	int tmp=16;
	char content[2];



	gbnhdr *ack=malloc(sizeof(gbnhdr));
	gbnhdr *fin=malloc(sizeof(gbnhdr));
	fin->type=FIN;

	ack->seqnum=-42;
	ack->type=0;
	ack->data=NULL;
	ack->checksum=0;



	fprintf(stdout, "Divided into %d packets original length: %d  DATALEN %d chp0 \n",times, len,DATALEN);
	if(len<DATALEN){
		times=1;

	}
	gbnhdr **packetarray=malloc(times*sizeof(gbnhdr));

	/*Allocating packets and inserting them in my array*/
	for(m=0;m<times;m++) {

		fprintf(stdout, "am I here? chp1 %u\n",buffer);


		gbnhdr *packet = malloc(sizeof(gbnhdr));
		packet->type = DATA;
		packet->seqnum = seqnum++;
		packet->data = malloc(sizeof(DATALEN));
		packet->data=buffer[m];

		uint32_t cs =  gbnhdr_checksum(packet);
		fprintf(stdout,"this is cs :%u\n",cs);

		/*packet->checksum = malloc(sizeof(DATALEN));*/
		packet->checksum = cs;

		fprintf(stdout,"INSIDE checksum from the sender side function call: %u \n",cs);
		fprintf(stdout,"INSIDE checksum from the sender side packet->checksum: %u, %u \n",(packet->checksum),cs);

		for(tmp=0;tmp<2;tmp++){
			content[tmp]=buffer[tmp];
		}
		fprintf(stdout,"CONTENT %s\n",content);

		packetarray[m]=packet;



	}
	/*
	*fprintf(stdout, "This is checking the content!!!!! packet:%u with content: %u\n",packetarray[0]->seqnum,packetarray[0]->data);
	*fprintf(stdout, "This is checking the content!!!!! packet:%u with content: %u\n",packetarray[1]->seqnum,packetarray[1]->data);
	*fprintf(stdout,"OUTSIDE checksum from the sender side function call: %u \n",gbnhdr_checksum(packetarray[0]));
	*fprintf(stdout,"OUTSIDE checksum from the sender side packet->checksum: %u \n",packetarray[0]->checksum);
	*/
	tnumberofpackets=times;
	fprintf(stdout, "This is checking the content!!!!! packet:%u with content: %u\n",packetarray[0]->seqnum,packetarray[0]->data);

	tnumberofpackets=times;
	seqnum=0;
	w=2;
	if (signal(SIGALRM, sig_alrm) == SIG_ERR)
		return (-1);

	seqnum=sendWindow(sockfd, base,w, packetarray, client,socklen, times);
	while(base<tnumberofpackets){
		fprintf(stdout,"waiting for ack %d\n",base);
		n=recvfrom(sockfd,ack,sizeof(ack),0,client, &socklen);
		fprintf(stdout,"Received ack %d n:%d sockfd %d, client %u socklen %u n= %d \n",ack->seqnum,n,sockfd,client,socklen,times);

		if(n!=-1 && ack->seqnum>=base && ack->seqnum<base+w ){
			/*Woken up by ack*/
			base=ack->seqnum+1;
			w=2;

				seqnum = sendWindow(sockfd, base, w, packetarray, client, socklen, m);


		}else{
			/*Woken up by the signal, need to send again*/
			w=1;
			fprintf(stdout,"TIMEOUT\n");
			seqnum=sendWindow(sockfd,base,w, packetarray, client,socklen,times);
		}

	}

	sendto(sockfd, fin, sizeof(gbnhdr),0,client,socklen);

	free(packetarray);
	free(ack);
	free(fin);

	return(0);
}

ssize_t gbn_recv(int sockfd, void **buffer, size_t len, int flags, struct sockaddr * server, socklen_t socklen, FILE *outputFile, int *expected){

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
	packet->checksum =0;
	packet->seqnum=0;
	packet->data=malloc(sizeof(DATALEN));
	packet->data='1';
	char buf[11024];
	int n=0;
	int tmp=16;


		n = recvfrom(sockfd, packet, sizeof(packet), 0, server, &tmp);
		fprintf(stdout, "Received packet %u with content %u Type:%u Checksum %u \n", packet->seqnum, packet->data, packet->type,packet->checksum);
		uint32_t cs =  gbnhdr_checksum(packet);

		fprintf(stdout, "this is n!: %d\n",n);
		fprintf(stdout,"expected checksum: %u \n",packet->checksum);
		fprintf(stdout, "this is n: %d, Expected :%d\n",n,*expected);
		fprintf(stdout, "Received packet %u with content %u Type:%u \n", packet->seqnum, packet->data,packet->type);

		if((packet->seqnum)==*expected) {
			fprintf(stdout,"Reading %u\n",packet->data);
			ack->seqnum=*expected;
			n=sendto(sockfd, ack, sizeof(ack), 0, server, 16);
			*buffer=packet->data;
			*expected=*expected+1;

			}else{
			ack->seqnum=*expected-1;
			n=sendto(sockfd, ack, sizeof(ack), 0, server, 16);

			fprintf(stdout,"Sending ack %d n:%d\n",ack->seqnum,n);

			}
			if((packet->type)==FIN){
				return 0;
			}
	fprintf(stdout,"Writing on buffer %u\n",*buffer);
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
