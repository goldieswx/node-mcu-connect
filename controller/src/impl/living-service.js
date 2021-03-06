/*
 node-mcu-connect . node.js UDP Interface for embedded devices.
 Copyright (C) 2013-6 David Jakubowski
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
'use strict';


var MCU = require('../core');
var q = require ('q');
var util  = require('util');


var livingService = function() {
    this.superClass = MCU.Service.prototype;
    livingService.super_.apply(this,arguments);
};

util.inherits(livingService,MCU.Service);


livingService.prototype.onRegisterHardware = function(deferred) {

    this.accessNetwork(function(net,$) {
        // Main LED bar in  xxx, nodeId = xxx, key =xxx

        net.add('main-led-north',16);
        $('main-led-north').add('interface-living',0x00);
        $('main-led-north').add('interface-dining',0x01);

        net.add('main-led-south',17);
        $('main-led-south').add('interface-living',0x00);
        $('main-led-south').add('interface-dining',0x01);

        (function(i) {
            i.add('led-1','pwm out 2.1').tag("out red rgb").inverted();
            i.add('led-2','digital out 2.3').tag("out out2 white").inverted();
            i.add('led-3','pwm out 3.3').tag("out blue rgb").inverted();
            i.add('led-4','pwm out 3.5').tag("out out1 white").inverted();
            i.add('led-5','pwm out 3.6').tag("out green rgb").inverted();
            i.add('led-6','digital out 2.4').tag("out out3 white").inverted();
            i.refresh();
        })($('main-led-north interface-living'));

        (function(i) {
            i.add('led-1','pwm out 2.1').tag("out red rgb").inverted();
            i.add('led-2','digital out 2.3').tag("out out2 white").inverted();
            i.add('led-3','pwm out 3.3').tag("out blue rgb").inverted();
            i.add('led-4','pwm out 3.5').tag("out out1 white").inverted();
            i.add('led-5','pwm out 3.6').tag("out green rgb").inverted();
            i.add('led-6','digital out 2.4').tag("out out3 white").inverted();
            i.refresh();
        })($('main-led-north interface-dining'));


        (function(i) {
            i.add('led-1','pwm out 2.1').tag("out red rgb").inverted();
            i.add('led-2','digital out 2.3').tag("out out2 white").inverted();
            i.add('led-3','pwm out 3.3').tag("out green rgb").inverted();
            i.add('led-4','pwm out 3.5').tag("out out1 white").inverted();
            i.add('led-5','pwm out 3.6').tag("out blue rgb").inverted();
            i.add('led-6','digital out 2.4').tag("out out3 white").inverted();
            i.refresh();
        })($('main-led-south interface-living'));

        (function(i) {
            i.add('led-1','pwm out 2.1').tag("out red rgb").inverted();
            i.add('led-2','digital out 2.3').tag("out out2 white").inverted();
            i.add('led-3','pwm out 3.3').tag("out blue rgb").inverted();
            i.add('led-4','pwm out 3.5').tag("out out1 white").inverted();
            i.add('led-5','pwm out 3.6').tag("out green rgb").inverted();
            i.add('led-6','digital out 2.4').tag("out out3 white").inverted();
            i.refresh();
        })($('main-led-south interface-dining'));



        // East switch, nodeID = 27, key = office-west
        let nodeKey = 'living-fire', interfaceKey = 'interface', nodeId = 24, node;
        net.add(nodeKey, nodeId);
        node = $(nodeKey);
        node.add(interfaceKey, 0x00);

        (function(i) {
            i.add('b1','digital in 2.5').tag('switch in left top');
            i.add('b2','digital in 1.2').tag('switch in right top');
            i.add('b3','digital in 2.4').tag('switch in left bottom');
            i.add('b4','digital in 1.3').tag('switch in right bottom');
            i.add('led-b1','digital out 3.4').tag('out led left top blue');
            i.add('led-b2','digital out 2.1').tag('out led right top green');
            i.add('led-b3','digital out 2.3').tag('out led left bottom red');
            i.add('led-b4','digital out 3.3').tag('out led right bottom orange');
            i.refresh();
        })(node.find(interfaceKey));




        nodeKey = 'hall-east', interfaceKey = 'interface', nodeId = 21, node;
        net.add(nodeKey, nodeId);
        node = $(nodeKey);
        node.add(interfaceKey, 0x00);

        (function(i) {
            i.add('b1','digital in 2.5').tag('switch in left top');
            i.add('b2','digital in 1.2').tag('switch in right top');
            i.add('b3','digital in 2.4').tag('switch in left bottom');
            i.add('b4','digital in 1.3').tag('switch in right bottom');
            i.add('led-b1','digital out 3.4').tag('out led left top orange hall');
            i.add('led-b2','digital out 2.1').tag('out led right top green ok hall');
            i.add('led-b3','digital out 2.3').tag('out led left bottom red hall');
            i.add('led-b4','digital out 3.3').tag('out led right bottom blue ok hall');
            i.refresh();
        })(node.find(interfaceKey));

        nodeKey = 'hall-west', interfaceKey = 'interface', nodeId = 29, node;
        net.add(nodeKey, nodeId);
        node = $(nodeKey);
        node.add(interfaceKey, 0x00);

        (function(i) {
            i.add('b1','digital in 2.5').tag('switch in left top');
            i.add('b2','digital in 1.2').tag('switch in right top');
            i.add('b3','digital in 2.4').tag('switch in left bottom');
            i.add('b4','digital in 1.3').tag('switch in right bottom');
            i.add('led-b1','digital out 3.4').tag('out led left top blue ok hall');
            i.add('led-b2','digital out 2.1').tag('out led right top orange ok hall');
            i.add('led-b3','digital out 2.3').tag('out led left bottom green ok hall');
            i.add('led-b4','digital out 3.3').tag('out led right bottom red hall');
            i.refresh();
        })(node.find(interfaceKey));


        setTimeout(function() {deferred.resolve();},300);
    });
};



livingService.prototype.onStart = function(deferred) {

    var self = this;
    this.accessNetwork(function(net,$) {


        $(':hall:out').disable();

        /* office-east Top-Left Switch  cycle led circuits  1/1+2/1+2+3/OFF  */
        /* office-west Top-Left Switch  cycle led circuits  1/1+2/1+2+3/OFF  */

        setInterval(function() {
            $(':hall:red').enable();
            setTimeout(function() {
                 $(':hall:red').disable();
	    },500);
	},3000);

        self.currentStates.cycleState = {
        }

        $('living-fire :out').disable();
        $('living-fire :orange').disable();
        $('interface-living :out').disable();
        $('interface-dining :out').disable();

        var cycleLEDs = function (nodeKey) {
            return function(value){
                // cycle led circuits  1/1+2/1+2+3/OFF
                if (!value.value) {
                    self.currentStates.cycleState[nodeKey] = self.currentStates.cycleState[nodeKey] | 0;
                    let lastval = self.currentStates.cycleState[nodeKey]+1;

                    if ((nodeKey == 'interface-dining') && (lastval == 4)) {
                        net.services('kitchen').toggleLighting();
                    }

                    lastval %= 4;
                    self.currentStates.cycleState[nodeKey] = lastval;

                    for (let j=1;j<=3;j++) {
                        $(nodeKey + ' :out'+j).enable(j<=(lastval));
                    }
                }
            };
        };

        let allDown = net.services('helper').allDown;

        $('living-fire b2').on("change",cycleLEDs('interface-living'),self);
        $('living-fire b1').on("change",cycleLEDs('interface-dining'),self);

        $('hall-east b2').on("change",cycleLEDs('interface-living'),self);
        $('hall-east b1').on("change",cycleLEDs('interface-dining'),self);
        $('hall-east b4').on("change",allDown,self);


        $('hall-west b2').on("change",cycleLEDs('interface-living'),self);
        $('hall-west b1').on("change",cycleLEDs('interface-dining'),self);


        $('dressing b3').on("change",cycleLEDs('interface-living'),self);
        $('dressing b3').on("change",cycleLEDs('interface-dining'),self);

        $('office-west b2').on("change",cycleLEDs('interface-living'),self);
        $('office-west b2').on("change",cycleLEDs('interface-dining'),self);

        $('living-fire b4').on("change",function(e){
            if (!e.value) {
                //console.log(e.value,"B3",e);
                self.currentStates.livingPWM = self.currentStates.livingPWM | 0;
                self.currentStates.livingPWM += 200;
                self.currentStates.livingPWM %= 3000;

                $('interface-living :out1').pwm(self.currentStates.livingPWM);
            }
        },self);


        $('living-fire b3').on("change",function(e){
            if (!e.value) {

                self.currentStates.diningPWM = self.currentStates.diningPWM | 0;
                self.currentStates.diningPWM += 200;
                self.currentStates.diningPWM %= 3000;

                $('interface-dining :out1').pwm(self.currentStates.diningPWM);
            }
        },self);


        deferred.resolve('run');
    });

};


exports.service = livingService;
