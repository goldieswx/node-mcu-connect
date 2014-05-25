/*
    node-mcu-connect . node.js UDP Interface for embedded devices.
    Copyright (C) 2013-4 David Jakubowski

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. 
*/

#include <bcm2835.h>
#include "stdio.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h> 
#include <string.h>


#define latch  RPI_GPIO_P1_22

#define MI_HEADER_SIZE 4
#define MI_PREAMBLE 0b10101100
#define MI_CMD 8714
#define MI_ANSWER 9934

#define SNCC_CMD     12453  // Mi issues an explicit SNCC 
#define MI_CHECK     12554  // (A) Slave(s) brought up MISO, who is(are) it(they)
#define MI_UPBFR     22342
#define MI_UPBFRAM   21233
#define MI_AM        3409
#define MI_CMD_BUFFER 14587

#define CS_APPEND       1
#define NO_APPEND    0

#define MAKEINT(a,b)  ((b & 0xFF) << 8) | (a & 0xFF)  // if little endian
#define HIBYTE(a)     ((a) >> 8)
#define LOBYTE(a)     ((a) | 0xFF)

#define ST_TRANSFER 0x01
#define ST_RECEIVED 0X02

#define BIT0  0x01
#define BIT1  0x02
#define BIT2  0x04
#define BIT3  0x08
#define BIT4  0x10
#define BIT5  0x20
#define BIT6  0x40
#define BIT7  0x80

#define BUFLEN 512
#define PORT 9930



typedef struct _message {
    char request [16] ;
    char response [16] ;
    int  expectedChkRequest;
    int  receivedChkResponse;
    int  requestLen;
    int  responseLen;
    int  transferred; // len of transferred
    int  status;
    int  lastOp;
    int  transferError;
    int  destination;
    int  slotSent; // slot in 64b buffer (1,2 or3)
} message;

 char d[64]; 

int printBuffer (char * b,int size) {

  int i;
  for (i=0;i<size;i++) {
    printf("%x ",b[i]);
  }
  printf("\n");

}

void zeroMem(void * p,int len) {
     
     while (len--) {
        *(unsigned char*)p++ = 0x00;
     }
}


void _memcpy( void* dest, void *src, int len) {

  while (len--) {
       *(char*)dest++ = *(char*)src++;
  }
}

void comm_cmd_header (message* q,int qLen) {

   // SEND MI_CMD HEADER
   // Rest of the cmd follows
   zeroMem(d,64);
   d[0] = MI_PREAMBLE;
   d[1] = MI_PREAMBLE; 
   *(unsigned short *) (&d[2]) = MI_CMD; 

   process_checksum(d,14,CS_APPEND);

   int i;
   for(i=0;i<qLen;i++){
        if (q[i].status == ST_TRANSFER) {
           q[i].lastOp = MI_CMD;  
        }
   }
 
   printBuffer(d,16);
   bcm2835_spi_transfern (d,16);
   printBuffer(d,16);

}

int process_checksum (char * req, int reqLen,int bAppend) {

    // proceess chksum on reqlen, append (2B) chksum after reqLen, return checksum;
    unsigned short chk = 0;
    unsigned short * appendPtr = (unsigned short *)(req+14);
    while (reqLen--) {
       chk += *req++;
    }
    chk = ~chk;
    if (bAppend) {
       *appendPtr = chk; 
    }

    return chk;
}

void comm_cmd_buffer (message * q,int qLen) {

   // SEND CMDs to devices (0x01, 0x02, 0x03)
   // no MISO is expected, so just don't care if we get bits in there.
   d[0] = MI_PREAMBLE; 
   d[1] = MI_PREAMBLE; 
   *(unsigned short *) (&d[2]) = MI_CMD_BUFFER; 

/*
   d[1] = 0x01;
   d[2] = 0x02;
   d[3] = 0x03;

   d[4] = 0x01;
   d[5] = 0x01;
   d[20] = 0x01;
   d[21] = 0x01;
   d[32] = 0x01;
   d[33] = 0x01;
*/

   int i;
   int j = 0;
   for(i=0;i<qLen;i++){
        j ++;
        if (q[i].status == ST_TRANSFER) {
           int reqLen = q[i].requestLen;
           q[i].expectedChkRequest = process_checksum(q[i].request,reqLen,CS_APPEND);
           q[i].lastOp = MI_PREAMBLE;
           
           // prepare spi send buffer
           d[i+4] = q[i].destination;
           _memcpy(&d[j*16],&q[i].request,16); 
           q[i].slotSent = j;
        }
   }

   process_checksum(d,14,CS_APPEND);

   printBuffer(d,16);
   printBuffer(d+16,16);
   printBuffer(d+32,16);
   printBuffer(d+48,16);

   bcm2835_spi_transfern (d,64);

}


