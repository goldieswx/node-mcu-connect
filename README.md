node-mcu-connect 
================

nodeMcuConnect is an open source framework whose goal is to setup an ultra low power, low latency microcontroller network controller (..matter of taste but I hate the IoT terminology).

The network is composed of nodes connected and powered through a RS-422/485 multipoint interface, handled by a master node.

- A comprehensive network controller is available in javascript (now through node.js) and communicates bi-directionally with a deamon which serves low level communication tasks.
- Master node deamon (written in C).
- Nodes supports up to three extensions (bi-directional and SPI wired to the node) which can be addressed through the controller. (written in javaScript)
- Code for msp430g2x nodes devices and extensions is available, hardware dependent instructions decoupled.  (written in C)

- Wiring schematics ready but not yet available.

This project is open source, licensed under GPLv3 terms (http://www.gnu.org/licenses/).




### Hardware ###

There is currently one hardware implementation (TI MSP430G2x value line). 
We'll call the Quinoha implementation.

The codebase however is well decoupled from hardware and may be ported to any microcrontroller similar or better.


### Network topology ###

The topology is Mesh networking. 
One master node (called master) controls one or several slave nodes (called nodes).
Which makes it compatible with RS-485 tranceivers, which allow inexpensive and (ultra) low power 
transmission over huge distances and difficult environments. 


The transmission uses a 3 wire synchronous SPI scheme. 
That is : 

* A Clock line (master to node)
* MISO (Master In Slave Out)
* MOSI (Master Out Slave In)

```
             [node-3]-----[node-4]-------[master]-----[node-1]-----[node-2]
                                                         +	
                                                         |
                                                      [node-5]
                                                         ...	
```

#### Note ####

QuinoHa uses the three SPI lines over rs-485 and a power line (4 wire pairs), 
which makes it pratical to use it fully powered with a single CAT5 transmission cable. 


### Network protocol ###

#####[Basic]#####

Small packets (around 40Bytes) are broadcast to the network synchronously on request from master to node. 
Packets are targeted to a particular node.
Simulteanously in the return buffer, a node may transfer data back to the master. To avoid collisions, 
the node must have been elected for talkback by the master node.

Any node can request a talkback transfer at any time, by either pulling the right bits in the return buffer header while data is being transmitted, 
or if nothing is being transmitted, pull MOSI high when no clock is being pulsed, 
at that time the master node will send a dummy packet to enable the data transfer.

### Hardware topology ###

This has the following form, master <=> node <=> extension.

```
                                                                  +=====[extension1]
                 [master]-----[node1]--------------------------[node2]
                                 +=====[extension1]
                                 +=====[extension2]
```

(Quinoha supports two (three) extensions on each node, ADCE [Analog/Digital common extension] is the code name of the implemented implementation). 

### Performance ###

Throughput of 115kbps is acheived using the msp430g2553 as node (network) chip. The chip doesn't contain nor a FIFO buffer and a DMA buffer, so it cannot do many more. Maybe not the perfect tool for the job (I guess a cortex M0 might have been better suited).

Packet latency of about 1ms can be expected one way at 115kbps.

The chip performs flawlessly though and throughput is not the primary goal of the project. Which is "making a control network".

### Extension ###

#####[Quinoha ADCE]#####

The extensions are the reason of existence of this project. They are the I/Os at the final chain end point.

The Quinoha ADCE extension supports 
* a total 12 total I/Os.
* 5 10-bit ADCs
* 4 PWM channels (2 dual channels)
* Remote code upload and execution in integrated flash for faster (parallel) execution.
* Bi directional communication with master.
 
Extensions are tied to a single node, with a common SPI bus and a dedicated CTS/RTS channel; up to two (or three) extensions can be wired to a single node.


### Quinoha Node Hardware and pinout ###

#####[Basic]#####
- MSP430G2553 20PIN

``` 
  P1.1  to Rasbperry GPIO MOSI
  P1.2  to Raspberry GPIO CLK
  P1.4  to Raspberry GPIO MISO Pulled up (inverted logically).

  P1.5  to Extension BUS CLK
  P1.6  to Extension BUS MISO
  P1.7  to Extension BUS MOSI
  P2.1  to Extension 1   RTS
  P2.2  to Extension 1   CTS
  P2.3  to Extension 2   RTS
  P2.4  to Extension 2   CTS  
```  

### Quinoha ADCE Hardware and pinout ###

#####[Basic]#####
- MSP430G2553 28PIN

```
  P1.5  to Extension BUS CLK
  P1.6  to Extension BUS MISO
  P1.7  to Extension BUS MOSI
  P2.1  to Extension 1   RTS
  P2.2  to Extension 1   CTS

  P1.0  I/O,ADC
  P1.1  I/O,ADC
  P1.2  I/O,ADC
  P1.3  I/O,ADC
  P1.4  I/O,ADC
  Px.X  I/O,PWM
  Px.X  I/O,PWM
  Px.X  I/O,PWM
  Px.X  I/O,PWM
  Px.x  I/O
  Px.x  I/O
  Px.x  I/O  
```

#####[Hardware specifications]#####

- Quinoha Nodes and ADCE fit both on a single dual-sided PCB 
- 3.5cmx2.5cm including RS485 transceivers and current regulators.
- 14-pin header (2.54 mm header)
- VCC Input from 5v to 10v DC
- Average power consumption is <1mA at idle. <10mA average load; absolute maximum 250mA if current is drawn from all I/Os. 
- second extension header
- debug/flash program headers for ADCE and Node.



Hardware prototypes available on request. Contact me.

