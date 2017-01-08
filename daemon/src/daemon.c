/*
	node-mcu-connect . node.js UDP Interface for embedded devices.
	Copyright (C) 2013-5 David Jakubowski

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
#include "fifo.h"

#include "stdint.h"
#include "network.h"
#include "daemon.h"

struct message * outQueues[MCOM_MAX_NODES][MCOM_NODE_QUEUE_LEN]; // ten buffer pointers per device;

struct message inQueues [MCOM_MAX_NODES];
int     lastNodeServiced = 0;

//// server socket
struct sockaddr_in my_addr, cli_addr;
int sockfd; 
socklen_t slen=sizeof(cli_addr);

//// client socket.
int clientSocket;
struct sockaddr_in si_other;
fifo_t * outQueuePool;



/* impl */
int onMessageReceived(struct message * q) {

  //printf("Received\n");
  //debugMessage(q);
	sendto(clientSocket, q, sizeof(struct message), 0, (struct sockaddr* ) &si_other, slen);
}

int onMessageSent(struct message * q) {

//  printf("Transferred\n");

}

fifo_t * initOutQueuePool() {

	fifo_t * pool = fifo_new();
	int i;
	for(i=0;i<20;i++) {
		struct message * m = (struct message*)malloc(sizeof(struct message));
		m->status = 0;
    	m->transferError = 0;
    	m->destination = 0;
		fifo_add(pool,m);
	}
	return pool;

}


unsigned long getTickCount() {

	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

}

void setRetryDelay(struct message * q) {

 	q->noRetryUntil = getTickCount()+50;

}

int canRetry(struct message *q) {

	return q && (!(q->transferError) || (getTickCount() >= q->noRetryUntil));

}


// if q is null, send sncc request message.
// if sncc:
// send a packet with only sncc destination,
// try to get the id of the slave in the first buffer byte if nothing.
// add the byte corresponding to the masks to the lists snccrequests.
// wait to be recalled if necessary.


void bcm2835_spi_transfern2 (unsigned char * p,int count) {

      bcm2835_spi_transfern (p,count);
      int i = 0;

      for (i=0;i<count;i++) {
         p[i] ^= 0xFF;
     }


}

