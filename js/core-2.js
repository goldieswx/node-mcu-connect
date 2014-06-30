
var dgram = require('dgram');

var MCUInterface = function(settings) {

	var message = new Buffer("                        ");
	var self = this;
	message.writeUInt32LE(3,20);

	this._listenntingSocket = dgram.createSocket('udp4');
	
	this._listenntingSocket.on("message",function(msg,rinfo){
		 self._callback(msg,rinfo);
	});

	this._listenntingSocket.bind(9931);

	setInterval(function() {
	var client = dgram.createSocket("udp4");
		client.send(message, 0, message.length, 9930,'localhost', function(err, bytes) {
			client.close();
		});
	},1000);

};


MCUInterface.prototype._callback = function(msg,rinfo) {

	console.log(msg);

};

var mi = new MCUInterface();
