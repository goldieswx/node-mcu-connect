/*
    node-mcu-connect . node.js UDP Interface for embedded devices.
    Copyright (C) 2013-5 David Jakubowski
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

var _     = require('lodash-node');
var util  = require('util');

var MCUObject = require('./Object');

var MCUInterface = function() {

	this.superClass = MCUObject.prototype;
	MCUInterface.super_.call(this);
	this.childType = "interface";
	this._outMessageQueue = [];
	this._cachedIOList = [];  // list of IO with hardware ids.
	
	this.throttleMessageQueue = MCUInterface.getThrottleMessageQueue(this);

};

util.inherits(MCUInterface,MCUObject);

MCUInterface.getThrottleMessageQueue = function(self) {

	return _.throttle(function(){
		var state;
		var aggreatedMessage = new Buffer("012345678901234567891234"); // see adce.c@processMsg for details 
		aggreatedMessage.fill(0);
		aggreatedMessage.writeUInt32LE(0x66+self.id,0);
		aggreatedMessage.writeUInt32LE(self.node.id,20);

		_.remove(self._outMessageQueue,function(item){
			if (item[0] == (0x66+self.id)) {
				state = state || [0x00,0x00,0x00,0xFF,0xFF,0xFF]; // everything unchanged.
				state[3] ^= ~item[4]; // toggle changed bits
				state[4] ^= ~item[5];
				state[5] ^= ~item[6];
				state[0] ^= item[1] ^ state[3]; // toggle changed bits expect if reset
				state[1] ^= item[2] ^ state[4];
				state[2] ^= item[3] ^ state[5];

			} else {
				self._network._sendMessage(item); 
			}         
			return true;
		});

		if(state) { 

		aggreatedMessage[1] = 0xff & state[0];
		aggreatedMessage[2] = 0xff & state[1];
		aggreatedMessage[3] = 0xff & state[2];
		aggreatedMessage[4] = 0xff & state[3];
		aggreatedMessage[5] = 0xff & state[4];
		aggreatedMessage[6] = 0xff & state[5];

		self._network._sendMessage(aggreatedMessage); }

	},30,{leading:false,trailing:true}); 

};


MCUInterface.getPWMMessage = function(nodeId,interfaceId,dutyCycle) {

	var msg = new Buffer("                        ");
	msg.fill(0);
	msg.writeUInt8(0x44+interfaceId,0);
	msg.writeUInt8(0x01,1);
	msg.writeUInt16LE(dutyCycle,2); 
	msg.writeUInt32LE(nodeId,20);

	return msg;
}

MCUInterface.prototype.add = function(key,hardwareKeys) {

   var child = new MCUIo();
   child.key = key;
   child.node = this.node;
   child._interface = this;
   if (_.isString(hardwareKeys)) { hardwareKeys = MCUIo.getPortMask(hardwareKeys); }
   child.hardwareKeys = hardwareKeys;
   return (this.superClass.add.bind(this))(child);

};

/**
 *  Function MCUInterface::refresh()
 *  Transmits the IO state 
 */

MCUInterface.prototype.refresh = function() {

	// loop through all children and see what io they use and how
	var hardwareKeyList = _.pluck(this.children,'hardwareKeys');
    var config = { portDIR: [0,0,0], portADC: 0, portREN: [0x00,0x00,0x00], portOUT: [0,0,0]};

    _.each(hardwareKeyList,function(item){
    		if (item.direction == "out") { // Digital out
    			config.portDIR[item.port-1] |= item.portMask;   // enable output flag
    			config.portREN[item.port-1] &= (0xFF & ~item.portMask); // disable pullup/dn flag
    			config.portOUT[item.port-1] &= (0xFF & ~item.portMask); // disable out flag
    		} else {
    			if (item.trigger < (31)) { // ADC
    				config.portADC |= item.configMask;
	    			config.portOUT[item.port-1] &= (0xFF & ~item.configMask); // disable out flag
    				config.portDIR[item.port-1] &= (0xFF & ~item.configMask); // enable output flag
    				config.portREN[item.port-1] &= (0xFF & ~item.configMask); // disable pullup/dn flag
    			} else { // Digital in
    				config.portDIR[item.port-1] &= (0xFF & ~item.portMask); // disable output flag
    				config.portREN[item.port-1] &= (0xFF & ~item.portMask); // disble pulldn flag
    				config.portOUT[item.port-1] &= (0xFF & ~item.portMask); // pull dn
    			}
    		}
    });

	this._network._sendMessage(MCUInterface.getRefreshMessage(config,this.node.id,this.id));

};

MCUInterface.prototype.pwm = function(dutyCycle) {

   this._network._sendMessage(MCUInterface.getPWMMessage(this.node.id,this.id,dutyCycle));

};

MCUInterface.getRefreshMessage = function(config,nodeId,interfaceId) {

	var msg = new Buffer("                        ");
	msg.fill(0);
	msg.writeUInt8(0x55+interfaceId,0);
	msg.writeUInt8(config.portDIR[0],1); 
	msg.writeUInt8(config.portADC,2); 
	msg.writeUInt8(config.portREN[0],3); 
	msg.writeUInt8(config.portOUT[0],4); 
	msg.writeUInt8(config.portDIR[1],5); 
	msg.writeUInt8(config.portREN[1],6); 
	msg.writeUInt8(config.portOUT[1],7); 
	msg.writeUInt8(config.portDIR[2],8); 
	msg.writeUInt8(config.portREN[2],9); 
	msg.writeUInt8(config.portOUT[2],10); 
	msg.writeUInt32LE(nodeId,20);

	return msg;
}


MCUInterface.prototype._callback = function(message) {
	
	// compare each io with the trigger and the port masks to identify 
	// the ios to callback
 	var ioId = message.trigger;
	_.each(this.children,function(item){
		//if ((item.hardwareKeys.trigger & message.trigger) && (item.hardwareKeys.ignorePortMask || (item.hardwareKeys.portMask & message.portData[item.hardwareKeys.port-1]))) {
		if (item.hardwareKeys.trigger & message.trigger) {
			item._callback(message);
		}
	});

};

module.exports = MCUInterface;
