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
/**********************************************************************************************************************/
/* A helper function for checksum. It calls checksum function at the end.
 * The gbnhdr_checksum temporarily stores the original checksum value (useful for reciever). Then it computes the checksum and puts back the value
 * to the corresponding packet. We divided by the size of uint16_t for nwords because the checksum provided by the lab checks the CS
 * by 0xffff which is 2 bytes*/
/**********************************************************************************************************************/
uint16_t gbnhdr_checksum(gbnhdr packet,int length) {

    uint32_t temp = packet.checksum;
    packet.checksum = 0 ;
    uint32_t returnval = checksum(&packet, (length / sizeof(uint16_t)));
    packet.checksum = temp ;
    return returnval;

}

static void sig_alrm(int signo) {
    fprintf(stdout,"Signal Handler\n");
    return;
}
/**********************************************************************************************************************/
/* Send the packets that are inside the window. The size of the window is taken as the parameter w as well as the base.
 * We set an alarm that will call a signal after TIMEOUT seconds.*/
/**********************************************************************************************************************/
int sendWindow(int sockfd,int base, int w,gbnhdr *packetarray, struct sockaddr *client, socklen_t socklen, int times,int reminder){
    if (signal(SIGALRM, sig_alrm) == SIG_ERR)
        return (-1);
    int i=0;
    int n=0;
    alarm(TIMEOUT);
    for(i=base;i<base+w;i++) {
        if (i>=times)
            break;
        if(i==times-1 && times!=N && reminder!=0){
            /*The length of the packet is calculated before calling the gbnhdr_checksum. We only want to calculate checksum of the valid data*/
            uint32_t cs =  gbnhdr_checksum(packetarray[i],sizeof(gbnhdr)-(DATALEN-reminder));
            packetarray[i].checksum = cs;
            n = maybe_sendto(sockfd, &packetarray[i], sizeof(gbnhdr)-(DATALEN-reminder), 0, client, socklen);

        }
        else {
            uint32_t cs =  gbnhdr_checksum(packetarray[i], sizeof(gbnhdr));
            packetarray[i].checksum = cs;
            n = maybe_sendto(sockfd, &packetarray[i], sizeof(gbnhdr), 0, client, socklen);
            printf("packet %u\n",packetarray[i].seqnum);

        }

    }
    return packetarray[i].seqnum;

}
/**********************************************************************************************************************/
/* The function first creates an array that will contain all the packets to send. Then it starts a loop that ends when
 * all the packets in the array are sent. Everytime the function waits at the receiving point. When the function is
 * unlocked depenging on the returning value we can instantiate a timeout or move the signal.*/
/**********************************************************************************************************************/
ssize_t gbn_send(int sockfd, const void *buffer, size_t len, int flags){

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
    int mult=0;

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
        memcpy(packet.data,buffer+m*DATALEN,DATALEN);
        packetarray[m]=packet;

    }
    /*Last packet smaller than DATALEN*/
    if (reminder>0){
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
    int counter=0;
    int newcounter=0;

    if (signal(SIGALRM, sig_alrm) == SIG_ERR)
        return (-1);

    seqnum=sendWindow(sockfd, base,w, packetarray, &myserver,mysocklen, times,reminder);
    while(counter<tnumberofpackets){
        printf("waiting for ack %d or %d\n",base, base+w);
        n=recvfrom(sockfd,ack,sizeof(ack),0,&myserver, &mysocklen);
        if(base==256)
            base=0;
        printf("Received  ack %d\n",ack->seqnum);

        if(n!=-1 && ack->seqnum>=base && ack->seqnum<=base+2 ){
            /*Woken up by ack*/
            counter=(ack->seqnum)+256*mult+1;
            printf("Counter %d, new counter %d\n",counter,newcounter);
            base=ack->seqnum+1;
            if(ack->seqnum==255)
                mult++;

            w=2;
            seqnum = sendWindow(sockfd, counter, w, packetarray, &myserver, mysocklen, times,reminder);

        }
        if(n==-1){
            /*Woken up by the signal, need to send again*/

            w=1;

            seqnum=sendWindow(sockfd,counter,w, packetarray, &myserver, mysocklen,times,reminder);
        }


    }
    /*We have finished to read the file and we close the connection*/
    if(len!=DATALEN*N) {
        fprintf(stdout,"Closing Connection\n");
        sendto(sockfd, fin, sizeof(gbnhdr), 0, &myserver, mysocklen);
    }

    return(0);
}

