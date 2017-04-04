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

/* Base service for creating services.
 */

var MCUService = function(net) {

    this.network = net;
    this.currentStates = {};

};

MCUService.prototype.onRegisterHardware = function(deferred) {

   deferred.resolve();

};


MCUService.prototype.onStart = function(deferred) {

    deferred.resolve('run');

};

/**
 * run () is called by the network registration services, calls internal methods to
 * run the service itself.
 * @returns {*|promise}
 */
MCUService.prototype.run = function() {

    var deferred = q.defer();

    this.onStart(deferred);
    return deferred.promise;

};

/**
 * init() is called by the network registration services, calls internal methods to ensure the
 * hardware is or gets registered.
 * @returns {*|promise}
 */
MCUService.prototype.init = function() {

    var deferred = q.defer();

    this.onRegisterHardware(deferred);
    return deferred.promise;

}

/**
 * accessNetwork() wraps the necessary network objects and calls the handler along with them.
 * @param handler
 */
MCUService.prototype.accessNetwork = function(handler) {

    var net = this.network;
    var $ = net.find.bind(net);
    return handler(net,$);

};

MCUService.prototype.getServicePath = function() {
    return __filename;
}

module.exports = MCUService;
