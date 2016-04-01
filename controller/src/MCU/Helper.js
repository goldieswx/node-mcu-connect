


var _     = require('lodash');
var Q     = require('q');


var MCUHelper = function(obj) {

    this.linkedObject = obj;

};


MCUHelper.prototype.toRGB = function (rgbHexString,intensity){


   var io = this.linkedObject;

   if (io.children && io.children.length) {

        var rgbArray = rgbHexString.match(/[0-9A-Za-z]{2}/g);

        rgbArray = _.map(rgbArray,function(value) {
           return parseInt("0x" + value)/255;
        });

       intensity = intensity || 1.0;
       intensity = Math.min(intensity,1.0);
       intensity = Math.max(intensity,0.0);

        _.each(io.children,function(item) {
           if (_.indexOf(item.tags,'red')>-1) {
               item.pwm(Math.round(item.getDutyCycle()*rgbArray[0]*intensity));
           } else if (_.indexOf(item.tags,'green')>-1) {
               item.pwm(Math.round(item.getDutyCycle()*rgbArray[1]*intensity*.76));
           } else if (_.indexOf(item.tags,'blue')>-1) {
               item.pwm(Math.round(item.getDutyCycle()*rgbArray[2]*intensity*.75));
           }
        });
   }
};

module.exports = MCUHelper;
