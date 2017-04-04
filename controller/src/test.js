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


var MCU = require('./core');
var _ = require('lodash');

/* test area */

var net = new MCU.Network();

net.ready.then(function($){

    var colcycle = ['800000','8B0000','A52A2A','B22222','DC143C','FF0000','FF6347','FF7F50','CD5C5C','F08080','E9967A','FA8072','FFA07A','FF4500','FF8C00','FFA500','FFD700','B8860B','DAA520','EEE8AA','BDB76B','F0E68C','808000','FFFF00','9ACD32','556B2F','6B8E23','7CFC00','7FFF00','ADFF2F','006400','008000','228B22','00FF00','32CD32','90EE90','98FB98','8FBC8F','00FA9A','00FF7F','2E8B57','66CDAA','3CB371','20B2AA','2F4F4F','008080','008B8B','00FFFF','00FFFF','E0FFFF','00CED1','40E0D0','48D1CC','AFEEEE','7FFFD4','B0E0E6','5F9EA0','4682B4','6495ED','00BFFF','1E90FF','ADD8E6','87CEEB','87CEFA','191970','000080','00008B','0000CD','0000FF','4169E1','8A2BE2','4B0082','483D8B','6A5ACD','7B68EE','9370DB','8B008B','9400D3','9932CC','BA55D3','800080','D8BFD8','DDA0DD','EE82EE','FF00FF','DA70D6','C71585','DB7093','FF1493','FF69B4','FFB6C1','FFC0CB','FAEBD7','F5F5DC','FFE4C4','FFEBCD','F5DEB3','FFF8DC','FFFACD','FAFAD2','FFFFE0','8B4513','A0522D','D2691E','CD853F','F4A460','DEB887','D2B48C','BC8F8F','FFE4B5','FFDEAD','FFDAB9','FFE4E1','FFF0F5','FAF0E6','FDF5E6','FFEFD5','FFF5EE','F5FFFA','708090','778899','B0C4DE','E6E6FA','FFFAF0','F0F8FF','F8F8FF','F0FFF0','FFFFF0','F0FFFF','FFFAFA'];

    console.log('ready');
    $(':switch').on("change",function(value,io) {
        if (!value.value) {
            io._interface.find('led-'+io.key).toggle();
        }
    });

    $('b1').on("change",function(value,io) {
       if (io.node.key === 'dressing') return;  
       if (!value.value) {
             value.lastval = value.lastval || 0;        
             value.lastval ++;
	     value.lastval %= 4; 
             var j; 
         //console.log(io.node.key);   
          for (j=1;j<=3;j++) {
    		if (io.node.key != 'living-fire') {
			$(':white:out'+j).enable(j<=(value.lastval));
                } else {
                        $('interface-living :white:out'+j).enable(j<=(value.lastval));
		}  
	    } 
	}
    });

    $('b2').on("change",function(value,io) {
     if (io.node.key === 'dressing') return;  
     value.lastVal = value.lastVal || 0;  
       if (!value.value) {
       
             value.lastVal = (value.lastVal==1)?999:1; 
             $(':rgb').pwm(value.lastVal);
	  }
    });
   $('b3').on("change",function(value,io) {
    if (io.node.key === 'dressing') return;  
    value.index = value.index || 0;
     value.index %= colcycle.length; 
     $(':rgb').helper.toRGB(colcycle[value.index]);
     value.index++;

    });
   $('b4').on("change",function(value,io) {
      if (io.node.key === 'dressing') return;  
 
      value.lastVal = value.lastVal || 0;  
       if (!value.value) {
       
             value.lastVal = (value.lastVal==1)?500:1; 
     		$(':room :out').toggle(); 
          }
    });

   var dressingSwUp =
       function(value,io) {
       if (!value.value) {
             value.lastval = value.lastval || 0;        
             value.lastval ++;
             value.lastval %= 4; 
             var j; 
          for (j=1;j<=3;j++) {
        	if (io.key === 'b3') {
        		$('interface-dining :white:out'+j).enable(j<=(value.lastval));
                } else {
                        $('interface-living :white:out'+j).enable(j<=(value.lastval));
        	}  
	    } 
        }
    };

  
   var diningThrottled = _.throttle(function() {
    $('interface-dining :rgb').pwm(1); 
   }, 3000, { 'leading': false, 'trailing': true });

   var livingThrottled = _.throttle(function() { 
    $('interface-living :rgb').pwm(1); 
   }, 3000, { 'leading': false, 'trailing': true });
 

   var dressingSwDn = function (value,io) {
      if (value.value === 0) {
         if (io.key === 'b2') { livingThrottled(); }; 
         if (io.key === 'b4') { diningThrottled(); }; 
      } else {
         if (io.key === 'b2') { livingThrottled.cancel(); }; 
         if (io.key === 'b4') { diningThrottled.cancel(); }; 
      }
      if (!value.value) {
           value.index = value.index || 0;
           value.index %= colcycle.length; 
            if (io.key === 'b2') { $('interface-living :rgb').helper.toRGB(colcycle[value.index]) };
            if (io.key === 'b4') { $('interface-dining :rgb').helper.toRGB(colcycle[value.index]) };
           value.index++;
  
         }

   };

   $('dressing b1').on("change",dressingSwUp);
   $('dressing b3').on("change",dressingSwUp);
   $('dressing b2').on("change",dressingSwDn);
   $('dressing b4').on("change",dressingSwDn);


    $(':out').disable();
    setInterval(function(){ $(':led').disable();  },30000);
    $('main-led-office :white').disable();

});


