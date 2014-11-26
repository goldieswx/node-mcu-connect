/*
    node-mcu-connect . node.js UDP Interface for embedded devices.
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

var util = require('util');
var dgram = require('dgram');
var _ = require('lodash-node');


var MCUObject = function(key) {

  this.children = [];
  this.key = key; 
  this.aliases = [];
  this.childType = "child";
  this.tags = [];

};


/**
 * Function find(selector)
 * @returns 
 *
 * Transforms a selector to a collection of objects.
 */

MCUObject.prototype.find = function(selector) {

  // split expression by spaces and recursively call ourselves
  // to reduce the result subset by subset
  var expression  = selector.split(' ');
  if (expression.length > 1) {
	 var tmp = this;
	 _.each(expression,function(item){
		tmp = tmp.find(item);
	 });
	 return tmp;
  }

  // treat simpler case where selector is just one string ([key]:[[tag]:[tag2]])
  var ret,children = [];
  var parseChildren = function(obj,filter,result) {
	 // check if the object matches, check also its children recursively
	 // when match, push result by reference
	 if (obj.children && obj.children.length) {
		result.push  (_.filter(obj.children,function(child){
			return  ((filter.key.length)?(child.key == filter.key):1)
					&& _.intersection(child.tags,filter.tags).length == filter.tags.length;
		}));
		_.each(obj.children,function(child) {
		  parseChildren(child,filter,result);
		});
	 } 
  }
  
  var selector = selector.split(":");
  tags = selector.splice(1);
  var key = selector;

  parseChildren(this,{"key":key[0],"tags":tags},children);
  children = _.flatten(children);

  if (children.length != 1) {
	 ret = new MCUObject();
	 ret.childType = "mixed-multiple";
	 ret.children = children;
  } else {
	 ret = children[0];
  }
  
  return ret;

};

MCUObject.prototype.alias = function(alias) {
  this.aliases.push(alias);
};

MCUObject.prototype.add  = function(child) {
   
   child._network = this._network||this;
   this.children.push(child);
   return child;
};

MCUObject.prototype.tag  = function(tags) {
   
   this.tags = _.union(this.tags,tags.split(' '));
   return this;
};

MCUObject.prototype.on = function(eventId,fn) {

    if (this.childType == "mixed-multiple") {
       _.each(this.children,function(item){
           item.on(eventId,fn);
       });
    }

}


var MCUNetwork = function() {


  var self = this;

  this.superClass = MCUObject.prototype;
  MCUNetwork.super_.call(this);
  this.childType = "network";

  this._cachedNodeList = {}; // node list lazy loaded and cached by id.


  this._listenntingSocket = dgram.createSocket('udp4');
  this._listenntingSocket.on("message",function(msg,rinfo){
	  self._callback(msg);
  });

  this._listenntingSocket.bind(9931);



};
util.inherits(MCUNetwork,MCUObject);


MCUNetwork.prototype.add = function(key,nodeId) {

   var child = new MCUNode();
   child.key = key;
   child.id = nodeId;
   return (this.superClass.add.bind(this))(child);

};

MCUNetwork.prototype._sendMessage = function(buffer) {

	var client = dgram.createSocket("udp4");
		client.send(buffer, 0, buffer.length, 9930,'localhost', function(err, bytes) {
		client.close();
	});

};

MCUNetwork.prototype._dispatchMessage = function(message) {

	// lazy cache of cachedNodeList.
	if(_.isUndefined(this._cachedNodeList[message.destination])) {
		this._cachedNodeList[message.destination] = _.find(this.children,{id:message.destination});
	}

	// dispatch message to right node.
	if (this._cachedNodeList[message.destination]) {
    this._cachedNodeList[message.destination]._callback(message);
  } else {
    console.log('callback required to unregistered node: ',message.destination);
  }

}


/**
 * MCUNetwork::_callback(value)
 *
 * value is received in binary form from udp.
 * extract node number, interface id and io.
 * dispatch right away to node
 *
 */
MCUNetwork.prototype._callback = function(value) {

	var message = {
		timestamp 	: 	(new Date()).getTime(),
		destination : 	value.readUInt32LE(36),
		trigger 	:  	value.readUInt16LE(12),
		portData 	: [	value.readUInt8(10),
						value.readUInt8(11),
						value.readUInt8(14)],
		adcData 	: [	value.readUInt16LE(0x0),
						value.readUInt16LE(0x2),
						value.readUInt16LE(0x4),
						value.readUInt16LE(0x6),
						value.readUInt16LE(0x8)],
		interfaceId	:	1
	}
	this._dispatchMessage(message);
}


var MCUNode = function() {

   this.superClass = MCUObject.prototype;
   MCUNode.super_.call(this);
   
   this.childType = "node";
   this._cachedInterfaceList = {}; // list of interfaced indexed by (hardware) id

};

util.inherits(MCUNode,MCUObject);
MCUNode.prototype.add = function(key,id) {

   var child = new MCUInterface();
   child.key = key;
   child.id = id;
   child.node = this;
   return (this.superClass.add.bind(this))(child);

};

