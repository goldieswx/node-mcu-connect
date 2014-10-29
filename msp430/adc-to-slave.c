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


#include "msp430g2553.h"
#include <legacymsp430.h>

volatile unsigned int action=0; // use volatile when dealing with variables globally (& within interrupts)
volatile unsigned int inHeader=0;
volatile unsigned char * pbfr=0;
volatile unsigned char * pstore=0;

unsigned char bfr [20];
unsigned char store [20];

#define MOSI  BIT7 
#define MISO  BIT6
#define SCK   BIT5

#define CS_NOTIFY_MASTER  BIT2   // External Interrupt INTR = P2.2
#define CS_INCOMING_PACKET  BIT2   // Master enable the line before sending  SIG = P3.2

#define PACKET_DAC 0b10010000

#define BEGIN_SAMPLE_DAC 0x02
#define CHECK_DAC 0x01

#define MAX_ADC_CHANNELS 5
#define NUM_PORTS_AVAIL  3

const char availP1 = (BIT0|BIT1|BIT2|BIT3|BIT4);
const char availP2 = (BIT3|BIT4|BIT5|BIT6|BIT7);
const char availP3 = (BIT3|BIT4|BIT5|BIT6|BIT7);

int adcData [MAX_ADC_CHANNELS+NUM_PORTS_AVAIL];


struct ioConfig {
   unsigned char P1DIR;
   unsigned char P1ADC;
   unsigned char P1REN;
   unsigned char P1OUT;
   unsigned char P2DIR;
   unsigned char P2REN;
   unsigned char P2OUT;
   unsigned char P3DIR;
   unsigned char P3REN;
   unsigned char P3OUT;
   unsigned char pcTimerCount; 
 };

struct ioConfig ioConfig;
char ioADCRead[MAX_ADC_CHANNELS]; 

void checkDAC();

void initIOConfig() {

   ioConfig.P1DIR = 0x00;
   ioConfig.P1ADC = BIT2;
   ioConfig.P1REN = 0xFF;
   ioConfig.P1OUT = 0;
   ioConfig.P2DIR = 0x00;
   ioConfig.P2REN = 0xFF;
   ioConfig.P2OUT = 0;
   ioConfig.P3DIR = 0;
   ioConfig.P3REN = 0xFF;
   ioConfig.P3OUT = 0;

   ioConfig.P1DIR &= availP1 & ~ioConfig.P1ADC;
   ioConfig.P1ADC &= availP1;
   ioConfig.P1REN &= (availP1 & ~ioConfig.P1ADC);
   ioConfig.P1OUT &= (availP1 & ~ioConfig.P1ADC);
   ioConfig.P2DIR &= availP2;
   ioConfig.P2REN &= availP2;
   ioConfig.P2OUT &= availP2;
   ioConfig.P3DIR &= availP3;
   ioConfig.P3REN &= availP3;
   ioConfig.P3OUT &= availP3;
   
   P1DIR    = (P1DIR & (~availP1)) | ioConfig.P1DIR;
   P1SEL    = (P1SEL & (~availP1)) | ioConfig.P1ADC;
   P1SEL2   = (P1SEL2 & (~availP1));
   ADC10AE0 = ioConfig.P1ADC;
   P1REN    = (P1REN & (~availP1)) | ioConfig.P1REN;
   P1OUT    = (P1OUT & (~availP1)) | ioConfig.P1OUT;
   P2DIR    = (P2DIR & (~availP2)) | ioConfig.P2DIR;
   P2REN    = (P2REN & (~availP2)) | ioConfig.P2REN;
   P2OUT    = (P2OUT & (~availP2)) | ioConfig.P2OUT;
   P3DIR    = (P3DIR & (~availP3)) | ioConfig.P3DIR;
   P3REN    = (P3REN & (~availP3)) | ioConfig.P3REN;
   P3OUT    = (P3OUT & (~availP3)) | ioConfig.P3OUT;

   unsigned int i;
   char ioCfg = ioConfig.P1ADC;

    for (i=0;i<MAX_ADC_CHANNELS;i++) {
        ioADCRead[i] = ioCfg & 1; 
        ioCfg >>=1;  
    }

}


