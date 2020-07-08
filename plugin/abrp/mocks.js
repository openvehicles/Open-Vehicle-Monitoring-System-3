exports.OvmsNotify = {
    Raise: function () {
        print(arguments);
    }
};

exports.OvmsConfig = {
    values: {},
    SetValues: function (key1, key2, value) {
        this.values[key1 + key2] = value;
    },
    GetValues: function (key1, key2) {
        return this.values[key1 + key2];
    }
};

responses = {};

exports.setMetricResponse = function (id, value) {
    responses[id] = value;
};

exports.OvmsMetrics = {
    Value: function (arg) {
        // print('mocks.setMetricResponse("' + arg + '", "xxxx");');
        return responses[arg];
    },
    AsFloat: function (arg) {
        // print('mocks.setMetricResponse("' + arg + '", 0000);');
        return parseFloat(responses[arg]);
    }
};

exports.HTTP = {
    Request: function (cfg) {
        if(cfg){
            if(cfg.done){
                cfg.done({})
            }
        }
    }
}