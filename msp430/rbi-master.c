

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
  

while (1) 
{

   d[0] = MI_PREAMBLE;
   d[1] = 64; 
   *(unsigned short *) (&d[2]) = MI_CMD; 

   printBuffer(d,4);
   bcm2835_spi_transfern (d,4);
   printBuffer(d,4);
  
   usleep(100);

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

   usleep(500);

   bcm2835_spi_transfern (d,64);
 
   printBuffer(d,64);

   // sets in SNCC

   usleep (500);

   printf("Staring SNCC\n");

   d[0] = MI_PREAMBLE;
   d[1] = 0; 
   *(unsigned short *) (&d[2]) = SNCC_CMD; 
   
   printBuffer(d,4); 
   bcm2835_spi_transfern (d,4);
   printBuffer(d,4); 

   usleep(250);

   while (!bcm2835_gpio_lev(latch))
   {
     usleep(750);
   }
   
   usleep(1000);

   printf("MI_CHECK\n");

   d[0] = MI_PREAMBLE;
   d[1] = 0; 
   *(unsigned short *) (&d[2]) = MI_CHECK; 
    
   printBuffer(d,4); 
   bcm2835_spi_transfern (d,4);
   printBuffer(d,4); 

   usleep(1000);

   printf("SEND IDS\n");

   d[0] = MI_PREAMBLE;
   d[1] = 0; 
   d[2] = 2;
   d[3] = 0;   

 
   printBuffer(d,4); 
   bcm2835_spi_transfern (d,4);
   printBuffer(d,4); 

   usleep(1000);

   printf("MI_UPBFR\n");

   d[0] = MI_PREAMBLE;
   d[1] = 0; 
   *(unsigned short *) (&d[2]) = MI_UPBFR; 
  
   printBuffer(d,4); 
   bcm2835_spi_transfern (d,64);
   printBuffer(d,64); 

   usleep(1000);
}

   bcm2835_spi_end();

   return 0;
}
