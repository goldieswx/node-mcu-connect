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

#define IOCFG_HW_ADDR       0xE000

struct IoConfig {
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
        unsigned char P2ADC;
        unsigned char P3ADC;
};

struct flashConfig {
        unsigned int  magic;
        struct IoConfig ioConfig;
        unsigned int  _magic;
};



void flash_erase(int *addr);
void flash_write(int *dest, int *src, unsigned int size);
void flashConfig(struct IoConfig * p);
