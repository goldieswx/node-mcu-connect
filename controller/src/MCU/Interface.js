

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

var _     = require('lodash');
var util  = require('util');

var MCUObject = require('./Object');
var MCUIo = require('./Io');

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

	return _.debounce(function(){
		var state;

		var aggreatedMessage = new Buffer("012345678901234567891234"); // see adce.c@processMsg for details 
			aggreatedMessage.fill(0);
			aggreatedMessage.writeUInt32LE(0x66+self.id,0);
			aggreatedMessage.writeUInt32LE(self.node.id,20);

		var pwmValues = {  values: [0xFFFF,0xFFFF,0xFFFF,0xFFFF], received:0};

		_.remove(self._outMessageQueue,function(item){
			// aggregate digital outs.
			if (item[0] == (0x66+self.id)) {
				state = state || [0x00,0x00,0x00,0xFF,0xFF,0xFF]; // everything unchanged.
				var j;
				for (j=0;j<3;j++) {
		 var tmp = state[j];	
					state[j] |= item[j+1] & state[j+3];
					state[j+3] &= tmp | item[j+4];
				}
			} else {
				// aggregate pwm values because we can pack them by 4
				if (item[0] == (0x44+self.id) && (item[1]==0x01) && (item[2]==0x20) && (item[3] == 0x00) ){
					pwmValues.received++;

					var i,j=4;
					for(i=0;i<4;i++) {
						var tmpValue = item.readUInt16LE(j);
						pwmValues.values[i] = (tmpValue == 0xFFFF)?pwmValues.values[i]:tmpValue;
						j += 2;
					}
					//pwm set value;
				} else {
					self._network._sendMessage(item);
				}

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
			self._network._sendMessage(aggreatedMessage);
//			console.log(aggreatedMessage);
		}

		if (pwmValues.received) {
//			console.log(MCUInterface.getPWMMessage(this.node.id,this.id,pwmValues.values));
			self._network._sendMessage(MCUInterface.getPWMMessage(this.node.id,this.id,pwmValues.values));
		}

	},35,{leading:false,trailing:true});

};




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
	var hardwareKeyList = _.map(this.children,'hardwareKeys');
    var config = { portDIR: [0xFF,0xFF,0xFF], portADC: 0, portREN: [0x00,0x00,0x00], portOUT: [0,0,0], portPWM: [0,0] /* only on 2.x, 3.x */
					,pwm: { channels : [ { setFlag:0,dutyCycle:1000}, { setFlag:0,dutyCycle:1000}], enabled: false}
					};

	this.config = this.config || config;
	this.config.pwm.enabled = false;

	_.each(hardwareKeyList,function(item){
    		if (item.direction == "out") { // Digital out
    			config.portDIR[item.port-1] |= item.portMask;   // enable output flag
    			config.portREN[item.port-1] &= (0xFF & ~item.portMask); // disable pullup/dn flag
    			config.portOUT[item.port-1] &= (0xFF & ~item.portMask); // disable out flag
				if (item.type == "pwm") {
					config.portPWM[item.port-2] |= item.portMask; /* enable */
					config.pwm.channels[0].enabled = config.pwm.channels[0].enabled || (_.indexOf(['3.5','3.6'],item.portName) > -1 );
					config.pwm.channels[1].enabled = config.pwm.channels[1].enabled || (_.indexOf(['2.1','3.3'],item.portName) > -1 );
					config.pwm.enabled = true;
				}

    		} else {
    			if (item.trigger < (31)) { // ADC
    				config.portADC |= item.configMask;
	    			config.portOUT[item.port-1] &= (0xFF & ~item.configMask); // disable out flag
    				config.portDIR[item.port-1] &= (0xFF & ~item.configMask); // enable output flag
    				config.portREN[item.port-1] &= (0xFF & ~item.configMask); // disable pullup/dn flag
    			} else { // Digital in
    				config.portDIR[item.port-1] &= (0xFF & ~item.portMask); // disable output flag
    				config.portREN[item.port-1] |= (0xFF & item.portMask); // enable pullup flag
    				config.portOUT[item.port-1] |= (0xFF & item.portMask); // pull "up"
    			}
    		}
    });

	this._network._sendMessage(MCUInterface.getRefreshMessage(config,this.node.id,this.id));
	if (config.pwm.enabled) {
		this._network._sendMessage(MCUInterface.getPWMDutyCycleMessage(config,this.node.id,this.id));
	}

};

