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

var officeService = function(net) {
    this.network = net;
    this.currentStates = {};

};

officeService.prototype.onRegisterHardware = function(deferred) {

    this.accessNetwork(function(net,$) {

        let nodeKey,interfaceKey,nodeId;
        let node;

        // Main LED bar in office, nodeId = 18, key = office-east.
        nodeKey = 'main-led-office'; interfaceKey = 'interface'; nodeId = 18;
        net.add(nodeKey, nodeId);
        node = $(nodeKey)
        node.add(interfaceKey, 0x00);

        (function (i) {
            i.add('led-1', 'digital out 2.3').tag("out white3").inverted();
            i.add('led-2', 'digital out 3.4').tag("out white2").inverted();
            i.add('led-3', 'digital out 2.5').tag("out white1").inverted();
            i.refresh();
        })(node.find(interfaceKey));

        // East switch, nodeID = 26, key = office-east
        nodeKey = 'office-east'; interfaceKey = 'interface'; nodeId = 26;
        net.add(nodeKey, nodeId);
        node = $(nodeKey);
        node.add(interfaceKey, 0x00);

         (function(i) {
             i.add('b1','digital in 1.3').tag('switch in left top');
             i.add('b2','digital in 2.4').tag('switch in right top');
             i.add('b3','digital in 1.2').tag('switch in left bottom');
             i.add('b4','digital in 2.5').tag('switch in right bottom');
             i.add('led-b1','digital out 3.3').tag('out led left top blue');
             i.add('led-b2','digital out 2.3').tag('out led right top red');
             i.add('led-b3','digital out 2.1').tag('out led left bottom green');
             i.add('led-b4','digital out 3.4').tag('out led right bottom orange');
             i.refresh();
         })(node.find(interfaceKey));

        // East switch, nodeID = 27, key = office-west
        nodeKey = 'office-west'; interfaceKey = 'interface'; nodeId = 27;
        net.add(nodeKey, nodeId);
        node = $(nodeKey);
        node.add(interfaceKey, 0x00);

        (function(i) {
            i.add('b1','digital in 1.3').tag('switch in left top');
            i.add('b2','digital in 2.4').tag('switch in right top');
            i.add('b3','digital in 1.2').tag('switch in left bottom');
            i.add('b4','digital in 2.5').tag('switch in right bottom');
            i.add('led-b1','digital out 3.3').tag('out led left top blue');
            i.add('led-b2','digital out 2.3').tag('out led right top red');
            i.add('led-b3','digital out 2.1').tag('out led left bottom green');
            i.add('led-b4','digital out 3.4').tag('out led right bottom orange');
            i.refresh();
        })(node.find(interfaceKey));

        /// done hardware registering
        setTimeout(function() {deferred.resolve();},200);
    });
};

officeService.prototype.onStart = function(deferred) {

    var self = this;
    this.accessNetwork(function(net,$) {

        try { 
	/* office-east Top-Left Switch  cycle led circuits  1/1+2/1+2+3/OFF  */
        /* office-west Top-Left Switch  cycle led circuits  1/1+2/1+2+3/OFF  */

        $('main-led-office :out').disable();
        $('office-east :out').disable();
        $('office-west :out').disable();

        let cycleFn =  net.services('helper').cycle(self.currentStates,'mainLEDCycle','main-led-office :white');

        $('office-east b1').on("change",cycleFn,self);
        $('office-west b1').on("change",cycleFn,self);

        deferred.resolve('run');
       } catch(e) { console.log('error: officeService:',e); };  

  });

};

/**
 * run () is called by the network registration services, calls internal methods to
 * run the service itself.
 * @returns {*|promise}
 */
officeService.prototype.run = function() {

    var deferred = q.defer();

    this.onStart(deferred);
    return deferred.promise;

};

/**
 * init() is called by the network registration services, calls internal methods to ensure the
 * hardware is or gets registered.
 * @returns {*|promise}
 */
officeService.prototype.init = function() {

    var deferred = q.defer();

    this.onRegisterHardware(deferred);
    return deferred.promise;

}

/**
 * accessNetwork() wraps the necessary network objects and calls the handler along with them.
 * @param handler
 */
officeService.prototype.accessNetwork = function(handler) {

    var net = this.network;
    var $ = net.find.bind(net);
    return handler(net,$);

};

officeService.prototype.getServicePath = function() {
    return __filename;
}


exports.service = officeService;
