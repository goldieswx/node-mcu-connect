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

var roomService = function(net) {
    this.network = net;
    this.currentStates = {};

};

roomService.prototype.onRegisterHardware = function(deferred) {

    this.accessNetwork(function(net,$) {

        // Main LED bar in  xxx, nodeId = xxx, key =xxx

        net.add('room-led',15);
        $('room-led').add('interface-south',0x00).tag('room');
        net.add('room-led-2',14);
        $('room-led-2').add('interface-south',0x00).tag('room');

        (function(i) {
            i.add('led-1','pwm out 2.1').tag("out red rgb").inverted();
            i.add('led-2','digital out 2.3').tag("out white1").inverted();
            i.add('led-3','pwm out 3.3').tag("out green rgb").inverted();
            i.add('led-4','digital out 3.5').tag("out white2").inverted();
            i.add('led-5','pwm out 3.6').tag("out blue rgb").inverted();
            i.add('led-6','digital out 2.4').tag("out white3").inverted();
            i.refresh();
        })($('room-led interface-south'));

        (function(i) {
            i.add('led-1','pwm out 2.1').tag("out red rgb").inverted();
            i.add('led-2','digital out 2.3').tag("out white1").inverted();
            i.add('led-3','pwm out 3.3').tag("out green rgb").inverted();
            i.add('led-4','digital out 3.5').tag("out white2").inverted();
            i.add('led-5','pwm out 3.6').tag("out blue rgb").inverted();
            i.add('led-6','digital out 2.4').tag("out white3").inverted();
            i.refresh();
        })($('room-led-2 interface-south'));


       // North switch, nodeID = 23, key = room-north
        let nodeKey = 'room-north', interfaceKey = 'interface', nodeId = 23, node;
        net.add(nodeKey, nodeId);
        node = $(nodeKey);
        node.add(interfaceKey, 0x00);

        (function(i) {
            i.add('b1','digital in 2.5').tag('switch in left top');
            i.add('b2','digital in 1.2').tag('switch in right top');
            i.add('b3','digital in 2.4').tag('switch in left bottom');
            i.add('b4','digital in 1.3').tag('switch in right bottom');
            i.add('led-b1','digital out 3.4').tag('room out led left top orange');
            i.add('led-b2','digital out 2.1').tag('room out led right top red');
            i.add('led-b3','digital out 2.3').tag('room out led left bottom green');
            i.add('led-b4','digital out 3.3').tag('room out led right bottom blue');
            i.refresh();
        })(node.find(interfaceKey));

        // South switch, nodeID = 22, key = room-north
        nodeKey = 'room-south'; interfaceKey = 'interface';  nodeId = 22; 
        net.add(nodeKey, nodeId);
        node = $(nodeKey);
        node.add(interfaceKey, 0x00);

        (function(i) {
            i.add('b1','digital in 2.5').tag('switch in left top');
            i.add('b2','digital in 1.2').tag('switch in right top');
            i.add('b3','digital in 2.4').tag('switch in left bottom');
            i.add('b4','digital in 1.3').tag('switch in right bottom');
            i.add('led-b1','digital out 3.4').tag('room out led left top orange');
            i.add('led-b2','digital out 2.1').tag('room out led right top red');
            i.add('led-b3','digital out 2.3').tag('room out led left bottom green');
            i.add('led-b4','digital out 3.3').tag('room out led right bottom blue');
            i.refresh();
        })(node.find(interfaceKey));
        

	// Dressing switch, nodeID = 30, key = dresssing
        nodeKey = 'dressing'; interfaceKey = 'interface'; nodeId = 30;
        net.add(nodeKey, nodeId);
        node = $(nodeKey);
        node.add(interfaceKey, 0x00);

        (function(i) {
            i.add('b1','digital in 2.5').tag('switch in left top');
            i.add('b2','digital in 1.2').tag('switch in right top');
            i.add('b3','digital in 2.4').tag('switch in left bottom');
            i.add('b4','digital in 1.3').tag('switch in right bottom');
            i.add('led-b1','digital out 3.4').tag('out led left top blue');
            i.add('led-b2','digital out 2.1').tag('out led right top red');
            i.add('led-b3','digital out 2.3').tag('out led left bottom green');
            i.add('led-b4','digital out 3.3').tag('out led right bottom orange');
            i.refresh();
        })(node.find(interfaceKey));

        /// done hardware registering
        setTimeout(function() {deferred.resolve();},200);
    });
};