void comm_cmd_resp_buffer (message * q,int * qLen,message * outQueue,int outQueueLen) {

   zeroMem(d,64);
   d[0] = MI_PREAMBLE;
   d[1] = MI_PREAMBLE; 
   *(unsigned short *) (&d[2]) = MI_ANSWER; 

   process_checksum(d,14,CS_APPEND);


   // get responses.
   printBuffer(d,16);
   printBuffer(d+16,16);
   printBuffer(d+32,16);
   printBuffer(d+48,16);  
   bcm2835_spi_transfern (d,64);
   printBuffer(d,16);
   printBuffer(d+16,16);
   printBuffer(d+32,16);
   printBuffer(d+48,16);  


  int i;
  for(i=0;i<*qLen;i++){
    if (q[i].status == ST_TRANSFER) {
        _memcpy(&(q[i].response),d+((q[i].slotSent)*16),16);

        int chkRequestToCompare = MAKEINT(q[i].response[14],q[i].response[15]); // request checksum repeated
        if (chkRequestToCompare != q[i].expectedChkRequest) {
          q[i].transferError++;
          //log error (q[i],transfererror), request checksum failed recheck on reply
          printf("log error (q[i],transfererror), request checksum failed recheck on reply\n");
          return;
        }
        q[i].receivedChkResponse = MAKEINT(q[i].response[12],q[i].response[13]);  // response checksum 
   
        int chqResponseToCompare = process_checksum(q[i].response,12,NO_APPEND);
        if (q[i].receivedChkResponse != chqResponseToCompare) {
          q[i].transferError++;
          printf("log error (q[i],transfererror), response checksum failed check\n");
          //log error (q[i],transfererror), response checksum failed check
          return;
        }
        // log ok pckt exchanged successfully. log (q[i]);
        q[i].status = ST_RECEIVED;
        _memcpy(&outQueue[outQueueLen++],&(q[i]),sizeof(message));
    }
  } 
 
}

void process_queue_post_transfer(message * inQueues, message ** inQueue, int* inLen) {

     message * srcQueue = *inQueue;
     int srcLen = *inLen;
     int i;

     // swap dest queue
     message * destQueue = inQueues;
     if (srcQueue == inQueues) {
         destQueue += 4;
     }

     int destLen = 0;

     for (i=0;i<srcLen;i++) {
        if (srcQueue[i].status == ST_TRANSFER) {
             _memcpy(&destQueue[destLen++],&srcQueue[i],sizeof(message));
        }
     }

     *inQueue = destQueue;
     *inLen = destLen;

}

void comm_sncc_header() {

 // sets in SNCC
   printf("Staring SNCC\n");

   d[0] = MI_PREAMBLE;
   d[1] = 0; 
   *(unsigned short *) (&d[2]) = SNCC_CMD; 
  
   printBuffer(d,4); 
   bcm2835_spi_transfern (d,4);
   printBuffer(d,4); 

}

void comm_sncc_id_mask_header() {

   // ask node to prepare buffer whether they sent the message
   printf("MI_CHECK\n");

   d[0] = MI_PREAMBLE;
   d[1] = 0; 
   *(unsigned short *) (&d[2]) = MI_CHECK; 
    
   printBuffer(d,4); 
   bcm2835_spi_transfern (d,4);
   printBuffer(d,4); 

}

void comm_sncc_id_mask_sendids_header() {

  // when first time master call this nodes, after the header
  // nodes should set their id in the binary mask.
  // also used in MOSI to signal which node are going to respond
  // in the next upid buffer  
  printf("SEND IDS\n");

   d[0] = MI_PREAMBLE;
   d[1] = 0; 
   d[2] = 2;
   d[3] = 0;   

   bcm2835_spi_transfern (d,4);

}


void comm_sncc_sendids_buffer() {

   printf("MI_UPBFR\n");

   d[0] = MI_PREAMBLE;
   d[1] = 0; 
   *(unsigned short *) (&d[2]) = MI_UPBFR; 
  
   bcm2835_spi_transfern (d,64);
}


