/*
 * Module       : JSON (not compatible with the browser component!)
 * State        : preinstalled OVMS component
 * Use          : a) JSON.print(data);              -- output data as JSON, readable
 *                b) JSON.print(data, false);       -- …compact (without spacing/indentation)
 *                c) t = JSON.format(data);         -- format data as JSON string, readable
 *                d) t = JSON.format(data, false);  -- …compact (without spacing/indentation)
 */

// Re-export builtin native JSON methods:
exports.stringify = JSON.stringify;
exports.parse = JSON.parse;

exports.format = function(obj, ind)
  {
  var res = '';
  var type = (obj === null) ? "null" : typeof obj;
  if (type == "object" && Array.isArray(obj)) type = "array";
  var sp = (ind !== false);
  if (!ind) ind = '';

  switch (type)
    {
    case "string":
      res += '"' + obj.replace(/\"/g, '\\\"') + '"';
      break;
    case "array":
      res += sp ? '[\n' : '[';
      for (var i = 0; i < obj.length; i++)
        {
        if (sp) res += (ind + '  ');
        res += exports.format(obj[i], sp ? ind + '  ' : false);
        if (i != obj.length-1) res += ',';
        if (sp) res += '\n';
        }
      res += ind + ']';
      break;
    case "object":
      res += sp ? '{\n' : '{';
      var keys = Object.keys(obj);
      for (var i = 0; i < keys.length; i++)
        {
        if (sp) res += ind + '  "' + keys[i] + '": ';
        else res += '"' + keys[i] + '":';
        res += exports.format(obj[keys[i]], sp ? ind + '  ' : false);
        if (i != keys.length-1) res += ',';
        if (sp) res += '\n';
        }
      res += ind + '}';
      break;
    default:
      res += '' + obj;
  }

  if (sp && ind == '') res += '\n';
  return res;
  }

exports.print = function(obj, ind)
  {
  print(exports.format(obj, ind));
  }