roomService.prototype.onStart = function(deferred) {

    var self = this;
    this.accessNetwork(function(net,$) {


        $('room-north :out').disable();
        $('room-south :out').disable();
        $('dressing :out').disable();
        $(':room :out').disable();
        $(':room :rgb').pwm(0);


        let cycleFn =  net.services('helper').cycle(self.currentStates,'mainLEDCycle',':room :white');

        $('room-north b1').on("change",cycleFn,self);
        $('room-south b1').on("change",cycleFn,self);
	$('office-east b3').on("change",cycleFn,self);
        $('dressing b4').on("change",cycleFn,self);

        // r=>b g=>r b=>g

	$('dressing b1').on("change",function(e){
            if (!e.value) {
                //console.log(e.value,"B3",e);
                //if (self.currentStates.bathPWMTimeout) {
                 //   clearTimeout(self.currentStates.bathPWMTimeout);
		 //   self.currentStates.bathPWMTimeout = undefined;	
	//	} 
		if ((self.currentStates.bathPWM === undefined)) {
		     self.currentStates.bathPWM = 2500;
	             self.currentStates.bathPWMTimeout = setTimeout(()=> {
			self.currentStates.bathPWM = undefined;
		     },30000);	
		} 

                self.currentStates.bathPWM += 500;
                self.currentStates.bathPWM %= 3500;

                $('interface-bathroom :out').pwm(self.currentStates.bathPWM);
            }
        },self);


        let colors = [
            '3FE500',
            '81E100',
            'C1DD00',
            'D9B300',
            'D57000',
            'D22E00',
            'CE000F',
            'CA004C',
            'C60086',
            'C200BE',
            '8900BF',
            '5200C2',
            '1800C6',
            '0023CA',
            '0062CE',
            '00A3D2',
            '00D5C5',
            '00D987',
            'FFFFFF',
            '808080',
            '000000'];

             let roomColorCycleFn = net.services('helper').colorCycle(self.currentStates,'color1','room-led :rgb',colors);
             let roomColorCycleFn2 = net.services('helper').colorCycle(self.currentStates,'color2','room-led-2 :rgb',colors);
             let allDown = function(value) {
                 net.services('helper').allDown(value);
                 setTimeout(function() { $(':room:led:blue').enable(); } ,50) ;
                 setTimeout(function() { $(':room:led:blue').disable(); } ,30000);
             }


        $('room-north b2').on("change",allDown,self);
        $('room-north b3').on("change",roomColorCycleFn,self);
        $('room-north b4').on("change",roomColorCycleFn2,self);
        $('room-south b2').on("change",allDown,self);
        $('room-south b3').on("change",roomColorCycleFn,self);
        $('room-south b4').on("change",roomColorCycleFn2,self);




//        $('living-fire b1').on("change",cycleLEDs('interface-living'),self);
//        $('living-fire b2').on("change",cycleLEDs('interface-dining'),self);

        deferred.resolve('run');
    });

};

/**
 * run () is called by the network registration services, calls internal methods to
 * run the service itself.
 * @returns {*|promise}
 */
roomService.prototype.run = function() {

    var deferred = q.defer();

    this.onStart(deferred);
    return deferred.promise;

};

/**
 * init() is called by the network registration services, calls internal methods to ensure the
 * hardware is or gets registered.
 * @returns {*|promise}
 */
roomService.prototype.init = function() {

    var deferred = q.defer();
    this.onRegisterHardware(deferred);
    return deferred.promise;

}

/**
 * accessNetwork() wraps the necessary network objects and calls the handler along with them.
 * @param handler
 */
roomService.prototype.accessNetwork = function(handler) {

    var net = this.network;
    var $ = net.find.bind(net);
    return handler(net,$);

};

roomService.prototype.getServicePath = function() {
    return __filename;
}


exports.service = roomService;
