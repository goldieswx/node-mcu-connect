
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
"use strict";


var MCU = require('../core');
var q = require ('q');

var helperService = function(net) {
    this.network = net;

};

helperService.prototype.published = function() {

    var self = this; 
    return {

        /* function cycle()
         *  @returns a callback function to be used with events
         *  linked to a specific container,key for state (storage)
         *  cycle circuits  1/1+2/1+2+3/OFF
         */

        cycle : function(stateContainer,stateKey,selector) {

            stateContainer[stateKey] = stateContainer[stateKey] | 0;
            return function (value) {
                //console.log(value);
                if (!value.value) {
                    let lastval = stateContainer[stateKey] + 1;
                    lastval %= 4;
                    stateContainer[stateKey] = lastval;

                    for (let j = 1; j <= 3; j++) {
	                   self.accessNetwork(function(net,$) { 
       				    $(selector + j).enable(j <= (lastval));
                       });
		            }
                }
            };
        },

        colorCycle : function(stateContainer,stateKey,selector,colorArray) {

            stateContainer[stateKey] = stateContainer[stateKey] | 0;
            var timeOutGradientStartHandler = null;
            var timeOutGradientSpeedHandler = null;
            var intensity = 1.0;
            var deltaIntensity = 0.05;

            return function (value) {
                //console.log(value);
                let lastval = stateContainer[stateKey];

                if (value.value) {

                    console.log('clearing timeouts');
                    clearTimeout(timeOutGradientStartHandler);
                    clearTimeout(timeOutGradientSpeedHandler);
                    timeOutGradientStartHandler = null;
                    timeOutGradientSpeedHandler = null;

                    self.accessNetwork(function(net,$) {
                       $(selector).helper.toRGB(colorArray[lastval++],intensity);
                    });

                    lastval %= colorArray.length;
                    stateContainer[stateKey] = lastval;
                } else {
                    // startTimeout.
                    console.log('starting hanlder in 2S');
                    if (!timeOutGradientStartHandler) {
                        timeOutGradientStartHandler = setTimeout(function() {
                            console.log('handler started');
                            timeOutGradientSpeedHandler = setInterval(function() {
                                console.log('setting intensity to',intensity);
                                if (intensity <= 0.0001) {
                                    deltaIntensity *= -1;
                                }
                                if (intensity >= 1) {
                                    deltaIntensity *= -1;
                                }
                                intensity -= deltaIntensity;
                                self.accessNetwork(function(net,$) {
                                    $(selector).helper.toRGB(colorArray[lastval],intensity);
                                });
                            },250);
                        },2000);
                    }
                }
            };
        },

        allDown :   function(value) {
                self.accessNetwork(function(net,$) {
                    if (!value.value) {
                        $(':out').disable();
                        $(':rgb').pwm(0);
                    }
                });
        }


    }

}

helperService.prototype.init = function() {

    var deferred = q.defer();
    deferred.resolve();
    return deferred.promise;

}

helperService.prototype.run = function() {

    var deferred = q.defer();
    deferred.resolve();
    return deferred.promise;

}

/**
 * accessNetwork() wraps the necessary network objects and calls the handler along with them.
 * @param handler
 */
helperService.prototype.accessNetwork = function(handler) {

    var net = this.network;
    var $ = net.find.bind(net);
    return handler(net,$);

};

helperService.prototype.getServicePath = function() {
    return __filename;
}

exports.service = helperService;
