var dgram = require('dgram');

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
