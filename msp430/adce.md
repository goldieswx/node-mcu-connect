
ADCE Module
===========

### Definition

*ADCE* stands for Analog and Digital Connection Extension. The extension connects to the *Node* with a 41-pin connector. Supply is ensured by the node. It offers 5 ADC input.  5 PWM outputs. and a total of 15 GPIO pins. 

### Hardware

The module is based on a Texas Instruments MSP430 (G2553). The package used is the TSSOP-28.
Fifteen GPIO are available on the board, among which, five can be used as 10-bit ADC, five for PWM. 

Each I/O can be pulled up and/or down directly from the hardware board enabling a lot of different applications. (Current limiting, R/C filtering, current pulling...)

### Communication

The ADCE board communicates with the *Node* with a 3-pin SPI connection. And a dual channel interrupt/signal line. Any node can support up to three ADCE modules stacked.

### Protocol

The communication protocol with the *Node* is illustrated [here](adce-protocol.txt)
