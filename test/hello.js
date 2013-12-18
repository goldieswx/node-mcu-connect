
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



