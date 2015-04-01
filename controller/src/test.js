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


var MCU = require('core.js');

/* test area */

var net = new MCU.Network();

(function($) {

    //net.add('node-entry',0x03).add('entry',0x01);
      net.add('new-node',0x03).add('itf',0x01);
      $('new-node').add('itf0',0x00); 
   //net.add('sync',0x09).add('sync',0x01).add('sync','digital out 1.0').tag('sync');
   
   	// Entry interface
    (function(i) {
		i.add('i0','digital in 1.3').tag("in");
		i.refresh();
	})($('itf0'));

    (function(i) {
		i.add('lg-1','digital out 2.3').tag("out");
		i.add('lg-2','digital out 3.4').tag("out");
		i.add('lg-3','digital out 3.3').tag("out");
		i.add('i1','digital in 1.3').tag("in");
		i.refresh();
	})($('itf'));



	var arrow = [$('lg-1'),$('lg-2'),$('lg-3')];
    var x = 0;
    	
    $('i1').on('change',function(e){
    	console.log('i1',e.value);
    });

    $('i0').on('change',function(e){
    	console.log('i0',e.value);
    });


	setInterval(function(){ $(':out').enable(0); (arrow[x++]).enable(1); x%=3; },85);

	return;
   	
})(net.find.bind(net));


