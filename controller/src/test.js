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


var MCU = require('core');

/* test area */

var net = new MCU.Network();

(function($) {

      net.add('new-node',0x03);
      $('new-node').add('itf0',0x00); 

   	// Entry interface
	(function(i) {
		i.add('button-1','digital in 1.3').tag("in");
		i.add('led-1','digital out 2.3').tag("out");
		i.add('led-2','digital out 3.4').tag("out");
		i.add('led-3','digital out 3.3').tag("out");
		i.add('slider-1','analog in 1.3').tag("in");
		i.refresh();
	})($('itf0'));

	$('button-1').on('change',function(e){
	    console.log('you pushed button 1 [',e.value,']');
	});
	
	$('slider-1').on('change',function(e){
	    console.log('slider 1 has now value: ',e.value);
	});

    	var x = 0;
	var ledArray = [$('led-1'),$('led-2'),$('led-3')];

	setInterval(function(){ $(':out').disable(); (ledArray[x++]).enable; x%=3; },85);

	return;
   	
})(net.find.bind(net));