void resync() {

     UCA0CTL1 = UCSWRST;   
     UCB0CTL1 = UCSWRST;        // State machine in reset
     __delay_cycles(100);
     UCB0CTL0 |=  UCCKPH |  UCCKPL |  UCMSB |  UCSYNC; // 3-pin, 8-bit SPI slave
     UCB0CTL1 &= ~UCSWRST;      // Initialize USCI

     while(IFG2 & UCB0RXIFG);   // Wait ifg2 flag cleared on RX
     IE2 |= UCB0RXIE;           // Enable USCI0 RX interrupt
}

void beginSampleDac() {

    action &= ~BEGIN_SAMPLE_DAC;
    __enable_interrupt();

    ADC10CTL0 &= ~ENC;
    while (ADC10CTL1 & BUSY);             // Wait if ADC10 core is active
    ADC10SA = (unsigned int)adcData;      // Copies data in ADC10SA to unsigned int adc array
    ADC10CTL0 |= ENC + ADC10SC;           // Sampling and conversion start

    // Then wait for results in the interrupt.   
}


void initADC(){

    ADC10AE0  = 0;
    ADC10CTL0 = 0;
    ADC10CTL1 = 0;
    ADC10MEM  = 0;
    ADC10DTC0 = 0;
    ADC10DTC1 = 0;

    ADC10CTL0 = SREF_0 | ADC10SHT_2 | MSC | ADC10ON | ADC10IE;  // Vcc,Vss as ref. Sample and hold 64 cycles
    ADC10CTL1 = INCH_4 | CONSEQ_1 ;                             // Channel 3, ADC10CLK/3
    ADC10AE0  = ioConfig.P1ADC;

    ADC10DTC1 = 5;                         // 5 conversions

}

void initUSCI(){

    P1DIR |= BIT5 | BIT7;
    P1DIR &= ~BIT6;
    P1SEL |=  BIT5 | BIT6 | BIT7; 
    P1SEL2 =  BIT5 | BIT6 | BIT7;

    UCA0CTL1 = UCSWRST;   
    UCB0CTL1 = UCSWRST;                       // **Put state machine in reset**
    __delay_cycles(10);
    UCB0CTL0 |= UCCKPH   + UCCKPL + UCMSB + UCSYNC;     // 3-pin, 8-bit SPI slave
    UCB0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**

    while(IFG2 & UCB0RXIFG);                  // Wait ifg2 flag on rx  
    IE2 |= UCB0RXIE;                          // Enable USCI0 RX interrupt
    UCB0TXBUF = 0x0;                         // We do not want to ouput anything on the line

    // init flow control Pins
    P2DIR   &= ~CS_INCOMING_PACKET;
    P2OUT   &= ~CS_INCOMING_PACKET;
    P2REN   &= ~CS_INCOMING_PACKET;
    P2SEL   &= ~CS_INCOMING_PACKET;
    P2SEL2  &= ~CS_INCOMING_PACKET;
    P2IES   &= ~CS_INCOMING_PACKET;
    P2IE    = CS_INCOMING_PACKET;
    P2IFG   = 0;

    P3DIR   |= CS_NOTIFY_MASTER;
    P3OUT   &= ~CS_NOTIFY_MASTER;
    P3REN   = ~CS_NOTIFY_MASTER;
    P3SEL   = ~CS_NOTIFY_MASTER;
    P3SEL2  = ~CS_NOTIFY_MASTER;

}

void initTimer() {

    TA0R = 0;
    TA0CCR0 = 250;              // Count to this, then interrupt;  0 to stop counting
    TA0CTL = TASSEL_1 | MC_1 ;             // Clock source ACLK
    TA0CCTL1 = CCIE ;                     // Timer A interrupt enable
}

inline void startDelayedSamplingSequence() {

    TA0CTL = TASSEL_1 | MC_1;  // Enable timera (then start sampling)
    TA0CCTL1 = CCIE;

}


int main(void)
{
    WDTCTL =  WDT_ARST_1000;         // WDTPW + WDTHOLD;        
    BCSCTL1 = CALBC1_1MHZ;           // DCOCTL = CALDCO_1MHZ;
    BCSCTL2 &= ~(DIVS_3);            // SMCLK/DCO @ 1MHz
    BCSCTL3 = LFXT1S_2; 

    initIOConfig();
    initUSCI();
    initADC();
    initTimer();

    action=0;
    pbfr=bfr;

    while(1)    {
        if (!action) {
            __bis_SR_register(LPM3_bits + GIE);
        }
        __enable_interrupt(); // process pending interrupts (between actions)
        __delay_cycles(100);
        __disable_interrupt(); 
        
        if(action & BEGIN_SAMPLE_DAC) {
            beginSampleDac();
            continue;
        }
        if(action & CHECK_DAC) {
            checkDAC();
            continue;
        }
    }
}
 

