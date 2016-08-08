


var _     = require('lodash');
var Q     = require('q');


var MCUHelper = function(obj) {

    this.linkedObject = obj;

};


MCUHelper.prototype.HSLtoRGB=  function HSLtoRGB(hsl) {

        var h, s, l;
        h = hsl[0];
        s = hsl[1];
        l = hsl[2];
        var r, g, b;
        if(s == 0){
            r = g = b = l; // achromatic
        }else{
            var hue2rgb = function hue2rgb(p, q, t){
                if(t < 0) t += 1;
                if(t > 1) t -= 1;
                if(t < 1/6) return p + (q - p) * 6 * t;
                if(t < 1/2) return q;
                if(t < 2/3) return p + (q - p) * (2/3 - t) * 6;
                return p;
            }

            var q = l < 0.5 ? l * (1 + s) : l + s - l * s;
            var p = 2 * l - q;
            r = hue2rgb(p, q, h + 1/3);
            g = hue2rgb(p, q, h);
            b = hue2rgb(p, q, h - 1/3);
        }
        return [Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)];

}


MCUHelper.prototype.toHSL = function (hslValues,intensity){

    this.toRGB(this.HSLtoRGB(hslValues,intensity));

};

MCUHelper.prototype.toRGB = function (rgbValues,intensity){


   var io = this.linkedObject;

   if (io.children && io.children.length) {

        var rgbArray = rgbValues;
        if (typeof(rgbValues) === 'string') {
            rgbArray = rgbValues.match(/[0-9A-Za-z]{2}/g);

            rgbArray = _.map(rgbArray, function (value) {
                return parseInt("0x" + value) / 255;
            });
        }

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
