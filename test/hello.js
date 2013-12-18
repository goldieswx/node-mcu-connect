/*
    node-arduino. UDP Interface for arduino and embedded devices.
    Copyright (C) 2013 David Jakubowski

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

var arduino = require('../js/core.js');

var ai = new arduino.ArduinoInterface({host:'192.168.0.177', port: 8888, analogScale:1024});

ai.select('D/8').disable();

ai.select('D/4').on({state:'any'},function(e,bMsg){
 console.log (e,bMsg);
});

var ntc1Sensor = { type:'ntc',
                   b25:3988,
                   rI:22000,
                   vI:5,
                   r25:10000 };

ai.select('A/0').on({state:'treshold'},function(e,bMsg){

   if (e.state == ArduinoInterface.const.state.initial) {
       console.log("initial");

       e.event.setMap('ntc1',ntc1Sensor)
              .setTreshold({
                  value   :   26,
                  loRange :   0,
                  hiRange :   4,
                  useMap  :   'ntc1'});

      return;
   }

   if (e.state == ArduinoInterface.const.state.loTreshold) {
      console.log("loTreshold, enabling in 5s",bMsg);

      ai.select('D/9').unbind();
      ai.select('A/0').unbind();

      ai.select('D/9').in('5s').enable('300s');
      ai.select('A/0').every('450s').refresh();

   }

   if (e.state == ArduinoInterface.const.state.hiTreshold) {
      console.log("hiTreshold, disable in 5s",bMsg);

      ai.select('D/9').unbind();
      ai.select('A/0').unbind();
      ai.select('D/9').in('5s').disable();
   }

   //console.log(e.event);

});



