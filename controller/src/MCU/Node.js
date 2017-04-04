/*
    node-mcu-connect . node.js UDP Interface for embedded devices.
    Copyright (C) 2013-5 David Jakubowski
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

var _     = require('lodash');
var util  = require('util');

var MCUObject = require('./Object');
var MCUInterface = require('./Interface');


var MCUNode = function() {

   this.superClass = MCUObject.prototype;
   MCUNode.super_.call(this);
   
   this.childType = "node";
   this._cachedInterfaceList = {}; // list of interfaced indexed by (hardware) id

};

util.inherits(MCUNode,MCUObject);

MCUNode.prototype.add = function(key,id) {

   var child = new MCUInterface();
   child.key = key;
   child.id = id;
   child.node = this;
   return (this.superClass.add.bind(this))(child);

};

MCUNode.prototype._callback = function(message) {


  	// lazy cache of cachedInterfaceList.
	if(_.isUndefined(this._cachedInterfaceList[message.interfaceId])) {
		this._cachedInterfaceList[message.interfaceId] = _.find(this.children,{id:message.interfaceId});
	}

	var _interface = this._cachedInterfaceList[message.interfaceId];
	if (_interface) { _interface._callback(message); } else {
		console.log('unknown interface ',message);
	}

};

module.exports = MCUNode;