void debugQueue(message * q) {

    printf("request : ");
    printBuffer(q->request,16);
    printf("response : ");
    printBuffer(q->response,16);
    printf("expectedChkRequest : 0x%x \n",q->expectedChkRequest);
    printf("receivedChkResponse : 0x%x \n",q->receivedChkResponse);
    printf("requestLen : 0x%x \n",q->requestLen);
    printf("responseLen : 0x%x \n",q->responseLen);
    printf("transferred : 0x%x \n",q->transferred);
    printf("status : 0x%x \n",q->status);
    printf("lastOp : %d \n",q->lastOp);
    printf("transferError : 0x%x \n",q->transferError);
    printf("destination : 0x%x \n",q->destination);
    printf("slotSent : %d\n",q->slotSent);

}

void debugQueues(message * q,int len) {

    while (len--) {
       debugQueue(q++);
    }

}


int main(int argc, char **argv)
{
    if (!bcm2835_init())
  	return 1;

    bcm2835_gpio_fsel(latch, BCM2835_GPIO_FSEL_INPT); 

    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      // The default
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   // The default
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_1024); // The default
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      // The default
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW); 

   
    message  inQueues[2][4];
    message   outQueue[100];
    int   outQueueLen = 0; 

    message * inQueue = inQueues[0];
    int   numQueues = 2;
    int   inLen=0;

   /////
    
   int flipFlop = BIT7;
  
   while (1) 
   {

      if (!inLen) {
      inLen++;
      zeroMem(&inQueue[0],sizeof(message));
      inQueue[0].request[0] = ~(BIT6+BIT7);
      inQueue[0].request[1] = flipFlop;
      inQueue[0].request[2] = 0x34;
      flipFlop ^= (BIT6+BIT7);

      inQueue[0].response[0] = 0xF7;
      inQueue[0].expectedChkRequest = 0xFF;
      inQueue[0].receivedChkResponse = 0xF7;
      inQueue[0].requestLen = 3;
      inQueue[0].responseLen = 0;
      inQueue[0].transferred = 0; // len of transferred
      inQueue[0].status = ST_TRANSFER;
      inQueue[0].lastOp = 0;
      inQueue[0].transferError = 0;
      inQueue[0].destination = 0x02;
    }
      usleep(1000);

     if (inLen && (outQueueLen < sizeof(outQueue)-4)) {
      comm_cmd_header(inQueue,inLen); // 4B transfer
      debugQueues(inQueue,inLen);

      usleep(100);

      comm_cmd_buffer(inQueue,inLen); // 64B transfer
      debugQueues(inQueue,inLen);

      usleep(500);

      comm_cmd_resp_buffer(inQueue,&inLen,outQueue,outQueueLen); // 64B transfer
      debugQueues(inQueue,inLen);

      usleep(500);

      printf("bfr1 ptr inQueue: 0x%x  len:%d\n",inQueue,inLen);
      process_queue_post_transfer(&(inQueues[0][0]),&inQueue,&inLen); // remove all transferred from queue. 

      printf("bfr2 ptr inQueue: 0x%x len:%d\n",inQueue,inLen);

      debugQueues(inQueue,inLen);


      
   }
    

    
  /*   comm_sncc_header(); // 4B transfer
     usleep(250);

    // wait slave message if any
    while (!bcm2835_gpio_lev(latch))
    {
       usleep(750);
       if (inLen) { break; }
    }
    usleep(1000);

    comm_sncc_id_mask_header(); // 4B transfer
    usleep(1000);

    // while all is not treated // 
    comm_sncc_id_mask_sendids_header (); // 4B
    usleep(1000);

    comm_sncc_sendids_buffer(); // 64B transfer
    usleep(1000); */
    //  while ///
  }

bcm2835_spi_end();

return 0;
}

 
void err(char *str)
{
    perror(str);
    exit(1);
}
 
int lstn(void)
{
    struct sockaddr_in my_addr, cli_addr;
    int sockfd, i; 
    socklen_t slen=sizeof(cli_addr);
    char buf[BUFLEN];
 
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
      err("socket");
    else
      printf("Server : Socket() successful\n");
 
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
     
    if (bind(sockfd, (struct sockaddr* ) &my_addr, sizeof(my_addr))==-1)
      err("bind");
    else
      printf("Server : bind() successful\n");
 
    while(1)
    {
        if (recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr*)&cli_addr, &slen)==-1)
            err("recvfrom()");
        printf("Received packet from %s:%d\nData: %s\n\n",
               inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), buf);
    }
 
    close(sockfd);
    return 0;
}
