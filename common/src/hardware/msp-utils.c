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

#include "msp-utils.h"
#include "string.h"


void flash_erase(int *addr)
{
  __disable_interrupt();                             // Disable interrupts. This is important, otherwise,
                                       // a flash operation in progress while interrupt may
                                       // crash the system.
  while(BUSY & FCTL3);                 // Check if Flash being used
  FCTL2 = FWKEY + FSSEL_1 + FN3;       // Clk = SMCLK/4
  FCTL1 = FWKEY + ERASE;               // Set Erase bit
  FCTL3 = FWKEY;                       // Clear Lock bit
  *addr = 0;                           // Dummy write to erase Flash segment
  while(BUSY & FCTL3);                 // Check if Flash being used
  FCTL1 = FWKEY;                       // Clear WRT bit
  FCTL3 = FWKEY + LOCK;                // Set LOCK bit
  __enable_interrupt();
}

void flash_write(int *dest, int *src, unsigned int size)
{
  __disable_interrupt();                             // Disable interrupts(IAR workbench).
  int i = 0;
  FCTL2 = FWKEY + FSSEL_1 + FN0;       // Clk = SMCLK/4
  FCTL3 = FWKEY;                       // Clear Lock bit
  FCTL1 = FWKEY + WRT;                 // Set WRT bit for write operation
   
  for (i=0; i< size; i++)
      *dest++ = *src++;         // copy value to flash
   
 
  FCTL1 = FWKEY;                        // Clear WRT bit
  FCTL3 = FWKEY + LOCK;                 // Set LOCK bit
  __enable_interrupt();
}


void flashConfig(struct IoConfig * p) {

  WDTCTL = WDTPW + WDTHOLD; // Hold watchdog.
  struct flashConfig buffer;
  buffer.magic = 0x4573;    // Surround Buffer with magic number.
  buffer._magic = 0x7354;      

  // Copy config buffer prior to writing it in flash
  memcpy(&buffer.ioConfig,p,sizeof(struct IoConfig));

  // Do not write flash with the same existing buffer.
  if (!memcmp(&buffer,(int*)IOCFG_HW_ADDR,sizeof(struct flashConfig))) {
    return;
  }

  //Do Update the Flash
  flash_erase((int*)IOCFG_HW_ADDR);
  flash_write((int*)IOCFG_HW_ADDR,(int*)&buffer,sizeof(struct flashConfig)/sizeof(int));

  WDTCTL = WDTHOLD; // reboot.
}
