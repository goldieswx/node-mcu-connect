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


/* test area */

var net = new MCU.Network();

net.ready.then(function($){

    console.log('ready');
    $(':switch').on("change",function(value,io) {
        if (!value.value) {
            io._interface.find('led-'+io.key).toggle();
        }
    });

    $('b1').on("change",function(value,io) {
        if (!value.value) {
            $(':white').toggle();
        }
    });

  $('b2').on("change",function(value,io) {
       value.lastVal = value.lastVal || 0;  
       if (!value.value) {
       
             value.lastVal = (value.lastVal==1)?999:1; 
             $(':rgb').pwm(value.lastVal);
	  }
    });
  $('b3').on("change",function(value,io) {
    });
$('b4').on("change",function(value,io) {
       value.lastVal = value.lastVal || 0;  
       if (!value.value) {
       
             value.lastVal = (value.lastVal==1)?500:1; 
     		$(':room :out').toggle(); 
          }
    });



    $(':out').enable();
    setInterval(function(){ $(':led').toggle();  },3000);

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
        i.add('led-1','pwm out 2.1').tag("out out1 red rgb").inverted();
        i.add('led-2','digital out 2.3').tag("out out1 white").inverted();
        i.add('led-3','pwm out 3.3').tag("out out1 blue rgb").inverted();
        i.add('led-4','digital out 3.5').tag("out out1 white").inverted();
        i.add('led-5','pwm out 3.6').tag("out out1 green rgb").inverted();
        i.add('led-6','digital out 2.4').tag("out out1 white").inverted();
        i.refresh();
    })($('room-led interface-south'));

 (function(i) {
        i.add('led-1','pwm out 2.1').tag("out out1 red rgb").inverted();
        i.add('led-2','digital out 2.3').tag("out out1 white").inverted();
        i.add('led-3','pwm out 3.3').tag("out out1 blue rgb").inverted();
        i.add('led-4','digital out 3.5').tag("out out1 white").inverted();
        i.add('led-5','pwm out 3.6').tag("out out1 green rgb").inverted();
        i.add('led-6','digital out 2.4').tag("out out1 white").inverted();
        i.refresh();
    })($('room-led-2 interface-south'));




    (function(i) {
        i.add('led-1','pwm out 2.1').tag("out out1 red rgb").inverted();
        i.add('led-2','digital out 2.3').tag("out out1 white").inverted();
        i.add('led-3','pwm out 3.3').tag("out out1 blue rgb").inverted();
        i.add('led-4','digital out 3.5').tag("out out1 white").inverted();
        i.add('led-5','pwm out 3.6').tag("out out1 green rgb").inverted();
        i.add('led-6','digital out 2.4').tag("out out1 white").inverted();
        i.refresh();
    })($('main-led-north interface-living'));

    (function(i) {
        i.add('led-1','pwm out 2.1').tag("out out2 red rgb").inverted();
        i.add('led-2','digital out 2.3').tag("out out2 white").inverted();
        i.add('led-3','pwm out 3.3').tag("out out2 blue rgb").inverted();
        i.add('led-4','digital out 3.5').tag("out out2 white").inverted();
        i.add('led-5','pwm out 3.6').tag("out out2 green rgb").inverted();
        i.add('led-6','digital out 2.4').tag("out out2 white").inverted();
        i.refresh();
    })($('main-led-north interface-dining'));


    (function(i) {
        i.add('led-1','pwm out 2.1').tag("out out3 red rgb").inverted();
        i.add('led-2','digital out 2.3').tag("out out3 white").inverted();
        i.add('led-3','pwm out 3.3').tag("out out3 green rgb").inverted();
        i.add('led-4','digital out 3.5').tag("out out3 white").inverted();
        i.add('led-5','pwm out 3.6').tag("out out3 blue rgb").inverted();
        i.add('led-6','digital out 2.4').tag("out out3 white").inverted();
        i.refresh();
    })($('main-led-south interface-living'));

    (function(i) {
        i.add('led-1','pwm out 2.1').tag("out out4 red rgb").inverted();
        i.add('led-2','digital out 2.3').tag("out out4 white").inverted();
        i.add('led-3','pwm out 3.3').tag("out out4 blue rgb").inverted();
        i.add('led-4','digital out 3.5').tag("out out4 white").inverted();
        i.add('led-5','pwm out 3.6').tag("out out4 green rgb").inverted();
        i.add('led-6','digital out 2.4').tag("out out4 white").inverted();
        i.refresh();
    })($('main-led-south interface-dining'));


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

//21 NEW ONE
registerSwitchNode('room-south'     ,22,3200);
registerSwitchNode('dressing'     ,24,1000);
registerSwitchNode('room-north'     ,23,3400);

registerSwitchNode('office-east'     ,26,1500);
    registerSwitchNode('office-west'     ,27,2000);
    registerSwitchNode('hall-west'       ,29,2500); //29;
    registerSwitchNode('living-fire'     ,30,3000);



    setTimeout(function(){net._registerHardwareDeferred.resolve(net.find.bind(net))},3500);



});
