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

var _     = require('lodash-node');
var Q     = require('q');

var MCUEvent = function(fn) {

   this.context = {}
   this.callback = fn;
   this.previousValue = null;
   this.value = null; 
   this.previousContext = {};

   // init clearable timer instance.
   this.clearableTimer = MCUEvent._clearableTimer();

}

MCUEvent.prototype.throttle = function (fn,timeout) {

   this._throttleFn = this._throttleFn || _.throttle(fn,timeout,{leading:false,trailing:true});
   this._throttleFn();

}

/**
 * function() averageFilter
 * Creates a average between the currently elapsed n milliseconds of the event values.
 */

MCUEvent.prototype.timedAverageFilter = function() {

	var event = this;
	var averageTimeFrame = 100; // values are kept for average for 50ms

	event.filters = event.filters || {};
	event.filters.timedAverage =  event.filters.timedAverage || {};
	var e = event.filters.timedAverage;
	var sum = 0, len = 0;

	e.ranges = e.ranges || [];
	e.ranges.push({ v: event.value, t: event.context.timestamp } );

	_.each(e.ranges,function(item) {
		if ((event.context.timestamp - item.t) < averageTimeFrame) {
			sum += item.v;
			len++;
		}
	});

	if (len) {
		sum = Math.round(sum/len);
	} else {
		sum = event.value; 
	}

	e.value = sum;

	_.remove(e.ranges,function(item){
		return ((event.context.timestamp - item.t) > averageTimeFrame);
	});

};


MCUEvent._clearableTimer = function() {

	var deferred;
	var killId; 

	var init = function() {
		deferred = Q.defer();
		killId = 0;
	};	init();

	return { 
			start: function (delay) {
				if (killId) { /* do not allow multiple timers, return rejected promise */ 
					var _deferred = Q.defer();	_deferred.reject('Already running');
					return Q(_deferred.promise);
				}
				killId  = setTimeout(function () { deferred.resolve(); init(); },delay);
				return Q(deferred.promise);
			},
			cancel: function(){ 
				clearTimeout(killId); 
				deferred.reject(); 
				init();
			},
	};
 
}

module.exports = MCUEvent;
