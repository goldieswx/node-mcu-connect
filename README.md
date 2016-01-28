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




Hardware.

There is currently one hardware implementation (TI MSP430G2x value line). 
We'll call the Quinoha implementation.

The codebase however is well decoupled from hardware and may be ported to any microcrontroller similar or better.


Network topology.

The topology is Mesh networking. 
One master node (called master) controls one or several slave nodes (called nodes).
Which makes it compatible with RS-485 tranceivers, which allow inexpensive and (ultra) low power 
transmission over huge distances and difficult environments. 


The transmission uses a 3 wire synchronous SPI scheme. That is : 
	- A Clock line (master to node)
	- MISO
	- MOSI

	[node-3]-----[node-4]-------[master]-----[node-1]-----[node-2]
						    +	
						    |
						 [node-5]
						   ...	

Note: 

QuinoHa uses the three SPI lines over rs-485 and a power line (4 wire pairs), 
which makes it pratical to use it fully powered with a single CAT5 transmission cable. 


Network protocol.

[Basic]

Small packets (around 40Bytes) are broadcast to the network synchronously on request from master to node. 
Packets are targeted to a particular node.
Simulteanously in the return buffer, a node may transfer data back to the master. To avoid collisions, 
the node must have been elected for talkback by the master node.

Any node can request a talkback transfer at any time, by either pulling the right bits in the return buffer header while data is being transmitted, 
or if nothing is being transmitted, pull MOSI high when no clock is being pulsed, 
at that time the master node will send a dummy packet to enable the data transfer.

[Hardware topology]

This has the following form, master <=> node <=> extension.

[master]-----[node]
		=====[extension1]
		=====[extension2]
