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

#define CS_APPEND       1
#define NO_APPEND    0

#define MAKEINT(a,b)  (((a) << 8) | (b))   // if little endian
#define HIBYTE(a)     ((a) >> 8)
#define LOBYTE(a)     ((a) | 0xFF)

#define ST_TRANSFER 0x01
#define ST_RECEIVED 0X02

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
} message;

 char d[64]; 

int printBuffer (char * b,int size) {

  int i;
  for (i=0;i<size;i++) {
    printf("%x ",b[i]);
  }
  printf("\n");

}


void _memcpy( void* dest, void *src, int len) {

  while (len--) {
       *(char*)dest++ = *(char*)src++;
  }
}

void comm_cmd_header (message* q,int qLen) {

   // SEND MI_CMD HEADER
   // Rest of the cmd follows
   d[0] = MI_PREAMBLE;
   d[1] = 64; 
   *(unsigned short *) (&d[2]) = MI_CMD; 

   int i;
   for(i=0;i<qLen;i++){
        if (q[i].status == ST_TRANSFER) {
           q[i].lastOp = MI_CMD;  
        }
   }
 
   //printBuffer(d,4);
   bcm2835_spi_transfern (d,4);
   //printBuffer(d,4);

}

int process_checksum (char * req, int reqLen,int bAppend) {

    // proceess chksum on reqlen, append (2B) chksum after reqLen, return checksum;
    unsigned short chk = 0;
    while (reqLen--) {
       chk ^= *req++;
    }
    if (bAppend) {
       *((unsigned short *)req) = chk; 
    }

    return chk;
}

void comm_cmd_buffer (message * q,int qLen) {

   // SEND CMDs to devices (0x01, 0x02, 0x03)
   // no MISO is expected, so just don't care if we get bits in there.
   d[0] = MI_PREAMBLE; 
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
        j += 16;
        if (q[i].status == ST_TRANSFER) {
           int reqLen = q[i].requestLen;
           q[i].expectedChkRequest = process_checksum(q[i].request,reqLen,CS_APPEND);
           q[i].lastOp = MI_PREAMBLE;
           
           // prepare spi send buffer
           d[i+1] = q[i].destination;
           _memcpy(&d[j],&q[i].request,16); 

        }
   }

   bcm2835_spi_transfern (d,64);

}


void comm_cmd_resp_buffer (message * q,int * qLen,message * outQueue,int outQueueLen) {

   // get responses.
   bcm2835_spi_transfern (d,64);

  int i,j=0;
  for(i=0;i<*qLen;i++){
    if (q[i].status == ST_TRANSFER) {
        j+=16;
        _memcpy(&(d[j]),&(q[i]).response,16);

        int chkRequestToCompare = MAKEINT(q[i].response[13],q[i].response[14]); // request checksum repeated
        if (chkRequestToCompare != q[i].expectedChkRequest) {
          q[i].transferError++;
          //log error (q[i],transfererror), request checksum failed recheck on reply
          return;
        }
        q[i].receivedChkResponse = MAKEINT(q[i].request[14],q[i].request[15]);  // response checksum 
   
        int chqResponseToCompare = process_checksum(q[i].response,12,NO_APPEND);
        if (q[i].receivedChkResponse != chqResponseToCompare) {
          q[i].transferError++;
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
         destQueue += 4*sizeof(message);
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

  while (1) 
  {

     if (inLen && (outQueueLen < sizeof(outQueue)-4)) {
       comm_cmd_header(inQueue,inLen); // 4B transfer
       usleep(100);

       comm_cmd_buffer (inQueue,inLen); // 64B transfer
       usleep(500);

       comm_cmd_resp_buffer (inQueue,&inLen,outQueue,outQueueLen); // 64B transfer
       usleep (500);
 
       process_queue_post_transfer(&(inQueues[0][0]),&inQueue,&inLen); // remove all transferred from queue. 
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
