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
	GNU General Public License for more details.F_memcpuy

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
#include <sys/time.h>
#include "fcntl.h"

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

#define MI_STATUS_QUEUED 0x03
#define MI_STATUS_TRANSFERRED 0x04
#define MI_STATUS_DROPPED 0x08


#define SNCC_SIGNAL_RECEIVED  0x05
#define SNCC_PREAMBLE_RECEIVED 0x06

#define MCOM_DATA_LEN 20
#define MCOM_NODE_QUEUE_LEN 10
#define MCOM_MAX_NODES 16

#define MAX_TRANSFER_ERRORS_MESSAGE  100

typedef struct _UDPMessage {

	unsigned char data [MCOM_DATA_LEN] ;
	int   destination;

} UDPMessage;


typedef struct _message {
	unsigned char data [MCOM_DATA_LEN] ;
	int   expectedChecksum;
	int   receivedChecksum;
	int   status;
	int   transferError;
	int   destination;
	unsigned long  noRetryUntil;
} message;

typedef struct _McomInPacket {
  unsigned char preamble_1;
  unsigned char preamble_2;
  unsigned word cmd;
  unsigned char destinationCmd;
  unsigned char destinationSncc;
  unsigned char __reserved_1;
  unsigned char __reserved_2;
  unsigned char data[MCOM_DATA_LEN];
  unsigned word snccCheckSum;
  unsigned word chkSum;
} McomInPacket;

typedef struct _McomOutPacket {
  unsigned char preamble_1;
  unsigned char preamble_2;
  unsigned word cmd;
  unsigned char signalMask2;
  unsigned char signalMask1;
  unsigned char __reserved_1;
  unsigned char __reserved_2;
  unsigned char data[MCOM_DATA_LEN];
  unsigned word snccCheckSum;
  unsigned word chkSum;
} McomOutPacket;

#define SIZEOF_MCOM_OUT_PAYLOAD  (MCOM_DATA_LEN+4*sizeof(char))
#define SIZEOF_MCOM_OUT_HEADER   (2*sizeof(char)+sizeof(word))
#define SIZEOF_MCOM_OUT_CHK      (2*sizeof(word))

message * outQueues[MCOM_MAX_NODES][MCOM_NODE_QUEUE_LEN]; // ten buffer pointers per device;


message inQueues [MCOM_MAX_NODES];
int     lastNodeServiced = 0;

//// server socket
struct sockaddr_in my_addr, cli_addr;
int sockfd; 
socklen_t slen=sizeof(cli_addr);

//// client socket.
int clientSocket;
struct sockaddr_in si_other;


/* functions */
void debugMessage(message * m);
void _memcpy( void* dest, void *src, int len);
int dataCheckSum (unsigned char * req, int reqLen);

/* impl */
int onMessageReceived(message * q) {

  //printf("Received\n");
  //debugMessage(q);
	sendto(clientSocket, q->data, 20, 0, (struct sockaddr* ) &si_other, slen);
}

int onMessageSent(message * q) {

//  printf("Transferred\n");

}


unsigned long getTickCount() {

	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

}

void setRetryDelay(message * q) {

 	q->noRetryUntil = getTickCount()+50;

}

int canRetry(message *q) {

	return q && (!(q->transferError) || (getTickCount() >= q->noRetryUntil));

}


// if q is null, send sncc request message.
// if sncc:
// send a packet with only sncc destination,
// try to get the id of the slave in the first buffer byte if nothing.
// add the byte corresponding to the masks to the lists snccrequests.
// wait to be recalled if necessary.