MCUNode.prototype._callback = function(message) {

	// lazy cache of cachedInterfaceList.
	if(_.isUndefined(this._cachedInterfaceList[message.interfaceId])) {
		this._cachedInterfaceList[message.interfaceId] = _.find(this.children,{id:message.interfaceId});
	}

	this._cachedInterfaceList[message.interfaceId]._callback(message);

};

var MCUInterface = function() {

   this.superClass = MCUObject.prototype;
   MCUInterface.super_.call(this);
   this.childType = "interface";

   this._cachedIOList = [];  // list of IO with hardware ids.

};

util.inherits(MCUInterface,MCUObject);

MCUInterface.prototype.add = function(key,hardwareKeys) {

   var child = new MCUIo();
   child.key = key;
   child.node = this.node;
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
    var config = { portDIR: [0,0,0], portADC: 0, portREN: [0xFF,0xFF,0xFF], portOUT: [0,0,0]};

    _.each(hardwareKeyList,function(item){
    		if (item.direction == "out") { // Digital in
    			config.portDIR[item.port-1] |= item.portMask;   // enable output flag
    			config.portREN[item.port-1] &= (0xFF & ~item.portMask); // disable pullup/dn flag
    			config.portOUT[item.port-1] &= (0xFF & ~item.portMask); // disable out flag
    		} else {
    			if (item.trigger < (31)) { // ADC
    				config.portADC |= item.trigger;
	    			config.portOUT[item.port-1] &= (0xFF & ~item.trigger); // disable out flag
    				config.portDIR[item.port-1] &= (0xFF & ~item.trigger); // enable output flag
    				config.portREN[item.port-1] |= (0xFF & ~item.trigger); // disable pullup/dn flag
    			} else { // Digital out 
    				config.portDIR[item.port-1] &= (0xFF & ~item.portMask); // disable output flag
    				config.portREN[item.port-1] |= item.portMask; // enable pulldn flag
    				config.portOUT[item.port-1] &= (0xFF & ~item.portMask); // pull dn
    			}
    		}
    });

    console.log (config);

};


MCUInterface.prototype._callback = function(message) {
	
	// compare each io with the trigger and the port masks to identify 
	// the ios to callback
 	var ioId = message.trigger;
	_.each(this.children,function(item){
        
		if ((item.hardwareKeys.trigger & message.trigger) && (item.hardwareKeys.ignorePortMask || (item.hardwareKeys.portMask & message.portData[item.port-1]))) {
			item._callback(message);
		}
	});

};


var MCUIo = function() {

   this.superClass = MCUObject.prototype;
   MCUIo.super_.call(this);
   this.childType = "io";
  // this.value = undefined;
   this._callbacks = { "change":[],"up":[],"down":[]};
};

util.inherits(MCUIo,MCUObject);


/**
 * MCUIo::on (eventType,callback)
 * 
 * register callback event
 */
MCUIo.prototype.on = function(eventType,fn) {

  this._callbacks[eventType].push (
    {callback:fn,context:{},previousValue:null,value:null,previousContext:{}}
  );

};

