/*
    node-mcu-connect . node.js UDP Interface for embedded devices.
    Copyright (C) 2013-4 David Jakubowski

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


var dgram = require('dgram');
var lowLevelArduinoInterface = require('./low-level.js');

var ArduinoInterface = function(arduinoSettings) {

  var $this = this;
  this.aliases = {};
  this.selectInterfaces = [];

  this._initializeAliases();

  this.client = dgram.createSocket("udp4");
  this.settings = arduinoSettings;

  this._listenntingSocket = dgram.createSocket('udp4');
  this._listenntingSocket.on("message",function(msg,rinfo){
     $this._onUDPReceiveMessage(msg,rinfo);
  });

  this._listenntingSocket.bind(8888);

  this._analogNotifications = [];
  this._digitalNotifications = [];
  this._initializeNotifications();

  // tell ardino we are alive and are listenning.
  this._sendMessage(lowLevelArduinoInterface.listenMessage());
};


ArduinoInterface.const = { HIGH:1, LOW:0, ANALOG: 2, DIGITAL: 1, state: { initial:-1,loTreshold:2,hiTreshold:3,high:1,low:0} };

ArduinoInterface.prototype.select = function($select) {
    for (var i=0;i<this.selectInterfaces.length;i++) {
      if (this.selectInterfaces[i].selector === $select) {
        return this.selectInterfaces[i];
      }
    }
    var $interface = new ArduinoInterfaceSelect(this,$select);
    this.selectInterfaces.push($interface);
    return $interface;
};

ArduinoInterface.prototype._initializeNotifications = function() {

  for(var i=0;i<10;i++) {
      this._analogNotifications.push({available:true});
  }

  for(var i=0;i<10;i++) {
      this._digitalNotifications.push({available:true});
  }
};

ArduinoInterface.prototype.getAnalogNotificationId = function() {

  for(var i=0;i<this._analogNotifications.length;i++) {
      if (this._analogNotifications[i].available) return i;
  }
  return null;
};

ArduinoInterface.prototype.releaseAnalogNotificationId = function(id) {

  if (id<this._analogNotifications.length) {
    this._analogNotifications[id].available = true;
  }
};

ArduinoInterface.prototype.getDigitalNotificationId = function() {

  for(var i=0;i<this._digitalNotifications.length;i++) {
      if (this._digitalNotifications[i].available) return i;
  }
  return null;
};

ArduinoInterface.prototype.releaseDigitalNotificationId = function(id) {

  if (id<this._digitalNotifications.length) {
    this._digitalNotifications[id].available = true;
  }
};


ArduinoInterface.prototype._initializeAliases = function() {

   for(var i=0;i<6;i++) {
     var alias = {};
     alias.type = ArduinoInterface.const.ANALOG;
     alias.pinId = i;
     this.aliases['A/'+i] = alias;
   }

   for(var i=0;i<14;i++) {
     var alias = {};
     alias.type = ArduinoInterface.const.DIGITAL;
     alias.pinId = i;
     this.aliases['D/'+i] = alias;
   }

}

ArduinoInterface.prototype._sendMessage = function(message) {

   console.log(message);
   var client = dgram.createSocket("udp4");
   client.send(message, 0, message.length, this.settings.port, this.settings.host, function(err, bytes) {
      client.close();
   });

};

ArduinoInterface.prototype._onUDPReceiveMessage = function(msg,rinfo) {

    var sMsg = msg.toString();
    var bMsg = {};

    if (sMsg.indexOf('ST/A') == 0) {
        bMsg.type = ArduinoInterface.const.ANALOG;
    }

    if (sMsg.indexOf('ST/D') == 0) {
        bMsg.type = ArduinoInterface.const.DIGITAL;
    }

    var tmp = sMsg.substr(4).split('/');
    bMsg.pinId = parseInt(tmp[0]);
    bMsg.value = parseInt(tmp[1]);


    for (var i=0;i<this.selectInterfaces.length;i++) {
       this.selectInterfaces[i]._callback(bMsg,sMsg);
    }

    console.log(sMsg);
};

var ArduinoInterfaceSelect = function($arduinoInterface,selector){
   this._arduinoInterface = $arduinoInterface;

   this.selector = selector;
   this.alias = this._arduinoInterface.aliases[selector];

   this._callbacks = [];
   this._delayExecution = 0;
   this._timeouts = [];
};

ArduinoInterfaceSelect.prototype.clone = function() {

   var clone = new ArduinoInterfaceSelect(this._arduinoInterface,this.selector);
   clone._callbacks = this._callbacks;
   clone._timeouts = this._timeouts;
   return clone;

}


/*
* returns a clone of the select interface bound to the same interface
* with delayed execution (of enabled/disable)
*/
ArduinoInterfaceSelect.prototype.every = function (duration) {

    var clone = this.clone();
    clone._delayExecution = clone._delayExecution + (parseInt(duration)*1000);
    clone._executionType = 'recurring';
    return clone;
};