/**********************************************************************************************************************/
/* The function keeps receiving packets and checking if they are the expected ones and by the checksum check
 * if they are correct. For all the correct packets it sends an ack back to the sender. It returns the right
 * amount of bytes to write on the file.*/
/**********************************************************************************************************************/
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
    int m=0;
    int zero=0;
    int tmp=0;

    while(1) {
        m = recvfrom(sockfd, &mypacket, sizeof(mypacket), 0, &myclient, &mycsocklen);
        printf("Received packet %d\n",mypacket.seqnum);

        uint32_t cs =  gbnhdr_checksum(mypacket,m);

        if ((mypacket.seqnum) == expected && (mypacket.checksum == cs)) {
            ack->seqnum = expected;
            n = maybe_sendto(sockfd, ack, sizeof(ack), 0, &myclient, mycsocklen);
            printf("OK:Sending ack %d\n",expected);
            tmp = expected +1;
            if(expected==255){
                memcpy(&expected, &zero, sizeof(int));
            }
            else {
                memcpy(&expected, &tmp, sizeof(int));
            }
            memcpy(buffer, mypacket.data, sizeof(mypacket.data));
            /*Return the amount of data contained in the packet : what has been received minus the type, the seqnum and checksum.*/
            return m-4;

        } else {
            ack->seqnum = expected-1;
            n = maybe_sendto(sockfd, ack, sizeof(ack), 0, &myclient, mycsocklen);
            printf("REPEAT:Sending ack %d\n",ack->seqnum);


        }
        if ((mypacket.type) == FIN) {
            fprintf(stdout, "Closing Connection\n");
            return 0;
        }


    }

}
/**********************************************************************************************************************/
/* Close The Socket*/
/**********************************************************************************************************************/
int gbn_close(int sockfd){
    close(sockfd);
    return(0);

}
/**********************************************************************************************************************/
/* Open the connection: send a SYN packet and wait for a SYNACK packet*/
/**********************************************************************************************************************/
int gbn_connect(int sockfd, const struct sockaddr *server, socklen_t socklen){

    /*Setup connection sending a SYN*/
    /*Allocating SYN packet*/
    gbnhdr *synpacket= malloc(sizeof(gbnhdr));
    synpacket->type=SYN;
    synpacket->checksum=0;
    synpacket->seqnum=0;

    memcpy( &myserver, server, sizeof(struct sockaddr *) );
    memcpy( &mysocklen, &socklen, sizeof(socklen_t) );


    fprintf(stdout,"Sending SYN %u\n",synpacket->type);
    int n=sendto(sockfd,synpacket, sizeof(synpacket), 0, server, socklen);
    if(n==-1){
        fprintf(stdout,"ERROR: sendTo()");
    }

    /*Waiting to receive the SYNACK*/
    gbnhdr *asynpacket= malloc(sizeof(gbnhdr));
    fprintf(stdout,"Waiting  SYNACK \n");
    n=recvfrom(sockfd, asynpacket, sizeof(gbnhdr), 0,server, &socklen);
    if(asynpacket->type!=SYNACK)
        return -1;
    fprintf(stdout, "Received SYNACK %u\n", asynpacket->type);

    return(n);
}

int gbn_listen(int sockfd, int backlog){


    return(0);
}
/**********************************************************************************************************************/
/* Binds socket to client*/
/**********************************************************************************************************************/
int gbn_bind(int sockfd, const struct sockaddr *server, socklen_t socklen){

    /* TODO: Your code here. */
    fprintf(stdout,"Trying bind()\n");
    int t=bind(sockfd, server, socklen);
    if(t==-1) {
        fprintf(stdout, "ERROR: bind()");
    }

    return t;

}
/**********************************************************************************************************************/
/* Create a Socket*/
/**********************************************************************************************************************/
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
/**********************************************************************************************************************/
/* Accepts a connection by replying to a SYN packet with a SYNACK packet.*/
/**********************************************************************************************************************/
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
