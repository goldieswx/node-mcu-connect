// Expermiental flash test

var exec = require('child_process').exec;
var fs = require('fs');

var elfPath = '../../peripheral/adce/src/adce.elf';
var cmd = ('msp430-objdump -h ' + elfPath + ' | grep ".flashstart" ');


var MCUNetwork = require('./MCU/Network');

var net = new MCUNetwork();

var getBufferMsg = function(nodeId,interfaceId,buffer,offset,sequenceNumber) {

          var transmitMsg = new Buffer(24);
          transmitMsg.fill(0);
          transmitMsg.writeUInt8(0x77+interfaceId,0);
          transmitMsg.writeUInt8(sequenceNumber & 0xFF,1);
          buffer.copy(transmitMsg,2,offset,offset+18);
          transmitMsg.writeUInt32LE(nodeId,20); 
          net._sendMessage(transmitMsg); 
  	console.log(transmitMsg);
};



exec(cmd, function(error,stdout,stderr) {

   if (stdout && stdout.length) {
 
      // size, xx, xx, fileoffset   
      var headerData = stdout.match(/[0-9a-fA-F]{8}/g);
      var size = parseInt('0x'+headerData[0]);
      var offset = parseInt('0x'+headerData[3]);

      // read opcode buffer from file.
      var fd = fs.openSync(elfPath,'rs'); 
      var paddedSize = Math.ceil(size/18)*18; 
   
      var buffer = new Buffer(paddedSize);
      buffer.fill(0);
 
      fs.read(fd,buffer,0,size,offset);
      fs.close(fd); 

      var n = paddedSize / 18;
      var i,j=0;
      for (i=0;i<n;i++) { 
      	getBufferMsg(0x17,0x00,buffer,j,i);
        j += 18; 
      } 
  }

   console.log(arguments);
});