ArduinoInterfaceSelect.prototype.on = function($event,$function) {

     //console.log($event);
     var event = new ArduinoInterfaceEvent($event,$function,this._arduinoInterface,this);
     this._callbacks.push(event);

};

ArduinoInterfaceSelect.prototype._execute = function(fn){

   if (this._delayExecution) {
      if (this._executionType != 'recurring') {
        this._timeouts.push(setTimeout(fn,this._delayExecution));
      } else {
        this._timeouts.push(setInterval(fn,this._delayExecution));
      }
  } else {
     fn();
   }

};

ArduinoInterfaceSelect.prototype.pwm = function(value,option) {

   var _this = this;
   var _arguments = arguments;

   var setPWMFn = function() {

     _this._arduinoInterface._sendMessage(
     lowLevelArduinoInterface.setAnalogStateMessage(_this.alias.pinId,value));

   }
   this._execute(function(){setPWMFn();});
   return this;


}

ArduinoInterfaceSelect.prototype.enable = function(option) {

   var _this = this;
   var _arguments = arguments;

   var enableFn = function() {

   if (_arguments.length && option.length) {
      _this._arduinoInterface._sendMessage(
      lowLevelArduinoInterface.setDigitalStateMessageDuration(_this.alias.pinId,ArduinoInterface.const.HIGH,parseInt(option)));
   } else {
      _this._arduinoInterface._sendMessage(
      lowLevelArduinoInterface.setDigitalStateMessage(_this.alias.pinId,ArduinoInterface.const.HIGH));
   }

   }
   this._execute(function(){enableFn();});
   return this;
};

ArduinoInterfaceSelect.prototype.disable = function(option) {

    var _this = this;
    var _arguments = arguments;

    var disableFn = function() {
      _this._arduinoInterface._sendMessage(
      lowLevelArduinoInterface.setDigitalStateMessage(_this.alias.pinId,ArduinoInterface.const.LOW));
    };

   this._execute(function(){disableFn();});
   return this;
}

ArduinoInterfaceSelect.prototype.refresh = function() {

  if (this.alias.type == ArduinoInterface.const.ANALOG) {

    var _this = this;
    var _arguments = arguments;

    var refreshFn = function() {
      _this._arduinoInterface._sendMessage(
      lowLevelArduinoInterface.getAnalogStateMessage(_this.alias.pinId));
    };

   this._execute(function(){refreshFn();});
   return this;

  }

}

ArduinoInterfaceSelect.prototype._matchAlias = function(bMsg) {
    return ((bMsg.type == this.alias.type) && (bMsg.pinId == this.alias.pinId));
}


ArduinoInterfaceSelect.prototype._callback = function(bMsg) {

   if (this._matchAlias(bMsg)) {
      for(var i =0;i<this._callbacks.length;i++) {
          this._callbacks[i].trigger(bMsg);
      }
   }

}

/*
 * cancels all settimeouts created by in/_delayExecutions.
 */
ArduinoInterfaceSelect.prototype.unbind = function() {
   for(var i=0;i<this._timeouts.length;i++) {
     clearTimeout(this._timeouts[i]);
   }
   this._timeouts.length = 0; // clear timeout array http://stackoverflow.com/questions/1232040/how-to-empty-an-array-in-javascript
};

/*
* returns a clone of the select interface bound to the same interface
* with delayed execution (of enabled/disable)
*/
ArduinoInterfaceSelect.prototype.in = function (duration) {

    var clone = this.clone();
    clone._delayExecution = clone._delayExecution + (parseInt(duration)*1000);
    clone._executionType = 'single';
    return clone;

};

