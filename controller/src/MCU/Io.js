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
var MCUEvent = require('./Event');

var MCUIo = function() {

   this.superClass = MCUObject.prototype;
   MCUIo.super_.call(this);
   this.childType = "io";
  // this.value = undefined;
   this._callbacks = { "change":[],"up":[],"down":[]};
  
};

util.inherits(MCUIo,MCUObject);

MCUIo.prototype.toggle = function() {

		if (this.value) {
			this.disable();
		} else {
			this.enable();
		}
    return this;
};

/**
 * MCUIo::on (eventType,callback)
 * 
 * register callback event
 */
MCUIo.prototype.on = function(eventType,fn) {

  this._callbacks[eventType].push (new MCUEvent(fn));

};

MCUIo.prototype._callback = function(message) {

  var self = this;
  _.forOwn(this._callbacks,function(item,key) {
      switch(key) {
          case "change": 
            _.each(item,function(changeEvent){
              changeEvent.value = MCUIo.getValueFromContext(self.hardwareKeys,message);
              if (changeEvent.value != changeEvent.previousValue) {
	              changeEvent.context = message;
	              changeEvent.previousValue = changeEvent.value;
	              changeEvent.callback(changeEvent); 
	              changeEvent.previousContext = message; 
	              this.value = changeEvent.value;
	          }
            });
          break;
      }
  });

};

MCUIo.prototype.toggle = function() {

		if (this.value) {
			this.disable();
		} else {
			this.enable();
		}
    return this;
};

MCUIo.prototype.enable = function(value) {

	if (!arguments.length) { value = 1; }
	value = (value)?1:0;

	if (_.isUndefined(this.value) || (this.value != value)) {
  	
  	    this._interface._outMessageQueue.push(MCUIo.getMessageIoWriteDigital(this.hardwareKeys,this.node.id,this._interface.id,value));
		this._interface.throttleMessageQueue();

		//var msg = MCUIo.getMessageIoWriteDigital(this.hardwareKeys,this.node.id,this._interface.id,value);
		//this._network._sendMessage(msg);
		//console.log(msg);

		this.value = value;
	}
  return this;

}

MCUIo.prototype.disable = function() {

	if (_.isUndefined(this.value) || (this.value)) {
    this._interface._outMessageQueue.push(MCUIo.getMessageIoWriteDigital(this.hardwareKeys,this.node.id,this._interface.id,0));
    this._interface.throttleMessageQueue();
		this.value = 0;
	}
  return this;
}


MCUIo.prototype.pwm = function(value) {

	this._interface._pwm(this,value);
	this.value = 0;

}


MCUIo.getValueFromContext = function(hardwareKeys,message) {

      if (!_.isUndefined(hardwareKeys.analogTrigger)) { return message.adcData[hardwareKeys.analogTrigger]; }
      return (hardwareKeys.portMask & message.portData[hardwareKeys.port-1])?1:0;

};


MCUIo.getMessageIoWriteDigital = function(hardwareKeys,nodeId,interfaceId,value) {

    var msg = new Buffer("012345678901234567891234"); // see adce.c@processMsg for details 
	var andAddrStart = 3; 
	var orAddrStart = 0;

	msg.fill(0);
    msg.writeUInt32LE(0x66+interfaceId,0);
    msg.writeUInt32LE(nodeId,20);

    // and all (other) ports (so to not disable them).
    msg[4] = 0xFF;
	msg[5] = 0xFF;
    msg[6] = 0xFF;

    if (value) {
    	msg.writeUInt8 (0xFF & hardwareKeys.portMask,orAddrStart + hardwareKeys.port );
		msg.writeUInt8 (0xFF ,andAddrStart + hardwareKeys.port );    	
    } else {
    	msg.writeUInt8 (0x00,orAddrStart + hardwareKeys.port );
		msg.writeUInt8 (0xFF & ~hardwareKeys.portMask,andAddrStart + hardwareKeys.port );    	
    }

   return msg;

}

MCUIo.getPortMask = function(stringMask) {

	// stringMask is like "digital out 2.2" "analog in 1.2"
	var keys = stringMask.split(" ");
	// assume digital if not specified.
	if (keys.length == 1) {
		keys.unshift("digital");
	}

	var portNumber = keys[2].split("."); // e.g "2.3"
	var mapping = { port: parseInt(portNumber[0]), mask: (1 << parseInt(portNumber[1])) };

	var ret = { trigger: 0, portMask: 0x00, port: mapping.port, ignorePortMask: 0, direction: keys[1], type: keys[0], portName: keys[2] };

	if (keys[0] == "analog") {
		// analog config is reverted 0..4 => 4..0 from writing config (0x55 messages) to triggers/events(0x66)
		// reason for this is MSP if writing adc buffers backwards in CONSEQ_1 mode.
  			ret.configMask  	= mapping.mask;
			ret.ignorePortMask 	= 1;
			ret.analogTrigger 	= (4-parseInt(portNumber[1])); // trigger mask is backwards (than used when flashing config --> 0x55)
			ret.trigger =  (1 << ret.analogTrigger);
	} else {
			ret.trigger 		= 1 << (4+mapping.port); // 1<<5 for port 1, <<6 for 2 ...
			ret.portMask 		= mapping.mask;
			if(mapping.port > 1) {
			//	ret.portMask <<= 3; // avail P2.x and P3.x is BIT3..BIT7 instead of BIT0..BIT4
			}
	}
	return ret;
};


module.exports = MCUIo;