void checkDAC() {

    action &= ~CHECK_DAC;
    __enable_interrupt();

    static int lastValues[5];
    static unsigned char lastP1, lastP2, lastP3;

    unsigned char newP1,newP2,newP3;
    int readValue;
    unsigned int i;
    unsigned char dataTrigger = 0;

    for (i=0;i<MAX_ADC_CHANNELS;i++) {
      if (ioADCRead[i]) {
         readValue = adcData[i];
         if (readValue < 750) {
            if ((readValue > (lastValues[i]+15)) || (readValue < (lastValues[i]-15)))
            {
                lastValues[i] = readValue;
                dataTrigger|= 0x01;
            }
         }
      }
    }

   newP1 = availP1 & (P1IN & ~ioConfig.P1DIR & ~ioConfig.P1ADC);
   newP2 = availP2 & (P2IN & ~ioConfig.P2DIR);
   newP3 = availP3 & (P3IN & ~ioConfig.P3DIR);

   if (lastP1 != newP1) { dataTrigger|= 0x02; lastP1 = newP1; }
   if (lastP2 != newP2) { dataTrigger|= 0x04; lastP2 = newP2; }
   if (lastP3 != newP3) { dataTrigger|= 0x08; lastP3 = newP3; }

  if (dataTrigger) {
    // write iodata at the end of the buffer.
    int * pAdc = &adcData[MAX_ADC_CHANNELS];
    *pAdc++ = newP1;
    *pAdc++ = newP2;
    *pAdc++ = newP3;
    P3OUT |= CS_NOTIFY_MASTER;
  } else {
    // Restart timer and sampling();
    startDelayedSamplingSequence();
  }

   return;
}



interrupt(ADC10_VECTOR) ADC10_ISR (void) { 
   
    action |= CHECK_DAC;
    ADC10CTL0 &= ~ADC10IFG;
    __bic_SR_register_on_exit(LPM3_bits + GIE); // exit LPM   
    return;

}
 
interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {


    if (pstore) { *pstore++ = UCB0RXBUF; }
    if (pbfr) { UCB0TXBUF = *pbfr++; }  

    if (inHeader) {
          //check action.
         if (UCB0RXBUF == 0x01) {
            // master asks interact with IOs
            pbfr = 0;
         }  else {
            // master asks our adc data 
            pbfr = (unsigned char *) adcData;
         }
         inHeader --;
    } 

    IFG2 &= ~UCB0RXIFG;   // clear current interrupt flag
    if (UCB0STAT & UCOE) {
      resync();
      IFG2 &= ~UCB0RXIFG;   // clear current interrupt flag
    }

    return;
}

interrupt(PORT2_VECTOR) P2_ISR(void) {


   P2IFG &= ~CS_INCOMING_PACKET;   // clear master notification interrupt flag        
   P2IES ^= CS_INCOMING_PACKET;    // invert edge select on csi

   if (P2IES & CS_INCOMING_PACKET) {  // check raising edge (xored)
        UCB0TXBUF = 0x00;           // prepare first byte
        inHeader = 1;
        pstore = store;
   } else {
      if (store[0] == 0x01) {
            // P1OUT &= (store[2] & (BIT0 + BIT1));
            // P1OUT |= (store[1] & (BIT0 + BIT1));
      } else {
        if (store[0] != 0x02) {
          resync();              
          pbfr = 0;
        }

        WDTCTL = WDTPW + WDTCNTCL; // Watchdog in countdown mode
        startDelayedSamplingSequence();
        P3OUT &= ~CS_NOTIFY_MASTER; // Clear master notification flag
      }
   }

   return;
}


interrupt(TIMER0_A1_VECTOR) ta1_isr(void) {

  TA0CTL = TACLR;  // stop & clear timer
  TA0CCTL1 &= 0;   // also disable timer interrupt & clear flag

  WDTCTL = WDTPW + WDTCNTCL;
  P3OUT &= ~CS_NOTIFY_MASTER;
  action |= BEGIN_SAMPLE_DAC;

  __bic_SR_register_on_exit(LPM3_bits + GIE); // exit LPM
  return;
} 