net.registerHardware(function($) {

    console.log('registering');

    net.add('room-led',15);
    $('room-led').add('interface-south',0x00).tag('room');
  

   net.add('room-led-2',14);
    $('room-led-2').add('interface-south',0x00).tag('room');
  


    net.add('main-led-north',16);
    $('main-led-north').add('interface-living',0x00);
    $('main-led-north').add('interface-dining',0x01);

    net.add('main-led-south',17);
    $('main-led-south').add('interface-living',0x00);
    $('main-led-south').add('interface-dining',0x01);

    // Entry interface
  (function(i) {
        i.add('led-1','pwm out 2.1').tag("out red rgb").inverted();
        i.add('led-2','digital out 2.3').tag("out white").inverted();
        i.add('led-3','pwm out 3.3').tag("out blue rgb").inverted();
        i.add('led-4','digital out 3.5').tag("out white").inverted();
        i.add('led-5','pwm out 3.6').tag("out  green rgb").inverted();
        i.add('led-6','digital out 2.4').tag("out white").inverted();
        i.refresh();
    })($('room-led interface-south'));

 (function(i) {
        i.add('led-1','pwm out 2.1').tag("out red rgb").inverted();
        i.add('led-2','digital out 2.3').tag("out white").inverted();
        i.add('led-3','pwm out 3.3').tag("out blue rgb").inverted();
        i.add('led-4','digital out 3.5').tag("out white").inverted();
        i.add('led-5','pwm out 3.6').tag("out green rgb").inverted();
        i.add('led-6','digital out 2.4').tag("out white").inverted();
        i.refresh();
    })($('room-led-2 interface-south'));




    (function(i) {
        i.add('led-1','pwm out 2.1').tag("out out1 red rgb").inverted();
        i.add('led-2','digital out 2.3').tag("out out1 white").inverted();
        i.add('led-3','pwm out 3.3').tag("out out2 blue rgb").inverted();
        i.add('led-4','digital out 3.5').tag("out out2 white").inverted();
        i.add('led-5','pwm out 3.6').tag("out out3 green rgb").inverted();
        i.add('led-6','digital out 2.4').tag("out out3 white").inverted();
        i.refresh();
    })($('main-led-north interface-living'));

    (function(i) {
        i.add('led-1','pwm out 2.1').tag("out red rgb").inverted();
        i.add('led-2','digital out 2.3').tag("out out1 white").inverted();
        i.add('led-3','pwm out 3.3').tag("out blue rgb").inverted();
        i.add('led-4','digital out 3.5').tag("out out2 white").inverted();
        i.add('led-5','pwm out 3.6').tag("out green rgb").inverted();
        i.add('led-6','digital out 2.4').tag("out out3 white").inverted();
        i.refresh();
    })($('main-led-north interface-dining'));


    (function(i) {
        i.add('led-1','pwm out 2.1').tag("out red rgb").inverted();
        i.add('led-2','digital out 2.3').tag("out out1 white").inverted();
        i.add('led-3','pwm out 3.3').tag("out green rgb").inverted();
        i.add('led-4','digital out 3.5').tag("out out2 white").inverted();
        i.add('led-5','pwm out 3.6').tag("out blue rgb").inverted();
        i.add('led-6','digital out 2.4').tag("out out3 white").inverted();
        i.refresh();
    })($('main-led-south interface-living'));

    (function(i) {
        i.add('led-1','pwm out 2.1').tag("out red rgb").inverted();
        i.add('led-2','digital out 2.3').tag("out out1 white").inverted();
        i.add('led-3','pwm out 3.3').tag("out blue rgb").inverted();
        i.add('led-4','digital out 3.5').tag("out out2 white").inverted();
        i.add('led-5','pwm out 3.6').tag("out green rgb").inverted();
        i.add('led-6','digital out 2.4').tag("out out3 white").inverted();
        i.refresh();
    })($('main-led-south interface-dining'));


  net.add('main-led-office',18);
    $('main-led-office').add('interface',0x00);
//was 1.2 1.3 3.5 3.6 2.3 3.4 2.4 2.5
    // Entry interface
  (function(i) {
        i.add('led-2','digital out 1.2').tag("out white").inverted();
        i.add('led-4','digital out 1.3').tag("out white").inverted();
        i.add('led-6','digital out 3.3').tag("out white").inverted();
        i.add('led-7','digital out 2.1').tag("out white").inverted();
      i.add('led-2','digital out 3.5').tag("out white").inverted();
        i.add('led-4','digital out 3.6').tag("out white").inverted();
        i.add('led-6','digital out 2.3').tag("out white").inverted();
        i.add('led-7','digital out 3.4').tag("out white").inverted();
        i.add('led-2','digital out 2.4').tag("out white").inverted();
        i.add('led-4','digital out 2.5').tag("out white").inverted();
        i.add('led-6','digital out 3.3').tag("out white").inverted();
        i.add('led-7','digital out 2.1').tag("out white").inverted();
     

     i.refresh();
    })($('main-led-office interface'));




    var registerSwitchNode = function (nodeKey,nodeId,timeOut,interfaceId,interfaceKey) {

        setTimeout(function() {

        interfaceId = interfaceId || 0;
        interfaceKey = interfaceKey || 'interface';

        net.add(nodeKey,nodeId);
        var node = $(nodeKey);
        node.add(interfaceKey,interfaceId);

        // Entry interface
        (function(i) {
            i.add('b1','digital in 1.2').tag('switch in');
            i.add('b2','digital in 1.3').tag('switch in');
            i.add('b3','digital in 2.5').tag('switch in');
            i.add('b4','digital in 2.4').tag('switch in');

            i.add('led-b1','digital out 2.3').tag('out led');
            i.add('led-b2','digital out 3.3').tag('out led');
            i.add('led-b3','digital out 3.4').tag('out led');
            i.add('led-b4','digital out 2.1').tag('out led');
            i.refresh();
        })(node.find(interfaceKey));
    },timeOut);
  };



registerSwitchNode('hall-east'     ,21,500);

registerSwitchNode('room-south'     ,22,3200);
registerSwitchNode('dressing'     ,24,1800);

registerSwitchNode('room-north'     ,23,3400);

registerSwitchNode('office-east'     ,26,1500);
    registerSwitchNode('office-west'     ,27,2000);
    registerSwitchNode('hall-west'       ,29,2200); //29;
    registerSwitchNode('living-fire'     ,30,3000);



    setTimeout(function(){net._registerHardwareDeferred.resolve(net.find.bind(net))},5600);



});
