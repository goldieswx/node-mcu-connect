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


/* test area */

var net = new MCU.Network();

(function($) {

      net.add('new-node',16);
    $('new-node').add('itf0',0x00);
    $('new-node').add('itf1',0x01);

    net.add('led-salon',17);
    $('led-salon').add('xitf0',0x00);
    $('led-salon').add('xitf1',0x01);

    // Entry interface
	(function(i) {
         i.add('discard1','digital out 1.0');
         i.add('discard2','digital out 1.1');
         i.add('discard3','digital out 1.2');
         i.add('discard4','digital out 1.3');
         i.add('discard5','digital out 1.4');
		i.add('led-1','pwm out 2.1').tag("out out1 red rgb").inverted();
		i.add('led-2','digital out 2.3').tag("out out1 white").inverted();
		i.add('led-3','pwm out 3.3').tag("out out1 green rgb").inverted();
		i.add('led-4','digital out 3.5').tag("out out1 white").inverted();
		i.add('led-5','pwm out 3.6').tag("out out1 blue rgb").inverted();
		i.add('led-6','digital out 2.4').tag("out out1").inverted();

		i.refresh();
	})($('itf0'));

    (function(i) {
        i.add('--discard1','digital out 1.0');
        i.add('--discard2','digital out 1.1');
        i.add('--discard3','digital out 1.2');
        i.add('--discard4','digital out 1.3');
        i.add('--discard5','digital out 1.4');
        i.add('--led-1','pwm out 2.1').tag("out out3 red rgb").inverted();
        i.add('--led-2','digital out 2.3').tag("out out3 white").inverted();
        i.add('--led-3','pwm out 3.3').tag("out out3 green rgb").inverted();
        i.add('--led-4','digital out 3.5').tag("out out3 white").inverted();
        i.add('--led-5','pwm out 3.6').tag("out out3 blue rgb").inverted();
        i.add('--led-6','digital out 2.4').tag("out out3").inverted();

        i.refresh();
    })($('xitf0'));

    (function(i) {
        i.add('---discard1','digital out 1.0');
        i.add('---discard2','digital out 1.1');
        i.add('---discard3','digital out 1.2');
        i.add('---discard4','digital out 1.3');
        i.add('---discard5','digital out 1.4');
        i.add('---led-1','pwm out 2.1').tag("out out4 red rgb").inverted();
        i.add('---led-2','digital out 2.3').tag("out out4 white").inverted();
        i.add('---led-3','pwm out 3.3').tag("out out4 green rgb").inverted();
        i.add('---led-4','digital out 3.5').tag("out out4 white").inverted();
        i.add('---led-5','pwm out 3.6').tag("out out4 blue rgb").inverted();
        i.add('---led-6','digital out 2.4').tag("out out4").inverted();

        i.refresh();
    })($('xitf1'));

    (function(i) {
        i.add('-discard1','digital out 1.0');
        i.add('-discard2','digital out 1.1');
        i.add('-discard3','digital out 1.2');
        i.add('-discard4','digital out 1.3');
        i.add('-discard5','digital out 1.4');
        i.add('-led-1','pwm out 2.1').tag("out out2 red rgb").inverted();
        i.add('-led-2','digital out 2.3').tag("out out2 white").inverted();
        i.add('-led-3','pwm out 3.3').tag("out out2 green rgb").inverted();
        i.add('-led-4','digital out 3.5').tag("out out2 white").inverted();
        i.add('-led-5','pwm out 3.6').tag("out out2 blue rgb").inverted();
        i.add('-led-6','digital out 2.4').tag("out out2").inverted();

        i.refresh();
    })($('itf1'));


    net.add('inthall',30);
    $('inthall').add('ihallitf0',0x00);

    // Entry interface
    (function(i) {
        i.add('b1','digital in 1.2');
        i.add('b2','digital in 1.3');
        i.add('b3','digital in 2.5');
        i.add('b4','digital in 2.4');

        i.add('i1','digital out 2.3').tag('leds');
        i.add('led1','digital out 3.3').tag('leds');
        i.add('i3','digital out 3.4').tag('leds');
        i.add('zi14','digital out 2.1').tag('leds');
        i.refresh();
    })($('ihallitf0'));

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



    // $(':leds').enable();

    $(':out').enable(0); // green;
//	$('led-1').disable(); // red;
//	$('led-4').disable(); // wht;
//	$('led-5').disable();

 //   console.log($('itf0 :rgb')); //.helper.toRGB();


    //    $('led-6').enable();
    var colIndex = 0;
    var cycleIndex = 0;
    // $('led-2').enable();
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


            console.log('rendering ',currPal[(0+cycleIndex)%5],currPal[(1+cycleIndex)%5],currPal[(2+cycleIndex)%5],currPal[(3+cycleIndex)%5])

        }

        ,2000);


    // console.log('enabling');
     $(':out1').enable();

	setInterval(function(){ $('i1').toggle(); },300);

	return;
   	
})(net.find.bind(net));