MCUIo.prototype._callback = function(message) {

  var self = this;
  _.forOwn(this._callbacks,function(item,key) {
      switch(key) {
          case "change": 
            _.each(item,function(changeEvent){
              changeEvent.context = message;
              changeEvent.previousValue = changeEvent.value;
              changeEvent.value = MCUIo.getValueFromContext(self.hardwareKeys,message);
              changeEvent.callback(changeEvent); 
              changeEvent.previousContext = message; 
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

};

MCUIo.prototype.enable = function(value) {

	if (!arguments.length) { value = 1; }
	value = (value)?1:0;

	if (_.isUndefined(this.value) || (this.value != value)) {
		this._network._sendMessage(MCUIo.getMessageIoWriteDigital(this.hardwareKeys,this.node.id,value));
		this.value = value;
	}

}

MCUIo.prototype.disable = function() {


	if (_.isUndefined(this.value) || (this.value)) {
		this._network._sendMessage(MCUIo.getMessageIoWriteDigital(this.hardwareKeys,this.node.id,0));
		this.value = 0;
	}

}


MCUIo.getValueFromContext = function(hardwareKeys,message) {

      if (!_.isUndefined(hardwareKeys.analogTrigger)) { return message.adcData[hardwareKeys.analogTrigger]; }
      return null;
};


MCUIo.getMessageIoWriteDigital = function(hardwareKeys,ioKey,value) {

    var msg = new Buffer("012345678901234567891234"); // see adce.c@processMsg for details 
	var andAddrStart = 3; 
	var orAddrStart = 0;

	msg.fill(0);
    msg.writeUInt32LE(0x66,0);
    msg.writeUInt32LE(ioKey,20);

    if (value) {
    	msg.writeUInt8 (0xFF & hardwareKeys.portMask,orAddrStart + hardwareKeys.port );
		msg.writeUInt8 (0xFF ,andAddrStart + hardwareKeys.port );    	
    } else {
    	msg.writeUInt8 (0x00,orAddrStart + hardwareKeys.port );
		msg.writeUInt8 (0xFF & ~hardwareKeys.portMask,andAddrStart + hardwareKeys.port );    	
    }

    console.log(msg);

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
	var mapping = { port: parseInt(portNumber[0]), mask: 1 << parseInt(portNumber[1]) };

	var ret = { trigger: 0, portMask: 0x00, port: mapping.port, ignorePortMask: 0, direction: keys[1]};

	if (keys[0] == "analog") {
		var tmp 				= mapping.mask;
  			ret.trigger  		= mapping.mask;
			ret.ignorePortMask 	= 1;
    		ret.analogTrigger 	= 0;  while (tmp >>= 1){ ret.analogTrigger++;  }
	} else {
			ret.trigger 		= 1 << (4+mapping.port); // 1<<5 for port 1, <<6 for 2 ...
			ret.portMask 		= mapping.mask;
			if(mapping.port > 1) {
				ret.portMask <<= 3; // avail P2.x and P3.x is BIT3..BIT7 instead of BIT0..BIT4
			}
	}
	return ret;
};

MCUIo.throttle = function (e,fn,timeout) {

   e.throttle = e.throttle  || _.throttle(fn,timeout,{leading:false,trailing:true});
   e.throttle();

}

/**
 * function() averageFilter
 * Creates a average between the currently elapsed n milliseconds of the event values.
 */

MCUIo.timedAverageFilter = function(event) {

	var averageTimeFrame = 50; // values are kept for average for 50ms

	event.filters = event.filters || {};
	event.filters.timedAverage =  event.filters.timedAverage || {};
	var e = event.filters.timedAverage;
	var sum = 0, len = 0;

	e.ranges = e.ranges || [];
	e.ranges.push({ v: event.value, t: event.context.timestamp } );

	_.each(e.ranges,function(item) {
		if ((event.context.timestamp - item.t) < averageTimeFrame) {
			sum += item.v;
			len++;
		}
	});

	if (len) {
		sum = Math.round(sum/len);
	} else {
		sum = event.value; 
	}

	e.value = sum;

	_.remove(e.ranges,function(item){
		return ((event.context.timestamp - item.t) > averageTimeFrame);
	});

};



/* test area */


var net = new MCUNetwork();

(function($) {

    net.add('node-entry',0x03).add('entry',0x01);
    net.add('node-bedroom',0x04).add('bedroom',0x01);
    net.add('sync',0x09).add('sync',0x01).add('sync','digital out 1.0').tag('sync');
   
   	// Entry interface
    (function(i) {
		i.add('sw-1','analog in 1.0').tag("in");
		i.add('sw-2','analog in 1.1').tag("in2");
		i.refresh();
	})($('entry'));


	//Bedroom interface
    (function(i) {
	    i.add('sw-3',		'analog in 1.2').tag("in");
		i.add('spotlight-1','digital out 1.1').tag("out");
		i.add('spotlight-2','digital out 1.0').tag("out");
        i.refresh();
	})($('bedroom'));

	var blink = function(event) {

		if (event.blink && event.blink.active) { return; }
		event.blink = { numRepeats : 0, active:1 };

		var clearInt = setInterval(function(){ $('spotlight-1').toggle(); event.blink.numRepeats++; if (event.blink.numRepeats > 5) { clearInterval(clearInt); event.blink.active=0; } },200);
	}

    // IO Logic
	$(":in").on("change",function(event) {

    	MCUIo.timedAverageFilter(event);
		MCUIo.throttle(event,function() { 
			blink(event);
			//$('spotlight-1').enable(event.filters.timedAverage.value < 450)
		}, 50);

	});

  
	$(":in2").on("change",function(event) {

		MCUIo.timedAverageFilter(event);
		MCUIo.throttle(event,function() { 
				$('spotlight-2').enable(event.filters.timedAverage.value < 450)
		}, 50);
	});


	setInterval(function(){ $(':sync').toggle() },2000);

})(net.find.bind(net));

/* * */ 
/*
var  messageToBuffer= function(message) {
	var buffer = new Buffer("1234567890123456789012345678901234567890");
   	buffer.writeUInt32LE(message.destination,36);
	buffer.writeUInt16LE(message.trigger,12);
	buffer.writeUInt8(message.portData[0],10);
	buffer.writeUInt8(message.portData[1],11);
	buffer.writeUInt8(message.portData[2],14);
	buffer.writeUInt16LE(message.adcData[0],0x0);
	buffer.writeUInt16LE(message.adcData[1],0x2);
	buffer.writeUInt16LE(message.adcData[2],0x4);
	buffer.writeUInt16LE(message.adcData[3],0x6);
	buffer.writeUInt16LE(message.adcData[4],0x8);
	return buffer;
};
*/