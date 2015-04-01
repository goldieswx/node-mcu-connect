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
