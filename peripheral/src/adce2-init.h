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
    
#define LOW_POWER_MODE(a)    (__bis_SR_register(LPM3_bits + GIE))
#define NODE_INTERRUPT 			 (P2IN & CS_INCOMING_PACKET)

#define IOCFG_HW_ADDR		0xE000

void setupUSCIPins(int state);

inline void msp430SampleInputs(struct Sample * sample);
inline void msp430ResetUSCI();
inline void msp430StopUSCI();
inline void msp430NotifyNode(int level);

inline void msp430StartTimer(int delay); 
inline void msp430BitMaskPorts (char * bitMasks, struct IoConfig * ioConfig);
inline void msp430InitializeClocks();

void initializeIOConfig(struct IoConfig * ioConfig);
void initializeUSCI();
void initializeADC(struct IoConfig * ioConfig);
void initializeTimer();

