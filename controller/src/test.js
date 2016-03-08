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
var pal = require('./palette');
var bootstrapFn = require('./bootstrap');


/* test area */

var net = new MCU.Network();
net.bootstrap(bootstrapFn).then (function($) {
   
    var states = {};

    $('b3').on("change",function(value){
        if (value.value) { return; }
        states.b3 = states.b3 || 0;
        states.b3 = (states.b3)?0:1;
        $(':out1').enable(states.b3);
    });

    $('b4').on("change",function(value){
        if (value.value) { return; }
        states.b4 = states.b4 || 0;
        states.b4 = (states.b4)?0:1;
        $(':out2').enable(states.b4);
    });

    $('b2').on("change",function(value){
        if (value.value) { return; }
        states.b2 = states.b2 || 0;
        states.b2 = (states.b2)?0:1;
        $(':out3').enable(states.b2);
        $(':out4').enable(states.b2);
    });

    $(':out').enable(0); 

    var colIndex = 0;
    var cycleIndex = 0;
    setInterval(function(){

            if (cycleIndex >= 5) {
                cycleIndex = 0;
                colIndex++;
            }
            cycleIndex++;
            var currPal = pal[colIndex];

            $('itf1 :rgb').helper.toRGB(currPal[(1+cycleIndex)%5]);
            $('xitf1 :rgb').helper.toRGB(currPal[(3+cycleIndex)%5]);
            $('itf0 :rgb').helper.toRGB(currPal[(0+cycleIndex)%5]);
            $('xitf0 :rgb').helper.toRGB(currPal[(2+cycleIndex)%5]);

            console.log('rendering ',
			currPal[(0+cycleIndex)%5],
			currPal[(1+cycleIndex)%5],
 			currPal[(2+cycleIndex)%5],
			currPal[(3+cycleIndex)%5]);
     }

    ,2000);

    setInterval(function(){ $('i1').toggle(); },300);
    return;
   	
});


