/*
    node-mcu-connect. A node.js UDP Interface for embedded devices.
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

var mcuConnect = require('../js/core.js');
var UI = mcuConnect.UDPInterface; 

var ai = new UI(
            { host:'192.168.0.178', 
              port: 8888, 
              analogScale:1024});

(function ($) {

   $('D/2').on({state:'change'},function(e,bMsg){

      if (e.state == UI.const.state.initial) { 
          $('D/3').disable();
          e.event.myInterruptState = 0;
          return; 
      } 

      if (e.state == UI.const.state.low) {
          e.event.myInterruptState = (e.event.myInterruptState)?0:1;
          
          if (e.event.myInterruptState) { 
            $('D/3').pwm(4);
          } else {
            $('D/3').disable();
          }
      }
  });
  
  /*
  // to come
  // groups
  $('D/2,D/3,D/4').enable().on('change',function() {
      
  });
  
  // aliases
  $('D/2,D/3,D/4').alias('myAlias');
  $('myAlias').enable();
  
  // long pulses
  // simplified event notation x = {state:x}
  $('D/3').on('long-pulse'),function(){
      
      if (e.state == UI.const.state.longPulse) {
          // ard sends a pulse each second
          // allowing to select long pulse duration
          if (e.state.duration >= 3) {
              
              
          }
      }
  });

  // button-up
  $('D/3').on('button-up'),function(){
  });

  // button-down
  $('D/3').on('button-down'),function(){
  });
  
  // multiple events
  $('D/3').on('button-down button-up'),function(){
  });
  
  // pwm/voltage fade enable, sequence enable (multiple events ordered by index)
  $('D/3').enable('30s fade-in fade-out sequence-in sequence-out');

  // locks
  $('D/3,D/4,D/5').unbind().lock('myLock').enable().in('30s').disable(function(){ this.unlock }); */

})(ai.select.bind(ai)); 



/*  var x = 1;

  var _b = function(x1,x2,y1,y2) { return  Math.log(y1/y2)/(x1-x2); };
  var _a = function(b,x1,y1) { return y1/Math.exp(b*x1); }

  var b = _b(1,256,1,256);
  var a = _a(b,1,1);

  var _y = function(a,b,x) { return Math.round(a*Math.exp(b*x)); }


setInterval(function(){
  console.log('test');

  ai.select('D/3').pwm(_y(a,b,x));
  x++;
  x %= 256;

  console.log("--"+x + " --" + _y(a,b,x));

},10);*/









/*var ntc1Sensor = { type:'ntc',
                   b25:3988,
                   rI:22000,
                   vI:5,
                   r25:10000,scale: 1024 };

ai.select('A/0').on({state:'treshold'},function(e,bMsg){

   if (e.state == UDPInterface.const.state.initial) {
       console.log("initial");

       e.event.setMap('ntc1',ntc1Sensor)
              .setTreshold({
                  value   :   26,
                  loRange :   0,
                  hiRange :   4,
                  useMap  :   'ntc1'});
       
      return;
   }

   if (e.state == UDPInterface.const.state.loTreshold) {
      console.log("loTreshold, enabling in 5s",bMsg);

      ai.select('D/9').unbind();
      ai.select('A/0').unbind();

      ai.select('D/9').in('5s').enable('300s');
      ai.select('A/0').every('450s').refresh();

   }

   if (e.state == UDPInterface.const.state.hiTreshold) {
      console.log("hiTreshold, disable in 5s",bMsg);

      ai.select('D/9').unbind();
      ai.select('A/0').unbind();
      ai.select('D/9').in('5s').disable();
   }

   //console.log(e.event);

})*/



