/*
 * Module       : JSON (not compatible with the browser component!)
 * State        : test/demo
 * Install as   : /store/scripts/lib/JSON.js
 * Load         : JSON = require("lib/JSON");
 * Use          : JSON.print(object);
 */

exports.print = function(obj, ind)
  {
  var type = typeof obj;
  if (type == "object" && Array.isArray(obj)) type = "array";
  if (!ind) ind = '';

  switch (type)
    {
    case "string":
      print('"' + obj.replace(/\"/g, '\\\"') + '"');
      break;
    case "array":
      print('[\n');
      for (var i = 0; i < obj.length; i++)
        {
        print(ind + '  ');
        exports.print(obj[i], ind + '  ');
        if (i != obj.length-1) print(',');
        print('\n');
        }
      print(ind + ']');
      break;
    case "object":
      print('{\n');
      var keys = Object.keys(obj);
      for (var i = 0; i < keys.length; i++)
        {
        print(ind + '  "' + keys[i] + '": ');
        exports.print(obj[keys[i]], ind + '  ');
        if (i != keys.length-1) print(',');
        print('\n');
        }
      print(ind + '}');
      break;
    default:
      print(obj);
  }

  if (ind == '') print('\n');
  }
