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

(function($) {

      net.add('new-node',16);
      $('new-node').add('itf0',0x00); 

   	// Entry interface
	(function(i) {
 i.add('ed-1','digital out 1.0');
 i.add('d-1','digital out 1.1');
 i.add('x-1','digital out 1.2');
 i.add('h1','digital out 1.3');
 i.add('ped-1','digital out 1.4');



		i.add('led-1','pwm out 2.1').tag("out red rgb").inverted();
		i.add('led-2','digital out 2.3').tag("out white").inverted();
		i.add('led-3','pwm out 3.3').tag("out green rgb").inverted();
		i.add('led-4','pwm out 3.5').tag("out white").inverted();
		i.add('led-5','pwm out 3.6').tag("out blue rgb").inverted();
		i.add('led-6','digital out 2.4').tag("out").inverted();

		i.refresh();
	})($('itf0'));



	$('led-3').pwm(800); // green;
	$('led-1').pwm(750); // red;
	$('led-4').pwm(500); // wht;
	$('led-5').pwm(250);

    console.log($('itf0 :rgb')); //.helper.toRGB();


 //   $('itf0:rgb').toRGB('BEBEBB',0.4);
        $('led-6').disable();
     $('led-2').disable();


//	setInterval(function(){ $(':out').enable(1); (ledArray[x++]).enable(1); x%=3; },85);

	return;
   	
})(net.find.bind(net));


