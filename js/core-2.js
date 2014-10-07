var util = require('util');
var dgram = require('dgram');
var _ = require('lodash-node');

var MCUObject = function(key) {

  this.children = [];
  this.key = key; 
  this.aliases = [];
  this.childType = "child";

};

MCUObject.prototype.find = function(selector) {

  var expression  = selector.split('/');
  var selectorKey = expression[1];
  return _.filter(this.children,{key:selectorKey});

};

MCUObject.prototype.alias = function(alias) {
  this.aliases.push(alias);
};

MCUObject.prototype.add  = function(child) {
   this.children.push(child);
   return child;
};

var MCUNetwork = function() {
};
util.inherits(MCUNetwork,MCUNetwork);

MCUNetwork.prototype.add = function(child) {
   if( _.isUndefined(child)) {
   	child = new MCUNode();	
   }
   return this._super.add(child);
};


var MCUNode = function() {

   this.childType = "node";

};

util.inherits(MCUNode,MCUObject);

var MCUInterface = function() {

   this.childType = "interface";

};


util.inherits(MCUInterface,MCUObject);


var MCUIo = function() {
	
   this.childType = "io";

};

util.inherits(MCUIo,MCUObject);


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


var MCUInterface = function(settings) {

        var messages = [];
        self = this;

        var BIT6 = 0x40;
		var BIT7 = 0x80;
        
        for (var i=0;i<3;i++) {
        messages.push( new Buffer("                        "));
        messages[i].writeUInt32LE(0x00,0);
        messages[i].writeUInt32LE(0x00,1); 
     	messages[i].writeUInt32LE(3+i,20);
        }

        for (var i=0;i<3;i++) {
        messages.push( new Buffer("                        "));
        messages[i+3].writeUInt32LE(BIT7,0);
        messages[i+3].writeUInt32LE(BIT7,1); 
     	messages[i+3].writeUInt32LE(3+i,20);
        }

        for (var i=0;i<3;i++) {
        messages.push( new Buffer("                        "));
        messages[i+6].writeUInt32LE(BIT6,0);
        messages[i+6].writeUInt32LE(BIT6,1); 
     	messages[i+6].writeUInt32LE(3+i,20);
        }



  	this._listenntingSocket = dgram.createSocket('udp4');

	this._listenntingSocket.on("message",function(msg,rinfo){
		 self._callback(msg,rinfo);
	});

	this._listenntingSocket.bind(9931);

        this.current = 0;

	setInterval(function() {
	var client = dgram.createSocket("udp4");
                 self.current %= 9;  
                var message = messages[self.current++];

                client.send(message, 0, message.length, 9930,'localhost', function(err, bytes) {
			client.close();
		});
	},150);

};


MCUInterface.prototype._callback = function(msg,rinfo) {

	console.log(msg);

};

var mi = new MCUInterface();
