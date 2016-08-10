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

var MCU = require('../core');
var q = require ('q');

var officeService = function(net) {
    this.network = net;

};

officeService.prototype.onRegisterHardware = function(deferred) {

    this.accessNetwork(function(net,$) {

        // Main LED bar in office, nodeId = 18, key = office-east.

        net.add('main-led-office', 18);
        $('main-led-office').add('interface', 0x00);

        (function (i) {
            i.add('led-1', 'digital out 2.3').tag("out white1").inverted();
            i.add('led-2', 'digital out 3.4').tag("out white2").inverted();
            i.add('led-3', 'digital out 2.5').tag("out white3").inverted();
            i.refresh();
        })($('main-led-office interface'));

        // East switch, nodeID = 26, key = office-east

         net.add('office-east',26);
         var node = $('office-east');
         node.add('interface',0);

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
         })(node.find('interface'));

        /// done hardware registering
        deferred.resolve();
    });
};

officeService.prototype.onStart = function(deferred) {

    var self = this;
    this.accessNetwork(function(net,$) {

        /// register logic
        //setInterval(function() {
        $('main-led-office :white2').disable();
        //},2000);
        /// done register logic

        $('office-east b1').on("change",function(){

                console.log ('changed');

        },self);



        deferred.resolve('run');
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