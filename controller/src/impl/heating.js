
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

var heatingService = function(net) {
    this.network = net;
    console.log('heating :: constructor');

};

heatingService.prototype.init = function() {

    var deferred = q.defer();
    deferred.resolve();
    console.log('heating :: initialized but not resolved');
    return deferred.promise;

}

heatingService.prototype.run = function() {

    var deferred = q.defer();
    deferred.resolve();
    console.log('heating :: running');
    return deferred.promise;

}

exports.service = heatingService;