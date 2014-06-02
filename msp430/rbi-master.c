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

#define word short
#define latch  RPI_GPIO_P1_22

#define MI_PREAMBLE 0b10101100
#define MI_DOUBLE_PREAMBLE 0xACAC
#define MI_CMD 8714

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
    unsigned char data [20] ;
    int  expectedChecksum;
    int  receivedChecksum;
    int  status;
    int  transferError;
    int  destination;
} message;

typedef struct _McomInPacket {
  unsigned word preamble;
  unsigned word cmd;
  unsigned char destinationCmd;
  unsigned char destinationSncc;
  unsigned char __reserved_1;
  unsigned char __reserved_2;
  unsigned char data[20];
  unsigned word snccCheckSum;
  unsigned word chkSum;
} McomInPacket;

typedef struct _McomOutPacket {
  unsigned word preamble;
  unsigned word cmd;
  unsigned char signalMask2;
  unsigned char signalMask1;
  unsigned char __reserved_1;
  unsigned char __reserved_2;
  unsigned char data[20];
  unsigned word snccCheckSum;
  unsigned word chkSum;
} McomOutPacket;


message * outQueues[32][10]; // ten buffer pointers per device;
int     snccRequest[32]; /// 1 if device requested sncc

int onMessageReceived(message * q) {
  debugMessage(q);
}


int main(int argc, char **argv)
{
  if (!bcm2835_init())
	return 1;

  bcm2835_gpio_fsel(latch, BCM2835_GPIO_FSEL_INPT); 

  bcm2835_spi_begin();
  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   
  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_1024); 
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW); 
  
  McomOutPacket r;
  while (1) 
  {
      // for i = 0 to 31;
      sendMessageToNode(m,snccRequest,&r);
      // 
  }

  bcm2835_spi_end();

return 0;
}


int process_checksum (char * req, int reqLen,int bAppend) {

    // proceess chksum on reqlen, append (2B) chksum after reqLen, return checksum;
    unsigned short chk = 0;
    unsigned short * appendPtr = (unsigned short *)(req+14);
    while (reqLen--) {
       chk += *req++;
    }
    //chk = ~chk;
    if (bAppend) {
       *appendPtr = chk; 
    }

    return chk;
}

void debugMessage(message * m) {

 unsigned char data [20] ;

    printf("data: ");
    printBuffer(m->data,20);
    printf("expectedChecksum: 0x%x\n",m->expectedChecksum);
    printf("receivedChecksum: 0x%x\n",m->receivedChecksum);
    printf("status: 0x%x\n",m->status);
    printf("transferError: 0x%x\n",m->transferError);
    printf("destination: 0x%x\n",m->destination);

}

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
