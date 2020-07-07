function Mock() {
    var name;
    var functions = {};
    var calls = [];
    var mockObj = {};

    getMock = function (name, methods) {
        name = name;
        methods.forEach(function (m) {
            functions[m] = {
                calls: 0,
                lastArgs: []
            };
        });
    };

    return {
        getMock
    };
}

exports.Mock = Mock;

// OvmsNotify.Raise
// OvmsConfig.SetValues