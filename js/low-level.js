/*
    node-mcu-connect. A node.js UDP Interface for embedded devices.
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


var lowLevelArduinoInterface = {};

/**
 * Function listenMessage
 * Make arduino reply on the IP address used for client connection (by default port 8888)
 */
lowLevelArduinoInterface.listenMessage = function() {
  return new Buffer("LISN");
}

/**
 * Function notifyAnalogMessage
 * Analog input arduino notify message, lowRange < anl.input==treshold < hiRange
.
 * Message will be sent from arduino to listenning client each time analog input
 * crosses lowRange or hiRange boundaries and upon setting up the listener.
 * Arduino can have up to 10 analog notification (0 <= notificationId < 10)
 */
lowLevelArduinoInterface.notifyAnalogMessage = function(notificationId,analogPinId,treshold,lowRange,hiRange) {

    var enable = 1;
    if (notificationId < 0)         { return; }
    if (notificationId >= 10)       { return; }
    if (analogPinId < 0)            { return; }
    if (analogPinId >= 10)          { return; }
    if (treshold > 1024)            { treshold = 1024; }
    if (treshold < 0)               { treshold = 0; }

    var message = new Buffer("NC/Axxxxxxxxxxxxx");

    message.writeUInt8(notificationId, 4);
    message.writeUInt16LE(analogPinId,7);
    message.writeUInt16LE(treshold,5);
    message.writeUInt16LE(lowRange,11);
    message.writeUInt16LE(hiRange,9);
    message.writeUInt16LE(9999,13);
    message.writeUInt16LE(enable,15);

    return message;
};


/**
 * Function notifyDigitalMessage
 * Digital input/output arduino notify message
 * Message will be sent from arduino to listenning client each digital i/o state
 change
 * and upon setting up the listener
 */
lowLevelArduinoInterface.notifyDigitalMessage = function(notificationId,digitalPinId) {

  if (notificationId < 0)   { return; }
  if (notificationId >= 10)   { return; }
  if (digitalPinId < 0)     { return; }
  if (digitalPinId >= 10)   { return; }

  var message = new Buffer("NC/Dxx");
  message.writeUInt8(notificationId, 4);
  message.writeUInt8(digitalPinId, 5);
  return message;
}

/**
 * Function getAnalogState()
 */
lowLevelArduinoInterface.getAnalogStateMessage = function(analogPinId) {

  if (analogPinId < 0)  { return; }
  if (analogPinId >= 10)  { return; }

  var message = new Buffer("ST/Ax");
  message.writeUInt8(analogPinId, 4);
  return message;
}

/**
 * Function getDigitalState()
 */
lowLevelArduinoInterface.getDigitalStateMessage = function(digitalPinId) {

  if (digitalPinId < 0)   { return; }
  if (digitalPinId >= 10) { return; }

  var message = new Buffer("ST/Dx");
  message.writeUInt8(digitalPinId, 4);
  return message;
}

/**
 * Function setDigitalState()
 */
lowLevelArduinoInterface.setDigitalStateMessage = function(digitalPinId,state) {

  if (digitalPinId < 0)   { return; }
  if (digitalPinId >= 10) { return; }
  state = (state)?1:0;

  var message = new Buffer("SE/Dxx");
  message.writeUInt8(digitalPinId, 4);
  message.writeUInt8(state, 5);
  return message;

}

lowLevelArduinoInterface.setDigitalStateMessageDuration = function(digitalPinId,state,duration) {

  if (digitalPinId < 0)   { return; }
  if (digitalPinId >= 10) { return; }
  state = (state)?1:0;

  var message = new Buffer("SD/Dxxxx");
  message.writeUInt8(digitalPinId, 4);
  message.writeUInt8(state, 5); // not used yet
  message.writeUInt16LE(duration,6);

  return message;

}
