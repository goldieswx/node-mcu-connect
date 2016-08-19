
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

var helperService = function(net) {
    this.network = net;

};

helperService.published = function() {

    return {

        /* function cycle()
         *  @returns a callback function to be used with events
         *  linked to a specific container,key for state (storage)
         *  cycle circuits  1/1+2/1+2+3/OFF
         */

        cycle : function(stateContainer,stateKey,selector) {

            stateContainer[stateKey] = stateContainer[stateKey] | 0;

            return function (value) {
                if (!value.value) {
                    let lastval = stateContainer[stateKey] + 1;
                    lastval %= 4;
                    stateContainer[stateKey] = lastval;

                    for (let j = 1; j <= 3; j++) {
                        $(selector + j).enable(j <= (lastval));
                    }
                }
            };
        }


    }

}

helperService.prototype.init = function() {

    var deferred = q.defer();
    deferred.resolve();
    return deferred.promise;

}

heatingService.prototype.run = function() {

    var deferred = q.defer();
    deferred.resolve();
    return deferred.promise;

}

exports.service = heatingService;