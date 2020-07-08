print = function (data) {
    console.log(data);
};

var mocks = require("./mocks");
var abrp = require("./sendlivedata2abrp");

OvmsNotify = mocks.OvmsNotify;
OvmsConfig = mocks.OvmsConfig;
OvmsMetrics = mocks.OvmsMetrics;
HTTP = mocks.HTTP;

mocks.setMetricResponse("v.p.latitude", 54.3234);
mocks.setMetricResponse("v.p.longitude", 3.2345);
mocks.setMetricResponse("v.b.soc", "10.5");
mocks.setMetricResponse("v.b.soh", "100");
mocks.setMetricResponse("v.p.altitude", "100");
mocks.setMetricResponse("v.b.power", "2");
mocks.setMetricResponse("v.p.speed", "30");
mocks.setMetricResponse("v.b.temp", "25");
mocks.setMetricResponse("v.e.temp", "16");
mocks.setMetricResponse("v.b.voltage", "356");
mocks.setMetricResponse("v.b.current", "1000");
mocks.setMetricResponse("v.c.state", "charging");
mocks.setMetricResponse("v.c.mode", "MODE");

abrp.resetConfig();
abrp.setConfigEntry("url", "URL!");
var r = OvmsMetrics.Value("v.p.altitude");
abrp.info();
abrp.onetime();
abrp.send(0);