int sendMessage(struct message * outQueue,struct message * inQueues, int * pNumSNCCRequests) {

	struct McomInPacket pck;
	char* ppck = (char*)&pck; 
	struct message * inQueue = NULL;

	pck.preamble.i16 = MI_DOUBLE_PREAMBLE;

	pck.cmd = MI_CMD;
     //  printf("HDR:\n");
	// send preamble and get the first answer
	//printBuffer2(ppck,SIZEOF_MCOM_OUT_HEADER);
	bcm2835_spi_transfern2 (ppck,SIZEOF_MCOM_OUT_HEADER);
	//printf("-");
	//printBuffer2(ppck,SIZEOF_MCOM_OUT_HEADER);

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
        printf("PL:\n");
	//printBuffer2(ppck,SIZEOF_MCOM_OUT_PAYLOAD);
	//printf("-");
	bcm2835_spi_transfern2 (ppck,SIZEOF_MCOM_OUT_PAYLOAD);
	//printBuffer2(ppck,SIZEOF_MCOM_OUT_PAYLOAD);
	ppck += SIZEOF_MCOM_OUT_PAYLOAD;

	int checkSumSNCC;
	if (inQueue) {
		checkSumSNCC = dataCheckSum(pck.data,MCOM_DATA_LEN);
		pck.snccCheckSum = checkSumSNCC;
	}

        // printf("CHK:\n");
	    //printBuffer2(ppck,SIZEOF_MCOM_OUT_CHK);
	    // printf("-");
	    bcm2835_spi_transfern2 (ppck,SIZEOF_MCOM_OUT_CHK);
	    //printBuffer2(ppck,SIZEOF_MCOM_OUT_CHK);
        usleep(12000);

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
int processNodeQueue(struct message ** q) {
	int i;
	// pop first valid pointer which message has been transferred 
	if ((*q) && (((*q)->status == MI_STATUS_TRANSFERRED) || ((*q)->status == MI_STATUS_DROPPED))) {
		
		fifo_add(outQueuePool,*q);
		//printf("Adding pool (%d) (%x) -- index %x\n",fifo_len(outQueuePool),*q,q);

		for (i=1;i<MCOM_NODE_QUEUE_LEN;i++) {
		  *q++ = *(q+1);
		}
		*q = NULL;
	} 

}


void onMessageDropped (struct message *q) {

  //printf("Dropped");
  debugMessage(q);

}

void dropMessageOnExcessiveErrors (struct message ** q) {

	if (*q) {
		if ((*q)->transferError >= MAX_TRANSFER_ERRORS_MESSAGE) {
			 (*q)->status = MI_STATUS_DROPPED;
			 onMessageDropped(*q);
			 processNodeQueue(q);
		}
	}

}



// process udp queue.
int insertNewCmds(struct message ** outQueues) {
 
	struct UDPMessage buf;
    //static message m;
	struct message * m;

    if (recvfrom (sockfd, &buf, sizeof(struct UDPMessage), 0, (struct sockaddr*)&cli_addr, &slen) == sizeof(struct UDPMessage)) {
		if (buf.destination <= MCOM_MAX_NODES) {
			int i,index = (MCOM_NODE_QUEUE_LEN*buf.destination);
			struct message * q;
			for (i=0;i<MCOM_NODE_QUEUE_LEN;i++) {
				if (outQueues[index]== NULL) {
				  	m = fifo_remove(outQueuePool);
		  			//printf("Got from (%d) (%x) index: %x\n",fifo_len(outQueuePool),m,&outQueues[index]);
					if (!m) { 
					//	printf("Got empty queue\n");
						//onMessageDropped(NULL); 
						return 0; 
					}
					_memcpy(m->data,&buf,sizeof(struct UDPMessage));
	    			m->status = 0;
    				m->transferError = 0;
    				m->destination = buf.destination;
					outQueues[index] = m;
					//printf("Got here\n");
					return 1;
				}
				index++;
			}
		}
 	}
    return 0;
}

		
// check the snccrequest queue, choose the next node to signal, if any;
int preProcessSNCCmessage(struct McomOutPacket* pck, struct message * inQueues,int * pNumSNCCRequests,struct message ** inQueue) {

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

   		if (pck->preamble.i8[0]) {
   			int probableNode = pck->preamble.i8[0] & 0x7F; // strip off nofify bit
   			if (probableNode && probableNode< MCOM_MAX_NODES) {
          //printf("sncc preamble received from node: [%x]\n",probableNode);
   				*inQueue = &(inQueues[probableNode]);
   				(*inQueue)->destination = probableNode;
   				(*inQueue)->status = SNCC_PREAMBLE_RECEIVED;
   			} 
   		} 
   }

}

// update snccrequest queue, in respect to what happened during transfer.
int postProcessSNCCmessage(struct McomOutPacket* pck,struct message * inQueues,struct message * inQueue,int * pNumSNCCRequests) {

  
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
	

	  for(i=0;i<(8*sizeof(signalmask_t));i++) {
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
  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_4096); 
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW); 
  
  memset(outQueues,0,MCOM_MAX_NODES*MCOM_NODE_QUEUE_LEN*sizeof(struct message*));
  memset(inQueues,0,MCOM_MAX_NODES*sizeof(struct message));

  struct McomOutPacket r;
  int i;

  struct message m;
  memset(&m,0,sizeof(struct message));

  printf("init pool\n");
  outQueuePool = initOutQueuePool();
  printf("done init pool\n");


  int           numSNCCRequests = 0;
  int           allMsgProcessed;
  struct McomInPacket  pck;
 

	while (1)  {

		while (!insertNewCmds((struct message**)outQueues)) { // give the opportunity to get new buffers from udp.
			if ((!bcm2835_gpio_lev(latch))||numSNCCRequests) { // either a node is requesting a sncc packet 
				//  or we have more snccRequests to service
				sendMessage(NULL,inQueues,&numSNCCRequests);
			} else {
				usleep(1500);
			}
		}
		allMsgProcessed=0;

		while (!allMsgProcessed) {
			
			allMsgProcessed++;
			
			int transferErrorsPresent = 0;
			int onlyTransferErrors = 1;

			for(i=0;i<MCOM_MAX_NODES;i++) {
				struct message ** q = outQueues[i]; 

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
					insertNewCmds((struct message**)outQueues);
					usleep(1500); 
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
	   chk =  crc16(chk, *req++);  
	}	
	//chk = ~chk;
	return chk;
}

void debugMessage(struct message * m) {

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
 return;
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