int sendMessage(message * outQueue,message * inQueues, int * pNumSNCCRequests) {

	McomInPacket pck;
	char* ppck = (char*)&pck; 
	message * inQueue = NULL;

	pck.preamble_1 = MI_PREAMBLE;
	pck.preamble_2 = MI_PREAMBLE;
	pck.cmd = MI_CMD;

	// send preamble and get the first answer
	printBuffer(ppck,SIZEOF_MCOM_OUT_HEADER);  bcm2835_spi_transfern (ppck,SIZEOF_MCOM_OUT_HEADER); printBuffer(ppck,SIZEOF_MCOM_OUT_HEADER);

	// send first bytes to preprocess, if no sncc request is pending,
	// we'll try to insert the request in this signalmask already

	preProcessSNCCmessage(&pck,inQueues,pNumSNCCRequests,&inQueue);
	ppck += SIZEOF_MCOM_OUT_HEADER;

	int checkSum;
	if (outQueue) {
		pck.destinationCmd = outQueue->destination;
		_memcpy(pck.data,outQueue->data,MCOM_DATA_LEN);
		outQueue->status = MI_STATUS_QUEUED;
		checkSum = dataCheckSum(pck.data,MCOM_DATA_LEN);
		pck.chkSum = checkSum;
	} else {
		pck.destinationCmd = 0;
		pck.chkSum = 0;
	}

	if (inQueue) {
		pck.destinationSncc = inQueue->destination;
		inQueue->status = MI_STATUS_QUEUED;
	} else {
		pck.destinationSncc = 0;
		pck.snccCheckSum = 0;
	}

	pck.__reserved_1 = 0;
	pck.__reserved_2 = 0;

	printBuffer(ppck,SIZEOF_MCOM_OUT_PAYLOAD); bcm2835_spi_transfern (ppck,SIZEOF_MCOM_OUT_PAYLOAD); printBuffer(ppck,SIZEOF_MCOM_OUT_PAYLOAD);
	ppck += SIZEOF_MCOM_OUT_PAYLOAD;

	int checkSumSNCC;
	if (inQueue) {
		checkSumSNCC = dataCheckSum(pck.data,MCOM_DATA_LEN);
		pck.snccCheckSum = checkSumSNCC;
	}

	printBuffer(ppck,SIZEOF_MCOM_OUT_CHK); bcm2835_spi_transfern (ppck,SIZEOF_MCOM_OUT_CHK);  printBuffer(ppck,SIZEOF_MCOM_OUT_CHK);

	if(outQueue) {
		if (pck.chkSum == checkSum) {
			outQueue->status = MI_STATUS_TRANSFERRED;
      onMessageSent(outQueue);
		} else {
			outQueue->transferError++;
			setRetryDelay(outQueue);
		}
	} 

	if (inQueue && (pck.snccCheckSum == checkSumSNCC)) {
		_memcpy(inQueue->data,pck.data,MCOM_DATA_LEN);
		inQueue->status = MI_STATUS_TRANSFERRED;
    onMessageReceived(inQueue);
	} 

	postProcessSNCCmessage(&pck,inQueues,inQueue,pNumSNCCRequests);      
}

/**
 * Process the pointer list for one node.
 */ 
int processNodeQueue(message ** q) {
	int i;
	// pop first valid pointer which message has been transferred 
	if ((*q) && (((*q)->status == MI_STATUS_TRANSFERRED) || ((*q)->status == MI_STATUS_DROPPED))) {
		for (i=1;i<MCOM_NODE_QUEUE_LEN;i++) {
		  *q++ = *(q+1);
		}
		*q = NULL;
	} 

}


void onMessageDropped (message *q) {

  printf("Dropped");
  debugMessage(q);

}

void dropMessageOnExcessiveErrors (message ** q) {

	if (*q) {
		if ((*q)->transferError >= MAX_TRANSFER_ERRORS_MESSAGE) {
			 (*q)->status = MI_STATUS_DROPPED;
			 onMessageDropped(*q);
			 processNodeQueue(q);
		}
	}

}



// process udp queue.
int insertNewCmds(message ** outQueues) {
 
	UDPMessage buf;
    static message m;

    if (recvfrom (sockfd, &buf, sizeof(UDPMessage), 0, (struct sockaddr*)&cli_addr, &slen) == sizeof(UDPMessage)) {
		_memcpy(m.data,&buf,sizeof(UDPMessage));
	    m.status = 0;
    	m.transferError = 0;
    	m.destination = buf.destination;
		//printf("%d\n",buf.destination);
		if (m.destination <= MCOM_MAX_NODES) {
			outQueues[(MCOM_NODE_QUEUE_LEN*3)] = &m;
		}
    	return 1;
 	}

   return 0;
}

		
// check the snccrequest queue, choose the next node to signal, if any;
int preProcessSNCCmessage(McomOutPacket* pck, message * inQueues,int * pNumSNCCRequests,message ** inQueue) {

   if (*pNumSNCCRequests) {
         // do not favor a particular node, hence loop from
   	     // lastnodeService to max node, then from 0 to lastnodeSeviced
   		int i, currentNode;
   		for (i=lastNodeServiced+1;i<(MCOM_MAX_NODES+lastNodeServiced+1);i++) {
   			currentNode = i%MCOM_MAX_NODES;
   			if (inQueues[currentNode].status == SNCC_SIGNAL_RECEIVED) {
          //printf("Signal received on node [%d]\n",currentNode);
   				*inQueue = &(inQueues[currentNode]);
   				(*inQueue)->destination = currentNode;
   			} 
   		}
   } else {
   		// nodes will squeeze their id in the preamble. 
   	 	// provided they were alone to do so, we can (if no outqueue)
   		// already inquiry them in the same packet.

   		if (pck->preamble_1) {
   			int probableNode = pck->preamble_1 & 0x7F; // strip off nofify bit
   			if (probableNode< MCOM_MAX_NODES) {
          //printf("sncc preamble received from node: [%x]\n",probableNode);
   				*inQueue = &(inQueues[probableNode]);
   				(*inQueue)->destination = probableNode;
   				(*inQueue)->status = SNCC_PREAMBLE_RECEIVED;
   			} 
   		} 
   }

}