MCUInterface.prototype._pwm = function(io,value) {

	var values = [0xFFFF,0xFFFF,0xFFFF,0xFFFF];
	var channelMap = ['3.5','3.6','2.1','3.3'];
	// 3.5 3.6 2.1 3.3
	var ioPWMChannelId = _.indexOf(channelMap,io.hardwareKeys.portName);
	if (ioPWMChannelId == -1) { console.log (io.hardwareKeys.portName + 'is not a pwm channel'); return; }
	values[ioPWMChannelId] = value;
	//this._network._sendMessage(MCUInterface.getPWMMessage(this.node.id, this.id,values));
	this._outMessageQueue.push(MCUInterface.getPWMMessage(this.node.id, this.id,values));
	this.throttleMessageQueue();
	//console.log("pused");
};



MCUInterface.getPWMDutyCycleMessage = function(config,nodeId,interfaceId) {

	// 0x44+interfaceId
    // 0X01 pwm
	// 0X0010 set/reset duty cycle
    // 0x0000 pwm init set to set (1) unset(2) to top,
	// 0x0000 dty cycle 1
	// 0x0000 pwm init set to set,
	// 0x0000 dty cycle 2
/*
#define PWM_INIT_SET_CHAN1 0X0001
#define PWM_INIT_UNSET_CHAN1 0X0002
#define PWM_INIT_SET_CHAN2 0X0004
#define PWM_INIT_UNSET_CHAN2 0X0008 */ 

	var msg = new Buffer("                        ");

	msg.fill(0);
	msg.writeUInt8(0x44+interfaceId,0);
	msg.writeUInt8(0x01,1);
	msg.writeUInt16LE(0x0010,2); // 	PWM_SET_DUTY_CYCLE
	
	var enableFlag = 0 ;
	enableFlag |= (config.pwm.channels[0].enabled) ? 0x0001:0x0002;
	enableFlag |= (config.pwm.channels[1].enabled) ? 0x0004:0x0008;
	//enableFlag = 5;
	//console.log("enbleFlag",enableFlag,JSON.stringify(config));

	msg.writeUInt16LE(enableFlag,4);
	msg.writeUInt16LE(config.pwm.channels[0].dutyCycle || 0,6);
	msg.writeUInt16LE(config.pwm.channels[1].dutyCycle || 0,8);
	msg.writeUInt32LE(nodeId,20);

	return msg;
}


MCUInterface.getPWMMessage = function(nodeId,interfaceId,pwmValues) {

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
	msg.writeUInt8(0x01,1); // custom cmd set pwm
	msg.writeUInt16LE(0x0020,2); // PWM_SET_VALUE
	msg.writeUInt16LE(pwmValues[0]||0xFFFF,4);
	msg.writeUInt16LE(pwmValues[1]||0xFFFF,6);
	msg.writeUInt16LE(pwmValues[2]||0xFFFF,8);
	msg.writeUInt16LE(pwmValues[3]||0xFFFF,10);
	msg.writeUInt32LE(nodeId,20);

	return msg;

}

MCUInterface.prototype.getPWMChannelMapping = function() {

	ioChannels = {'3.5':{id: 0},'3.6' :{id: 0},'2.1' :{id: 1},'3.3':{id: 1}};
	return ioChannels;

};

MCUInterface.prototype.getConfig = function() {
	return this.config;
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
