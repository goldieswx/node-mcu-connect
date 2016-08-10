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
var dgram = require('dgram');
var q     = require('q');

var MCUObject = require('./Object');
var MCUNode = require('./Node');

var MCUNetwork = function() {

  var self = this;

  this.superClass = MCUObject.prototype;
  MCUNetwork.super_.call(this);
  this.childType = "network";
  this._cachedNodeList = {}; // node list lazy loaded and cached by id.
  this._listenntingSocket = dgram.createSocket('udp4');
  this._listenntingSocket.on("message",function(msg,rinfo){
	  self._callback(msg);
  });

  this._listenntingSocket.bind(9931);

  this._services = { items: []};
  this._registerHardwareDeferred = q.defer();
  this.ready = this._registerHardwareDeferred.promise;

};

util.inherits(MCUNetwork,MCUObject);


MCUNetwork.prototype.registerHardware = function(fn) {

  fn(this.find.bind(this));
 // this._registerHardwareDeferred.resolve(this.find.bind(this));

};


MCUNetwork.prototype.add = function(key,nodeId) {

   var child = new MCUNode();
   child.key = key;
   child.id = nodeId;

   return (this.superClass.add.bind(this))(child);

};

MCUNetwork.prototype._sendMessage = function(buffer) {
//console.log(buffer);
	var client = dgram.createSocket("udp4");
		client.send(buffer, 0, buffer.length, 9930,'localhost', function(err, bytes) {
		client.close();
	});

};

MCUNetwork.prototype._dispatchMessage = function(message) {

	// lazy cache of cachedNodeList.
	if(_.isUndefined(this._cachedNodeList[message.destination])) {
		this._cachedNodeList[message.destination] = _.find(this.children,{id:message.destination});
	}

	// dispatch message to right node.
	if (this._cachedNodeList[message.destination]) {
    this._cachedNodeList[message.destination]._callback(message);
  } else {
    console.log('callback required to unregistered node: ',message.destination);
  }

}


/**
 * MCUNetwork::_callback(value)
 *
 * value is received in binary form from udp.
 * extract node number, interface id and io.
 * dispatch right away to node
 *
 */
MCUNetwork.prototype._callback = function(value) {

  	var message = {
		timestamp 	: 	(new Date()).getTime(),
		destination : 	value.readUInt32LE(36),
		trigger 	:  	value.readUInt16LE(16),
		portData 	: [	value.readUInt16LE(10),
						value.readUInt16LE(12),
						value.readUInt16LE(14)],
		adcData 	: [	value.readUInt16LE(0x0),
						value.readUInt16LE(0x2),
						value.readUInt16LE(0x4),
						value.readUInt16LE(0x6),
						value.readUInt16LE(0x8)],
		interfaceId	:	value.readUInt8(19)-1
	}
	this._dispatchMessage(message);
}


/**
 * MCUNetwork::registerServices(array)
 * Receives an array of {name/impl} mapping services and their impementation path (.js)
 */
MCUNetwork.prototype.registerServices = function (_services) {

    // establish a chain of promise with service inits.
    var deferred = q.defer();
    var promise = deferred.promise;

    this._services.items =  _services;
    var services = this._services;

    var net = this;
    var $ = net.find.bind(net);

    var singleServiceRunner = function(service) {

        if (!_.find(services.items,{name:service.name,loaded:true})) {
            console.log ('[',service.name,'] Constructor.');
            try {
                if (service.path) {
                    delete (require.cache[service.path]);
                }
            } catch(e) {
                console.log("[warning] Deleting cache of [",service.name,"]",e);
            }
            service.liveInstance = new (require(service.impl).service)(net);
            service.liveInstance.key = service.name;
            service.path = service.liveInstance.getServicePath();
            service.loaded = true;
        }

        if (!service.initialized) {
            console.log ('[',service.name,'] Initializing.');
            service.initialized = true;
            // newPromise = service.liveInstance.init()
            return  service.liveInstance.init();
        }

        if (!service.running) {
            console.log ('[',service.name,'] Running.');
            service.running = true;
            // newPromise = service.promise;
            return  service.liveInstance.run();
        }
   };

    deferred.resolve('initial');

    _.each(services.items,function(service){
        if (!service.initialized) {
            promise = promise.then(function(x) {

               var ret = singleServiceRunner(service);
               return  ret;
            });
        }
    });

    _.each(services.items,function(service){
        if (!service.running) {
            promise = promise.then(function(x) {
                return singleServiceRunner(service);
            });
        }
    });

};

MCUNetwork.prototype.reloadService = function(serviceName) {

    var svc = _.find(this._services.items,{name:serviceName});

    this._clearCallbacks(serviceName);

    svc.loaded = false;
    svc.running = false;
    svc.initialized = true;

    this.registerServices(this._services.items);

};

MCUNetwork.prototype.startRemote = function() {

    var http = require("http"),
        url = require("url"),
        path = require("path"),
        fs = require("fs"),
        port = 8888;
        self = this;

    http.createServer(function(request, response) {

        var uri = url.parse(request.url).pathname
            , filename = path.join(process.cwd(), uri);

        response.writeHead(200, {"Content-Type": "text/plain"});
        var cmd = uri.split('/');

        var serviceName = cmd.pop();
        var action = cmd.pop();

        if (action === 'reload') {
            self.reloadService(serviceName);
        }

        response.write(uri);
        response.end();


    }).listen(parseInt(port, 10));

}


module.exports = MCUNetwork;