// update snccrequest queue, in respect to what happened during transfer.
int postProcessSNCCmessage(McomOutPacket* pck,message * inQueues,message * inQueue,int * pNumSNCCRequests) {

  
	  int processedNode = 0;
	  if (inQueue) {
  

	  	 //preprocess assigned a queue, so let's check.
	  	 if (inQueue->status == MI_STATUS_TRANSFERRED) {
         // printf("SNCC Status transferred\n");
	  	 		inQueue->status = 0;
	  	 		processedNode = inQueue->destination;
	  	 		lastNodeServiced = processedNode;
	  	 }
	  }

	  // analyse signalmask.
	  unsigned int i,j = 1;

  
	  *pNumSNCCRequests = 0; // recaculate num of open sncc requests
	

	  for(i=0;i<8;i++) {
	  		if (pck->signalMask1 & j) {
        	if ((inQueue == NULL) || (i != inQueue->destination)) { // ignore signal of 'just processed' node 
	  											  // (can't (already) request another one)
	  				inQueues[i].destination = i;
     				inQueues[i].status = SNCC_SIGNAL_RECEIVED;
	  			}
	  		}
	  		j <<= 1;

	  		if (inQueues[i].status == SNCC_SIGNAL_RECEIVED) {
	  			(*pNumSNCCRequests)++;
	  		}
	  }

    

}


int main(int argc, char **argv)
{
  client();
  lstn();

  if (!bcm2835_init())
	return 1;

  bcm2835_gpio_fsel(latch, BCM2835_GPIO_FSEL_INPT); 

  bcm2835_spi_begin();
  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   
  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_1024); 
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW); 
  
  memset(outQueues,0,MCOM_MAX_NODES*MCOM_NODE_QUEUE_LEN*sizeof(message*));
  memset(inQueues,0,MCOM_MAX_NODES*sizeof(message));

  McomOutPacket r;
  int i;

  message m;
  memset(&m,0,sizeof(message));


  int           numSNCCRequests = 0;
  int           allMsgProcessed;
  McomInPacket  pck;
 

	while (1)  {

		while (!insertNewCmds((message**)outQueues)) { // give the opportunity to get new buffers from udp.
			if (bcm2835_gpio_lev(latch)||numSNCCRequests) { // either a node is requesting a sncc packet 
				//  or we have more snccRequests to service
				sendMessage(NULL,inQueues,&numSNCCRequests);
			} else {
				usleep(750);
			}
		}
		allMsgProcessed=0;

		while (!allMsgProcessed) {
			
			allMsgProcessed++;
			
			int transferErrorsPresent = 0;
			int onlyTransferErrors = 1;

			for(i=0;i<MCOM_MAX_NODES;i++) {
				message ** q = outQueues[i]; 

				if (*q != NULL) {
					if(canRetry(*q)) sendMessage(*q,inQueues,&numSNCCRequests);         
					if ((*q)->status == MI_STATUS_TRANSFERRED) {
						processNodeQueue(q); 
						onlyTransferErrors = 0;
					} else {
						transferErrorsPresent += (*q)->transferError;
            //printf("Transfererrors : %d [node: %d]",(*q)->transferError,(*q)->destination);
						dropMessageOnExcessiveErrors(q);
					}
					
					allMsgProcessed=0;
				}
			}
			if (transferErrorsPresent) {
				if (onlyTransferErrors) {
					insertNewCmds((message**)outQueues);
					usleep(750); 
				} 
			} 
		}
	}

  bcm2835_spi_end();

return 0;
}


int dataCheckSum (unsigned char * req, int reqLen) {

	// proceess chksum on reqlen, append (2B) chksum after reqLen, return checksum;
	unsigned short chk = 0;
	while (reqLen--) {
	   chk += *req++;
	}
	//chk = ~chk;
	return chk;
}

void debugMessage(message * m) {

	unsigned char data [20];

	printf("data: ");
	printBuffer2(m->data,20);
	printf("expectedChecksum: 0x%x\n",m->expectedChecksum);
	printf("receivedChecksum: 0x%x\n",m->receivedChecksum);
	printf("status: 0x%x\n",m->status);
	printf("transferError: 0x%x\n",m->transferError);
	printf("destination: 0x%x\n",m->destination);

}

int printBuffer (char * b,int size) {

  return;
  int i;
  for (i=0;i<size;i++) {
	printf("%x ",b[i]);
  }
  printf("\n");

}

int printBuffer2 (char * b,int size) {

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
	printf(str);
	printf("\n");
	//exit(1);
}
 
int lstn(void)
{


	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
	  err("socket");
	else
	  printf("Server : Socket() successful\n");
 
	fcntl(sockfd, F_SETFL, O_NONBLOCK);


	bzero(&my_addr, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(PORT);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	 
	if (bind(sockfd, (struct sockaddr* ) &my_addr, sizeof(my_addr))==-1)
	  err("bind");
	else
	  printf("Server : bind() successful\n");
 
	//close(sockfd);
	return 0;
}

int client() 
{

    
    int slen=sizeof(si_other);
    
    clientSocket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	memset((char *) &si_other, 0, sizeof(si_other));

	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(PORT+1);

	inet_aton("127.0.0.1", &si_other.sin_addr);
//    

//	close(s);
}


