#include "gbn.h"
const struct sockaddr myserver;
socklen_t mysocklen;

const struct sockaddr myclient;
socklen_t mycsocklen;
int expected=0;


uint16_t checksum(uint16_t *buf, int nwords)
{
    uint32_t sum;

    for (sum = 0; nwords > 0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return ~sum;
}

uint16_t gbnhdr_checksum(gbnhdr packet,int length) {

    uint32_t temp = packet.checksum;
    fprintf(stdout,"checksum!inside%u\n",packet.checksum);
    packet.checksum = 0 ;
    fprintf(stdout,"checksum! inside2%u\n",packet.checksum);
    uint32_t returnval = checksum(&packet, (length / sizeof(uint16_t)));
    packet.checksum = temp ;
    return returnval;

}

static void sig_alrm(int signo) {
    fprintf(stdout,"Signal Handler\n");
    return;
}

int sendWindow(int sockfd,int base, int w,gbnhdr *packetarray, struct sockaddr *client, socklen_t socklen, int times,int reminder){
    if (signal(SIGALRM, sig_alrm) == SIG_ERR)
        return (-1);
    int i=0;
    int n=0;
    alarm(TIMEOUT);
    fprintf(stdout,"Alarm set\n");
    for(i=base;i<base+w;i++) {
        if (i>=times)
            break;
        if(i==times-1 && times!=1024){
            uint32_t cs =  gbnhdr_checksum(packetarray[i],sizeof(gbnhdr)-(DATALEN-reminder));
            packetarray[i].checksum = cs;
            fprintf(stdout,"NOT END checksum from the sender side packet->seq : %u, packet->type : %u, packet->data : %u, checksum =%u function call %u \n",packetarray[i].seqnum,packetarray[i].type,packetarray[i].data,packetarray[i].checksum,cs);
            n = sendto(sockfd, &packetarray[i], sizeof(gbnhdr)-(DATALEN-reminder), 0, client, socklen);
            fprintf(stdout, "\n\nLAST PACKET Contenst \n\n");

        }
        else {
            uint32_t cs =  gbnhdr_checksum(packetarray[i], sizeof(gbnhdr));
            packetarray[i].checksum = cs;
            fprintf(stdout,"NOT END checksum from the sender side packet->seq : %u, packet->type : %u, packet->data : %u, checksum =%u function call %u \n",packetarray[i].seqnum,packetarray[i].type,packetarray[i].data,packetarray[i].checksum,cs);
            n = sendto(sockfd, &packetarray[i], sizeof(gbnhdr), 0, client, socklen);
        }

    }
    return n;

}
/*ssize_t gbn_send(int sockfd, const void *buf, size_t len, int flags)*/

ssize_t gbn_send(int sockfd, const void *buffer, size_t len, int flags){

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
    int i=0;
    int bufferReader=0;
    int reminder=len%DATALEN;



    gbnhdr *ack=malloc(sizeof(gbnhdr));
    gbnhdr *fin=malloc(sizeof(gbnhdr));
    fin->type=FIN;

    ack->seqnum=-42;
    ack->type=0;
    ack->checksum=0;


    fprintf(stdout, "Divided into %d packets original length: %d  DATALEN %d reminder %d chp0 \n",times, len,DATALEN,reminder);
    if(reminder>0)
        i=1;
    gbnhdr *packetarray=malloc((times+i)*sizeof(gbnhdr));

    /*Allocating packets and inserting them in my array*/
    for(m=0;m<times;m++) {



        gbnhdr packet;
        packet.type = DATA;
        packet.checksum = 0;
        packet.seqnum = seqnum++;
        /*for(i=0;i<DATALEN;i++) {
            packet.data[i] = buffer[bufferReader++];
            fprintf(stdout,"CONTENT %u",packet.data[i]);
        }*/
        memcpy(packet.data,buffer+m*DATALEN,DATALEN);
        /*buffer++;*/
        packetarray[m]=packet;

    }

    if (reminder>0){
        fprintf(stdout, "Adding  one last packet of length %d \n",reminder);
        gbnhdr packet;
        packet.type = DATA;
        packet.checksum = 0;
        packet.seqnum = seqnum++;
        memcpy(packet.data,buffer+m*DATALEN,DATALEN);
        packetarray[m]=packet;
        times++;
    }


    tnumberofpackets=times;
    seqnum=0;
    w=2;
    if (signal(SIGALRM, sig_alrm) == SIG_ERR)
        return (-1);

    seqnum=sendWindow(sockfd, base,w, packetarray, &myserver,mysocklen, times,reminder);
    while(base<tnumberofpackets){
        fprintf(stdout,"waiting for ack %d\n",base);
        n=recvfrom(sockfd,ack,sizeof(ack),0,&myserver, &mysocklen);
        fprintf(stdout,"Received ack %d n:%d\n",ack->seqnum,n,sockfd);

        if(n!=-1 && ack->seqnum>=base && ack->seqnum<base+w ){
            /*Woken up by ack*/
            base=ack->seqnum+1;
            w=2;

            seqnum = sendWindow(sockfd, base, w, packetarray, &myserver, mysocklen, times,reminder);

        }
        if(n==-1){
            /*Woken up by the signal, need to send again*/
            w=1;
            fprintf(stdout,"TIMEOUT|\n");
            seqnum=sendWindow(sockfd,base,w, packetarray, &myserver, mysocklen,times,reminder);
        }

    }
    if(len!=1048576 ) {
        fprintf(stdout,"SENDING FIN\n");
        sendto(sockfd, fin, sizeof(gbnhdr), 0, &myserver, mysocklen);
    }

    return(0);
}

ssize_t gbn_recv(int sockfd, void *buffer, size_t len, int flags){

    /* TODO: Your code here. */
    /*Allocating FINACK packet*/
    gbnhdr *finackpacket= malloc(sizeof(gbnhdr));
    gbnhdr mypacket;
    gbnhdr *ack= malloc(sizeof(gbnhdr));


    if(ack==NULL){
        fprintf(stdout,"ERROR in Malloc\n");
        return(-1);
    }
    ack->seqnum=0;
    ack->type=DATAACK;
    ack->checksum=0;

    finackpacket->type=FINACK;
    finackpacket->checksum=0;
    finackpacket->seqnum=0;


    char buf[DATALEN];
    int n=0;
    int tmp=16;
    int m=0;

    while(1) {
        m = recvfrom(sockfd, &mypacket, sizeof(mypacket), 0, &myclient, &mycsocklen);
        fprintf(stdout, "this is n: %d,Expected :%d\n", n, expected);
        fprintf(stdout, "Received packet %u ", mypacket.seqnum);

        uint32_t cs =  gbnhdr_checksum(mypacket,m);

        fprintf(stdout,"INSIDE checksum from the receiver side packet->seq : %u, packet->type : %u, packet->data : %u, checksum =%u function call %u \n",mypacket.seqnum,mypacket.type,mypacket.data,mypacket.checksum,cs);

        /*fprintf(stdout, "%s\n",buffer);*/
        if ((mypacket.seqnum) == expected && (mypacket.checksum == cs)) {
            fprintf(stdout, "Reading  %u\n", mypacket.data);
            ack->seqnum = expected;
            n = sendto(sockfd, ack, sizeof(ack), 0, &myclient, mycsocklen);
            /*for(tmp=0;tmp<DATALEN;tmp++){
               if(*(packet->data)+tmp=='/0'){
                  fprintf(stdout,"NULL CHARACTER\n");
                  break;
               }

               buf[tmp]=*(packet->data)+tmp;
               fprintf(stdout,"DEBUG  %u ", buf[tmp]);
            }*/
            tmp = expected + 1;
            if(tmp==1024)
                tmp=0;
            memcpy(&expected, &tmp, sizeof(int));

            memcpy(buffer, mypacket.data, sizeof(mypacket.data));
            fprintf(stdout,"\n M-7  %d\n",m-4);
            return m-4;

        } else {
            ack->seqnum = expected - 1;
            n = sendto(sockfd, ack, sizeof(ack), 0, &myclient, mycsocklen);

            fprintf(stdout, "Sending ack %d n:%d\n", ack->seqnum, n);

        }
        if ((mypacket.type) == FIN) {
            fprintf(stdout, "RECEIVED FIN\n");
            return 0;
        }


    }

}
void changeStage(int *stage){
    *stage = 1;
}
int gbn_close(int sockfd){

    /* TODO: Your code here. */
    close(sockfd);
    return(0);

}

int gbn_connect(int sockfd, const struct sockaddr *server, socklen_t socklen){

    /* TODO: Your code here. */
    /*Setup connection sending a SYN*/
    /*Allocating SYN packet*/
    gbnhdr *synpacket= malloc(sizeof(gbnhdr));
    synpacket->type=SYN;
    synpacket->checksum=0;
    synpacket->seqnum=0;

    memcpy( &myserver, server, sizeof(struct sockaddr *) );
    memcpy( &mysocklen, &socklen, sizeof(socklen_t) );
    fprintf(stdout,"Original Server: %ld. My %ld\n\n",server,&myserver);


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


    return(0);
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
    memcpy( &myclient, client, sizeof(struct sockaddr *) );
    memcpy( &mycsocklen, &socklen, sizeof(socklen_t) );

    /*RECEIVE SYN*/
    int n=recvfrom(sockfd, synpacket, sizeof(synpacket), 0,client, &socklen);
    fprintf(stdout,"Received %u\n",synpacket->type);

    /*Allocating SYNACK packet*/
    gbnhdr *asynpacket= malloc(sizeof(gbnhdr));
    asynpacket->type=SYNACK;
    asynpacket->checksum=0;
    asynpacket->seqnum=0;

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