var ArduinoInterfaceEvent = function(settings,$function,arduinoInterface,arduinoSelectInterface) {

    this.settings = settings;
    this._callback = $function;
    this._arduinoInterface = arduinoInterface;
    this._arduinoSelectInterface = arduinoSelectInterface;

    this._maps = {};
    this.tresholdState = ArduinoInterface.const.state.initial;

    this._mapsImpl =  {
        ntc:function(settings,value,type) {

            var forward = function(settings,value) {
              var vm = settings.vI*value/settings.scale;
              var rm = settings.rI*vm/(settings.vI-vm);
              var t = 1/((Math.log(rm/settings.r25)/settings.b25)+(1/298.15));
              return Math.round(100*(-273.15+t))/100;
            };

            var reverse = function(settings,value) {
              var rm = settings.r25*Math.exp(settings.b25*((1/(value+273.15))-(1/298.15)));
              return Math.round((rm/(settings.rI+rm))*settings.scale);
            };

            if (type=='forward') {
               return forward(settings,value);
            }
            if (type=='reverse') {
               return reverse(settings,value);
            }
            if (type=='treshold') {
               var treshold = reverse(settings,value.value);
               var hiRange  = reverse(settings,value.value + value.hiRange) - treshold;
               var loRange  = treshold - reverse(settings,value.value - value.loRange);

               // invert & swap lo/hi if map function is a decreasing function
               var isDecreasingFunction = ((hiRange <= 0) && (loRange <=0));
               return  {
                  value : treshold,
                  hiRange : isDecreasingFunction?-loRange:hiRange,
                  loRange : isDecreasingFunction?-hiRange:loRange,
                  direction : isDecreasingFunction?'decreasing':'increasing'
               }
            }
            return forward(settings,value);
        },
    };

    $function.call(arduinoInterface,{state:ArduinoInterface.const.state.initial,event:this});

    if (settings.state=='change') {
          this.notificationId = this._arduinoInterface.getAnalogNotificationId();

          var message = lowLevelArduinoInterface.notifyDigitalMessage(this.notificationId,
                                                       this._arduinoSelectInterface.alias.pinId);
          this._arduinoInterface._sendMessage(message);
    }

}

ArduinoInterfaceEvent.prototype.trigger = function (message) {

    // if any calculate maps and apply to message.
    for (var i in this._maps) {
       message[i] = this._mapsImpl[this._maps[i].type](this._maps[i],message.value,'forward');
    }

    if (this.settings.state == 'treshold'){

      // check whether lo/hiTreshold and if needed to dispatch Call or not.
      // In case there is a mapping which toogles the increase/decrease function
      // behavior (e.g: ntc), invert hiTreshold with loTreshold and vice versa.

      if (message.value >= this._tresholdParams.value + this._tresholdParams.hiRange) {

        var newState = this._tresholdParams.direction=='increasing'
                      ?ArduinoInterface.const.state.hiTreshold
                      :ArduinoInterface.const.state.loTreshold;
        if (newState != this.tresholdState) {
          this._callback.call(this.arduinoInterface,
                {
                   state:  newState ,
                   event:  this
                }, message);
        }
      }

      if (message.value <= this._tresholdParams.value - this._tresholdParams.loRange) {

         var newState = this._tresholdParams.direction=='increasing'
                      ?ArduinoInterface.const.state.loTreshold
                      :ArduinoInterface.const.state.hiTreshold;

        if (newState != this.tresholdState) {
         this._callback.call(this.arduinoInterface,
              { state: newState,
                event:this
              },message);
       }
      }
    } else if (this.settings.state == 'change'){ 
         this._callback.call(this.arduinoInterface,
              { state: message.value,
                event:this
              },message);
    }

}

ArduinoInterfaceEvent.prototype.setMap = function(name,settings) {
    this._maps[name] = settings;
    return this;
}


ArduinoInterfaceEvent.prototype.setTreshold = function(settings) {

  var tresholdParams = settings;
  tresholdParams.direction = 'increasing';

  /* If a custom map is defined for this treshold settings, use the custom treshold method for cacluating
     mapped treshold value and ranges. Mapping function can be decreasing, in/surjective, ... and thus imply
     non trivial compute of lorange/hirange settings.
     (e.g ntc thermistors, have negative mapping, and logarithmic/exponential scale)
      */
  if (typeof settings.useMap !== 'undefined') {
       tresholdParams = this._mapsImpl[this._maps[settings.useMap].type](this._maps[settings.useMap],settings,'treshold');
  }

  var notificationId = this._arduinoInterface.getAnalogNotificationId();

  var message = lowLevelArduinoInterface.notifyAnalogMessage(notificationId,
                                               this._arduinoSelectInterface.alias.pinId,
                                               tresholdParams.value,
                                               tresholdParams.loRange,
                                               tresholdParams.hiRange);

  this._arduinoInterface._sendMessage(message);

  this._tresholdParams = tresholdParams;
  this._tresholdParams.notificationId = notificationId;

  return this;
}


exports.UDPInterface = ArduinoInterface;
