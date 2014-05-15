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

int printBuffer (char * b,int size) {

  int i;
  for (i=0;i<size;i++) {
    printf("%x ",b[i]);
  }
  printf("\n");

}


int main(int argc, char **argv)
{
  
   char buf[3];

    if (!bcm2835_init())
  	return 1;
    int channel = 0;
    bcm2835_gpio_fsel(latch, BCM2835_GPIO_FSEL_INPT); 
//    bcm2835_gpio_afen(latch);
//    bcm2835_gpio_aren(latch);   	

    char d[64];


    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      // The default
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   // The default
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_1024); // The default
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      // The default
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW); 

int i = 0;
  

void comm_cmd_header () {

   // SEND MI_CMD HEADER
   // Rest of the cmd follows.

   d[0] = MI_PREAMBLE;
   d[1] = 64; 
   *(unsigned short *) (&d[2]) = MI_CMD; 

   printBuffer(d,4);
   bcm2835_spi_transfern (d,4);
   printBuffer(d,4);

}

void comm_cmd_buffer () {

   // SEND CMDs to devices (0x01, 0x02, 0x03)
   d[0] = MI_PREAMBLE; 

   d[1] = 0x01;
   d[2] = 0x02;
   d[3] = 0x03;

   d[4] = 0x01;
   d[5] = 0x01;
   d[20] = 0x01;
   d[21] = 0x01;
   d[32] = 0x01;
   d[33] = 0x01;

   bcm2835_spi_transfern (d,64);

}

void comm_cmd_resp_buffer () {

   // get responses.
   bcm2835_spi_transfern (d,64);
   printBuffer(d,64);

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
   printBuffer(d,4); 
   bcm2835_spi_transfern (d,4);
   printBuffer(d,4); 


}


void comm_sncc_sendids_buffer() {

   printf("MI_UPBFR\n");

   d[0] = MI_PREAMBLE;
   d[1] = 0; 
   *(unsigned short *) (&d[2]) = MI_UPBFR; 
  
   printBuffer(d,4); 
   bcm2835_spi_transfern (d,64);
   printBuffer(d,64); 


}

  while (1) 
  {
     comm_cmd_header(); // 4B transfer
     usleep(100);

     comm_cmd_buffer (); // 64B transfer
     usleep(500);

     comm_cmd_resp_buffer (); // 64B transfer
     usleep (500);

     comm_sncc_header(); // 4B transfer
     usleep(250);

      // wait slave message if any
      while (!bcm2835_gpio_lev(latch))
      {
         usleep(750);
      }
      
      usleep(1000);

     comm_sncc_id_mask_header(); // 4B transfer
     usleep(1000);

     // while all is not treated // 
       comm_sncc_id_mask_sendids_header (); // 4B
       usleep(1000);

       comm_sncc_sendids_buffer(); // 64B transfer
       usleep(1000);
     //  while ///
  }

bcm2835_spi_end();

return 0;
}
