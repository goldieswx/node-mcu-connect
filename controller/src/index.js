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


var MCU = require('./core');
var net = new MCU.Network();


net.startRemote();

net.registerServices(
    [
      //  { name: 'heating',       impl : '../impl/heating' },
        { name: 'office-service', impl : '../impl/office-service' }
      /*  { name: 'room-lighting', impl : './impl/room-lightning.js' }, */
    ]
);


/*

setTimeout(function() {
    net.reloadService('office-service');
},5000);
*/
//net.addRemoteListener('127.0.0.1',8882);