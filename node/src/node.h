/*
		node-mcu-connect . node.js UDP worderface for embedded devices.
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

#include "inttypes.h"
#include "network.h"

#define LONG uint32_t 
#define word uint16_t

//#ifndef nodeId
//	#define nodeId 					5
//#endif

#define MASTER_REQUEST_SNCC(pc) \
				(((pc)->dataIn.destinationSncc == nodeId) && ((pc)->signalMaster))

#define MASTER_SENDING_MICMD(pc) \
				(((pc)->dataIn.destinationCmd == nodeId) && (!(pc)->masterInquiryCommand))

#define DO_CHECKSUM(pc,rx) \
		       { (pc)->dataOut.chkSum =  crc16((pc)->dataOut.chkSum, rx); }

//				((pc)->dataOut.chkSum += (rx))


#define PTR_END_OF_HEADER               (2)     // Header ends after preamble
#define PTR_END_OF_DESTINATION          (PTR_END_OF_HEADER + 4 + padding_t_len + sizeof(signalmask_t))     // Desgination ends after destinationSNCC
#define PTR_END_OF_RESERVED             (PTR_END_OF_DESTINATION + 1)     
#define PTR_END_OF_CHECKSUM_PRE         (PTR_END_OF_DESTINATION  + 20)
#define PTR_END_OF_CHECKSUM             (PTR_END_OF_DESTINATION  + 21)
#define PTR_END_OF_PACKET               (PTR_END_OF_CHECKSUM + 2) // out lags 2 bytes behind in (therefore 4-2 = 2).
#define PTR_END_OF_PACKET2              (PTR_END_OF_CHECKSUM + 4) // out lags 2 bytes behind in (therefore 4-2 = 2).

#define MOSI  BIT7 
#define MISO  BIT6
#define SCK   BIT5

typedef void (*Inspector) ( word rx, void  * packetContainer);

struct PacketContainer {

		word                                     pointer;
		// Number from 0 to sizeof(McomInPacket)-1 representing the byte number being inspected.
		word                                     incomplete;
		// When cleared, a packet has been sucessfully received from master.
		word                                     synced;
		// When set, the SPI stream is potentially in sync. (when checksumCount is set, it is fully in sync.)
		Inspector                               inspect;
		// Function pointer to the next/current packet inspector.
		word                                     validChecksumCount;
		word                                     signalMaster;
		// When set, calls in a SNCC.
		word                                     masterInquiryCommand;
		// When set means, the inBuffer needs to be transferred to an ADCE.
		word                                     startMICMDCheckSum;
		// When set, start Checksumming rx (MICMDs)

		word                                     outBuffer[10];
		// Buffer to write to master (SNCCs)
		word                                     outBufferChecksum;
		// When writing SNCCs, checksum of the outBuffer
		word                                     inBuffer[10];
		// Buffer to write to ADCE

		struct McomInPacket     dataIn;
		word __padding; // output padding must be 0 (used when tx overflows)
		// Raw data In
		struct McomOutPacket    dataOut;
		// Raw data Out
		word ___padding; // output padding must be 0 (used when tx overflows)
		unsigned char *                 pIn;
		// Floating Pointr to dataIn
		unsigned char *                 pOut;
		// Floating Pointer to dataOut
		word                                     initialized;
		// This structure needs to be initialized.
		word                                     transmissionErrors;
		// clear  masterInquiryCommand bit on next packet end.
		word 								clearMasterInquiry;
		word *								pClearLP; // clear low power flag on rx interrupt
		word 								setSignalMaster;

};


word             initializePacketContainer(struct PacketContainer * packetContainer);

word             incrementPacket(const register word rx, struct PacketContainer  * packetContainer);
inline void     rescue (const register word rx, struct PacketContainer * packetContainer);
inline word      inspectAndIncrement(const register word rx, word* pClearLP);

void            endOfHeaderEvent        (const register word rx, struct PacketContainer * packetContainer);
void            endOfDestinationEvent   (const register word rx, struct PacketContainer * packetContainer);
void            endOfPacketEvent        (const register word rx, struct PacketContainer * packetContainer);
void            endOfReservedEvent      (const register word rx, struct PacketContainer * packetContainer);
void 			endOfPacketEvent2       (const register word rx, struct  PacketContainer * packetContainer);
void            packetCheckSumEvent     (const register word rx, struct PacketContainer * packetContainer);
void 			packetPreCheckSumEvent        (const register word rx,  struct  PacketContainer * packetContainer);


void            noActionEvent           (const register word rx, const struct PacketContainer * packetContainer);
void            prepareNextPacketCycle   (struct PacketContainer* packetContainer);

void 			initGlobal();
void 			initializeUSCI();
word 			transfer(word s);
void 			clearMasterInquiry (struct PacketContainer * packetContainer);
struct 			PacketContainer * getPacketContainer(struct PacketContainer * set);

void 			adceSetTrigger(int state, int adceId);
int 			adceSignalCmd(struct PacketContainer * p); 
int 			adceSignalTriggerAny();
void 			adceServiceTrigger(struct PacketContainer * p, int adceId);
int 			adceService(unsigned char * adceIn, unsigned char * adceOut, int adceId);
void 			adceServiceCmd(struct PacketContainer * p, int adceId);
