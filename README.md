node-mcu-connect 
================

nodeMcuConnect is an open source framework whose goal is to setup an ultra low power, low latency microcontroller network controller (..matter of taste but I hate the IoT terminology).

The network is composed of nodes connected and powered through a RS-422/485 multipoint interface, handled by a master node.

- A comprehensive network controller is available in javascript (now through node.js) and communicates bi-directionally with a deamon which serves low level communication tasks.
- Master node deamon (written in C).
- Nodes supports up to three extensions (bi-directional and SPI wired to the node) which can be addressed through the controller. (written in javaScript)
- Code for msp430g2x nodes devices and extensions is available, hardware dependent instructions decoupled.  (written in C)

- Wiring schematics available.

This project is open source, licensed under GPLv3 terms (http://www.gnu.org/licenses/).

### Software ###

#### [basics] ####

A primitive end goal is to drive an I/O as output to a logical level (up or down). 
IOs are implemented on what is called an extension (see the hardware overview for more details).

To handle this, use the controller framework  :
```
  $('my-io').enable();  // sets my-io to logical high.
```
Another goal is to subscribe to an I/O as input changes
```
  $('button-1').on('change',function(e){
	    console.log('button 1 state is now :', e.value);
	});
```

Before this can be done, the network has to be setup, the nodes needs to be identified and mapped as well as the extensions.

```
   var net = new MCU.Network();

   // define node with hardware id #0x17, mapped to "my-new-node" key
   net.add('my-new-node',0x17);                            

   // define extension connected to extension #0.
   net.find("my-new-node").add('my-ext-0",0x00);   

   // define IOs on this extension.
   net.find("my-ext-0").add('button-1','digital in 1.3');
   net.find("my-ext-0").add('my-io','digital out 1.4');
   net.find("my-ext-0").add('my-analog-io-1','analog in 1.2');
   
   // upload IO mapping and IO state to the extension.
   net.find("my-ext-0").refresh();
```

The controller framework support chains which allow the following shortcuts
```
(function($) {
	net.add('my-new-node',0x17);   
	$('my-new-node').add('my-ext-0",0x00);   

	// Map IOs
	(function(i) {
		i.add('button-1','digital in 1.3');
		i.add('my-analog-io-1','analog in 1.2');
		i.add('my-io-1','digital out 2.1').tag("out");
		i.add('my-io-2','digital out 2.2').tag("out").tag("foo");
		i.add('my-io-3','digital out 2.3').tag("out");
		i.refresh();
	})($('my-ext-0'));

	// disable all IO tagged with out defined on the network (logical low)
	$(':out').disable();

	// disable all IO tagged with out under my-ext-0 (logical low)
	$('my-ext-0:out').disable();

})(net.find.bind(net));
```

#### Architecture ####

The software has the following structure

```
controller                  [high level script, master]            [Javascript]
daemon                      [unix daemon, interface with nodes]   [C]
node                        [firmware, interface with extensions] [C]
extension(adce)             [firmware, extension firmware]        [C]
```

### Compiling the firmware ###

You need the msp-gcc cross compiler packages, and the msp-gdb debug tools as well. Once downloaded there are two firmware to build.

##### Node #####

Under node/src type
```
$ make
```
This will generate a node firmware, the default has an hardware Id=1. Hardware ids identifies the node on the network and node ids must be unique on a network. Available IDs range from 1 to 31. use the NODEID option to create hardware with different ids. 

Next, plug the MSP430g2553 (20pin) node chip and upload it. 
```
$ mspdebug rf2500 "prog node.o"
```

##### Adce #####

Under peripheral/adce/src type 
```
$ make
```

plug the second MSP430g2553 chip (28pin) 
$ mspdebug rf2500 "prog node.o"


### Hardware ###

There is currently one hardware implementation (TI MSP430G2x value line), 
this is codenamed Quinoha.

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

Throughput of 120kbps is acheived using the msp430g2553 as node (network) chip. The chip doesn't contain nor a FIFO buffer and a DMA buffer, so it cannot do many more. Maybe not the perfect tool for the job (I guess a cortex M0 might have been better suited).

Packet latency of about 1ms can be expected (one way) at 120kbps. Theoretically, this makes about 400 packets per second. Altough possible the network is not supposed to grow over 30 nodes (at that time, favor multiple paralleled networks). Average network size is 10-15 nodes so this would mean a very resonable 25ms/35ms latency at full load.

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
- 3.5cmx2.5cm PCB including RS485 transceivers and current regulators.
- 14-pin header (2.54 mm header)
- VCC Input from 5v to 10v DC
- Average power consumption is <1mA at idle. <10mA average load; absolute maximum 250mA if current is drawn from all I/Os. 
- second extension header
- debug/flash program headers for ADCE and Node.



Hardware prototypes available on request. Contact me.

