
module.exports  = function(net,$,deferred) {

    console.log('bootstrapped');
    net.add('new-node',16);
    $('new-node').add('itf0',0x00);
    $('new-node').add('itf1',0x01);

    net.add('led-salon',17);
    $('led-salon').add('xitf0',0x00);
    $('led-salon').add('xitf1',0x01);

    // Entry interface
    (function(i) {
                i.add('led-1','pwm out 2.1').tag("out out1 red rgb").inverted();
                i.add('led-2','digital out 2.3').tag("out out1 white").inverted();
                i.add('led-3','pwm out 3.3').tag("out out1 green rgb").inverted();
                i.add('led-4','digital out 3.5').tag("out out1 white").inverted();
                i.add('led-5','pwm out 3.6').tag("out out1 blue rgb").inverted();
                i.add('led-6','digital out 2.4').tag("out out1").inverted();
                i.refresh();
     })($('itf0'));

    (function(i) {
                i.add('--led-1','pwm out 2.1').tag("out out3 red rgb").inverted();
                i.add('--led-2','digital out 2.3').tag("out out3 white").inverted();
                i.add('--led-3','pwm out 3.3').tag("out out3 green rgb").inverted();
                i.add('--led-4','digital out 3.5').tag("out out3 white").inverted();
                i.add('--led-5','pwm out 3.6').tag("out out3 blue rgb").inverted();
                i.add('--led-6','digital out 2.4').tag("out out3").inverted();
                i.refresh();
    })($('xitf0'));

    (function(i) {
        i.add('---led-1','pwm out 2.1').tag("out out4 red rgb").inverted();
        i.add('---led-2','digital out 2.3').tag("out out4 white").inverted();
        i.add('---led-3','pwm out 3.3').tag("out out4 green rgb").inverted();
        i.add('---led-4','digital out 3.5').tag("out out4 white").inverted();
        i.add('---led-5','pwm out 3.6').tag("out out4 blue rgb").inverted();
        i.add('---led-6','digital out 2.4').tag("out out4").inverted();

        i.refresh();
    })($('xitf1'));

    (function(i) {
        i.add('-led-1','pwm out 2.1').tag("out out2 red rgb").inverted();
        i.add('-led-2','digital out 2.3').tag("out out2 white").inverted();
        i.add('-led-3','pwm out 3.3').tag("out out2 green rgb").inverted();
        i.add('-led-4','digital out 3.5').tag("out out2 white").inverted();
        i.add('-led-5','pwm out 3.6').tag("out out2 blue rgb").inverted();
        i.add('-led-6','digital out 2.4').tag("out out2").inverted();
        i.refresh();
    })($('itf1'));


    net.add('inthall',30);
    $('inthall').add('ihallitf0',0x00);

    // Entry interface
    (function(i) {
        i.add('b1','digital in 1.2');
        i.add('b2','digital in 1.3');
        i.add('b3','digital in 2.5');
        i.add('b4','digital in 2.4');

        i.add('i1','digital out 2.3').tag('leds');
        i.add('led1','digital out 3.3').tag('leds');
        i.add('i3','digital out 3.4').tag('leds');
        i.add('zi14','digital out 2.1').tag('leds');
        i.refresh();
    })($('ihallitf0'));

    setTimeout(function() {
	deferred.resolve($);
    },2000);

};
