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
   
   this.children.push(child);
   return child;
};

MCUObject.prototype.tag  = function(tags) {
   
   this.tags = _.union(this.tags,tags.split(' '));
   return this;
};




var MCUNetwork = function() {


  var self = this;

  this.superClass = MCUObject.prototype;
  MCUNetwork.super_.call(this);
  this.childType = "network";


  this._listenntingSocket = dgram.createSocket('udp4');
  this._listenntingSocket.on("message",function(msg,rinfo){
      self._callback(msg);
  });

  this._listenntingSocket.bind(9931);

};
util.inherits(MCUNetwork,MCUObject);


MCUNetwork.prototype.add = function(key) {

   var child = new MCUNode();
   child.key = key;
   return (this.superClass.add.bind(this))(child);

};

MCUNetwork.prototype._sendMessage = function(buffer) {

  var client = dgram.createSocket("udp4");
      client.send(buffer, 0, buffer.length, 9930,'localhost', function(err, bytes) {
      client.close();
  });

};


MCUNetwork.prototype._callback = function(value) {

  // value is received in binary form from udp.
  // extract node number, interface id and io.
  // dispatch right away to io and bubble up if necessary to net.
  var nodeId = "node-5";
  var interfaceId = "iface-2";
  var ioId = "1.5";

  var message = {
 	timestamp : (new Date()).getTime(),
 	destination : value.readUInt32LE(36),
        trigger :  value.readUInt16LE(12),
 	portData : [value.readUInt8(10),
 		    value.readUInt8(11),
 		    value.readUInt8(14)],
 	adcData : [value.readUInt16LE(0x0),
 	           value.readUInt16LE(0x2),
 	           value.readUInt16LE(0x4),
 	           value.readUInt16LE(0x6),
 	           value.readUInt16LE(0x8)], 
  }

  
  var dbg = [message.destination], j=1,i;

   for(i=0;i<5;i++) { 
    if (message.trigger & (j)) {
        dbg.push(i);
        dbg.push(message.adcData[i]);
    }
    j <<= 1;
   }
  console.log(dbg);
  
}


var MCUNode = function() {

   this.superClass = MCUObject.prototype;
   MCUNode.super_.call(this);
   this.childType = "node";

};

util.inherits(MCUNode,MCUObject);
MCUNode.prototype.add = function(key) {

   var child = new MCUInterface();
   child.key = key;
   return (this.superClass.add.bind(this))(child);

};


var MCUInterface = function() {

   this.superClass = MCUObject.prototype;
   MCUNode.super_.call(this);
   this.childType = "interface";

};
util.inherits(MCUInterface,MCUObject);

MCUInterface.prototype.add = function(key) {

   var child = new MCUIo();
   child.key = key;
   return (this.superClass.add.bind(this))(child);

};


var MCUIo = function() {

   this.superClass = MCUObject.prototype;
   MCUNode.super_.call(this);
   this.childType = "io"
};

MCUIo.prototype.on = function(eventType,fn) {



};


util.inherits(MCUIo,MCUObject);


var net = new MCUNetwork();
var node = net.add('node-5',5);
var iface = node.add('iface-2',1);
var io = iface.add('1.5',"P1.5").tag("tag-1");
//io = iface.add('1.5').tag("tag-1 tag-2");

/*iface.find('1.6').on('touch',function(e) {

      e.addTag('lighting-button','night');
      e.cancelBubble(); // bubble up to iface/node/net by deft.

});*/

//net._callback();
//console.log(net.find(5)[0].find(2)[0].find('1.5'));

/* core-2 draft

 var net = new MCUNetwork('NETWORK-ID');
 
 var node = net.add('1','room');
 var iface = node.add('1');
 var io = node.add('1.5'); 
 
 net.find('room/1/1.5');
 net.find('room//1.5','bed-sensor-left'); // often there is just one extension.
 net.find('room//bed-sensor-left');
 
 net.find('room//bed-sensor-left').setup('1.2p adc 15dx 15s 3.3v 10bit','5v 10k 10k'); // 15s retransmit value is unchanged
 net.find('room//bed-sensor-left').setup('1.3p out','5v 10k 10k'); // 15s retransmit value is unchanged
 
 
 (function ($) {
 	
 	$('room//bed-sensor-left').on('touch',function() {
              $('room//[lighting,out,low|mid]').toggle();
 	});
 	
 } (net.find));

var ext = $('node/7').find('ext/1').alias('room');

    ext.find('io/1.1')
       .alias('ambiance')
       .io('out','triac')
       .tag(['out','lightning','ambiance'])
       .map("2m 18m 2.5m, 4m 19m 2.5m"); // bbox or dot
       

    ext.find('io/1.2')
	.alias('entrance-sensor')
	.io('in','analog')
	.tag(['in','analog','lightning','sensor','low'])
	.map("south-east 1.5m");


   $('entrance-sensor').on('tap',function(value){
	
	$('ambiance').toggle();

   })


   $('entrance-sensor').on('any',function(value){
	// logData
   });

 
   $('entrance-sensor').on('touch',function(value){
	 var light = ext.find('[lighting,out]'); 

	 if (value.greater(.5)) {	
		light.find('[mid|high]').enable();
		light.find('[night]').disable();
	 }  else {
		light.find('[mid|low]').enable();
		light.find('[night]').disable();
	 }

   });



*/
