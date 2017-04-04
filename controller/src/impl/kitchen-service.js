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

var kitchenService = function(net) {
    this.network = net;
    this.currentStates = {};

};

kitchenService.prototype.onRegisterHardware = function(deferred) {

    this.accessNetwork(function(net,$) {

        let nodeKey,interfaceKey,nodeId;
        let node;

        // Main LED bar in office, nodeId = 18, key = office-east.
        nodeKey = 'kitchen-lightning'; interfaceKey = 'interface'; nodeId = 20;
        net.add(nodeKey, nodeId);
        node = $(nodeKey)
        node.add(interfaceKey, 0x00);

        (function (i) {
            

        i.add('led-2','digital out 1.1').tag("out white");
        i.add('led-4','digital out 1.3').tag("out white");
        i.add('led-6','digital out 3.3').tag("out white");
        i.add('led-7','digital out 3.5').tag("out white");
        i.add('iled-2','digital out 2.3').tag("out white");
        i.add('iled-4','digital out 2.4').tag("out white");
        i.add('iled-6','digital out 1.0').tag("out white");
        i.add('iled-7','digital out 1.2').tag("out white");
        i.add('eled-2','digital out 1.4').tag("out white");
        i.add('eled-4','digital out 3.6').tag("out white");
        i.add('eled-6','digital out 3.4').tag("out white");
        i.add('eled-7','digital out 2.5').tag("out white");
     
        i.refresh();
        })(node.find(interfaceKey));

        /// done hardware registering
        setTimeout(function() {deferred.resolve();},200);
    });
};

kitchenService.prototype.published = function(self) {

    return {
        /**
         * toggle Lighting in kitchen
         */
        toggleLighting : function() {

            self.accessNetwork(function(net,$) {
                $('kitchen-lightning :out').toggle();
                //console.log('lightningToggled');
            });
        }
    };

};

kitchenService.prototype.onStart = function(deferred) {

    var self = this;
    this.accessNetwork(function(net,$) {

       $('kitchen-lightning :out').disable();

        /* office-east Top-Left Switch  cycle led circuits  1/1+2/1+2+3/OFF  */
        /* office-west Top-Left Switch  cycle led circuits  1/1+2/1+2+3/OFF  */


      ///  setInterval(function() { $('kitchen-lightning :out').toggle(); },2000);


        deferred.resolve('run');
    });

};

/**
 * run () is called by the network registration services, calls internal methods to
 * run the service itself.
 * @returns {*|promise}
 */
kitchenService.prototype.run = function() {

    var deferred = q.defer();

    this.onStart(deferred);
    return deferred.promise;

};

/**
 * init() is called by the network registration services, calls internal methods to ensure the
 * hardware is or gets registered.
 * @returns {*|promise}
 */
kitchenService.prototype.init = function() {

    var deferred = q.defer();

    this.onRegisterHardware(deferred);
    return deferred.promise;

}

/**
 * accessNetwork() wraps the necessary network objects and calls the handler along with them.
 * @param handler
 */
kitchenService.prototype.accessNetwork = function(handler) {

    var net = this.network;
    var $ = net.find.bind(net);
    return handler(net,$);

};

kitchenService.prototype.getServicePath = function() {
    return __filename;
}


exports.service = kitchenService;
