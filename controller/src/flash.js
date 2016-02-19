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



var nodeId = 17;
var interfaceId = 0;
var delay = 5000;


exec(cmd, function(error,stdout,stderr) {

   console.log (arguments);
   console.log ('--------');

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

      console.log('Found .flashstart section. offset:',headerData[3]," '"+offset+"' size:",headerData[0]," '"+size);

      // seding buffer in 18 bytes chunk.

      var n = paddedSize / 18;
      console.log('Sending data in ',n,' chunk(s)'); 
      var i,j=0,k=0;
      console.log('Transfer preview follows: (nodeid:',nodeId,' interface:',interfaceId,')'); 
      for (i=0;i<n;i++) { 
        
        getBufferMsg(nodeId,interfaceId,buffer,i*18,i,true);
      	setTimeout(function() { getBufferMsg(nodeId,interfaceId,buffer,j,k); j+= 18; k++; },delay*(i+1));
      } 
      console.log('Transfer starts in 5s. Ctrl-C to stop Now!');  
}

});


