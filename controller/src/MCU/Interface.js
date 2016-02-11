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

/**
 * Repack all digital out events in one single event on each throttle
 */

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
				var j;
				for (j=0;j<3;j++) {
					state[j] |= (item[j+1] & item[j+4]);
					state[j] &= (item[j+4]);
					state[j+3] |= (item[j+4]);
					state[j+3] &= (item[j+1] | item[j+4]);
				}

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
    var config = { portDIR: [0,0,0], portADC: 0, portREN: [0x00,0x00,0x00], portOUT: [0,0,0], portPWM: [0,0] /* only on 2.x, 3.x */};

	var pwmData = { channels : [ { setFlag:0,dutyCycle:1000}, { setFlag:0,dutyCycle:1000}]};

    _.each(hardwareKeyList,function(item){
    		if (item.direction == "out") { // Digital out
    			config.portDIR[item.port-1] |= item.portMask;   // enable output flag
    			config.portREN[item.port-1] &= (0xFF & ~item.portMask); // disable pullup/dn flag
    			config.portOUT[item.port-1] &= (0xFF & ~item.portMask); // disable out flag
				if (item.type == "pwm") {
					config.portPWM[item.port-2] |= item.portMask; /* enable */

				}
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

MCUInterface.prototype.pwm = function(value,dutyCycle) {
	if (!_.isUndefined(dutyCycle)) {
		this._network._sendMessage(MCUInterface.getPWMDutyCycleMessage(this.node.id, this.id, dutyCycle));
	}

};
MCUInterface.getPWMDutyCycleMessage = function(nodeId,interfaceId,pwmInfo) {

	// 0x44+interfaceId
    // 0X01 pwm
	// 0X0010 set/reset duty cycle
    // 0x0000 pwm init set to set (1) unset(2) to top,
	// 0x0000 dty cycle 1
	// 0x0000 pwm init set to set,
	// 0x0000 dty cycle 2

	var msg = new Buffer("                        ");

	msg.fill(0);
	msg.writeUInt8(0x44+interfaceId,0);
	msg.writeUInt8(0x01,1);
	msg.writeUInt16LE(0x0010,2);
	msg.writeUInt16LE(pwmInfo.channels[0].setFlag,4);
	msg.writeUInt16LE(pwmInfo.channels[0].dutyCycle,6);
	msg.writeUInt16LE(pwmInfo.channels[1].setFlag,8);
	msg.writeUInt16LE(pwmInfo.channels[1].dutyCycle,10);
	msg.writeUInt32LE(nodeId,20);

	return msg;
}

MCUInterface.getPWMDutyCycleMessage = function(nodeId,interfaceId,pwmData) {

	// 0x44+interfaceId
	// 0X01 pwm
	// 0X0020 set pwm value(s)
	// 0x0000 chan1,
	// 0x0000 chan2
	// 0x0000 chan3,
	// 0x0000 chan4

	var msg = new Buffer("                        ");

	msg.fill(0);
	msg.writeUInt8(0x44+interfaceId,0);
	msg.writeUInt8(0x01,1);
	msg.writeUInt16LE(0x0020,2);
	msg.writeUInt16LE(pwmInfo.channel1||0xFFFF,4);
	msg.writeUInt16LE(pwmInfo.channel2||0xFFFF,6);
	msg.writeUInt16LE(pwmInfo.channel3||0xFFFF,8);
	msg.writeUInt16LE(pwmInfo.channel4||0xFFFF,10);
	msg.writeUInt32LE(nodeId,20);

	return msg;

}

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
	msg.writeUInt8(config.portPWM[0],11);
	msg.writeUInt8(config.portPWM[1],12);

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
