/* ovms.js | (c) Michael Balzer | https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3 */

const monthnames = ['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
const supportsTouch = 'ontouchstart' in window || navigator.msMaxTouchPoints;

if (window.loggedin == undefined)
  window.loggedin = false;


/**
 * Utilities
 */

function after(seconds, fn) {
  return window.setTimeout(fn, seconds*1000);
}

function now() {
  return Math.floor((new Date()).getTime() / 1000);
}

function encode_html(s) {
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/'/g, '&apos;')
    .replace(/"/g, '&quot;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;');
}

function unwrapLogLine(s) {
  return String(s)
    .replace(/(\S)\|+(.)/g, "$1\n……: $2");
}

function fix_minheight($el) {
  if ($el.css("resize") != "none") return;
  var mh = parseInt($el.css("max-height")), h = $el.outerHeight();
  $el.css("min-height", mh ? Math.min(h, mh) : h);
}

function getPathURL(path) {
  // TODO: use actual http.server config
  if (path.startsWith('/sd/'))
    return path.substr(3);
  else
    return '';
}


/**
 * AJAX Pages & Commands
 */

var page = {
  uri: null,
  path: null,
  search: null,
  params: {}
};

function setPage(uri) {
  page.uri = uri;
  var uriparts = uri.split("?");
  page.path = uriparts[0];
  page.search = uriparts[1];
  page.params = {};
  if (page.search) {
    page.search.split("&").map(function(kv) {
      var v = kv.split("=");
      page.params[decodeURIComponent(v[0])] = (v[1] != null) ? decodeURIComponent(v[1]) : true;
    });
  }
  if (page.params["nm"] == 1) $("body").addClass("night");
  else if (page.params["nm"] == 0) $("body").removeClass("night");
}

function updateLocation(reload) {
  page.search = '';
  for (v in page.params)
    page.search += "&" + encodeURIComponent(v) + "=" + encodeURIComponent(page.params[v]);
  page.search = page.search.slice(1);
  var hash = "#" + page.path + (page.search ? "?" + page.search : "");
  if (!reload) $("#main").data("uri", hash.substr(1));
  location.hash = hash;
}

function readLocation() {
  var uri = location.hash.substr(1);
  if (!uri.match("^/?[a-zA-Z0-9_]"))
    uri = "/home";
  return uri;
}

function loadPage(uri) {
  if (typeof uri != "string")
    uri = readLocation();
  if ($("#main").data("uri") != uri) {
    loaduri("#main", "get", uri, {});
  }
}

function reloadpage() {
  var uri = readLocation();
  loaduri("#main", "get", uri, {});
}

function reloadmenu() {
  $("#menu").load("/menu");
}

function login(dsturi) {
  if (!dsturi)
    dsturi = page.uri || "/home";
  loadPage("/login?uri=" + encodeURIComponent(dsturi));
}

function logout() {
  loadPage("/logout");
}

function xhrErrorInfo(request, textStatus, errorThrown) {
  var txt = "";
  if (request.status == 401 || request.status == 403)
    txt = "Unauthorized. <a class=\"btn btn-sm btn-default\" href=\"javascript:login()\">Login</a>";
  else if (request.status >= 400)
    txt = "Error " + request.status + " " + request.statusText;
  else if (textStatus)
    txt = "Request " + textStatus + ", please retry";
  else if (errorThrown)
    txt = errorThrown;
  return txt;
}

function setcontent(tgt, uri, text){
  if (!tgt || !tgt.length) return;

  tgt.find(".receiver").unsubscribe();
  tgt.chart("destroy");
  tgt.table("destroy");

  if (tgt[0].id == "main") {
    $("#nav .dropdown.open .dropdown-toggle").dropdown("toggle");
    $("#nav .navbar-collapse").collapse("hide");
    $("#nav li").removeClass("active");
    var mi = $("#nav [href='"+uri+"']");
    mi.parents("li").addClass("active");
    tgt[0].scrollIntoView();
    tgt.html(text);
    var $p = tgt.find(">.panel");
    if ($p.length == 1) $p.addClass("panel-single");
    var moduleid = $("title").data("moduleid") || "OVMS";
    if (mi.length > 0)
      document.title = moduleid + " " + (mi.attr("title") || mi.text());
    else
      document.title = moduleid + " Console";
  } else {
    tgt[0].scrollIntoView();
    tgt.html(text);
  }

  tgt.find(".get-window-resize").trigger('window-resize');
  tgt.find(".receiver").subscribe().trigger("msg:metrics", metrics);
  tgt.trigger("load");
}

function loaduri(target, method, uri, data){
  var tgt = $(target), cont;
  if (tgt.length != 1)
    return false;
  
  tgt.data("uri", uri);
  if (tgt[0].id == "main") {
    cont = $("html");
    location.hash = "#" + uri;
    setPage(uri);
  } else {
    cont = tgt.closest(".modal") || tgt.closest("form") || tgt;
  }
  
  $.ajax({ "type": method, "url": uri, "data": data,
    "timeout": 15000,
    "beforeSend": function(){
      cont.addClass("loading");
    },
    "complete": function(){
      cont.removeClass("loading");
    },
    "success": function(response){
      setcontent(tgt, uri, response);
    },
    "error": function(response, xhrerror, httperror){
      var text = response.responseText || httperror+"\n" || xhrerror+"\n";
      if (text.search("alert") == -1) {
        text = '<div id="alert" class="alert alert-danger alert-dismissable">'
          + '<a href="#" class="close" data-dismiss="alert" aria-label="close">&times;</a>'
          + '<strong>' + text + '</strong>'
          + '</div>';
      }
      setcontent(tgt, uri, text);
    },
  });

  return true;
}

$.fn.loaduri = function(method, uri, data) {
  return this.each(function() {
    loaduri(this, method, uri, data);
  });
};

function standardTextFilter(msg) {
  if (msg.error)
    return '<div class="bg-danger">'+msg.error+'</div>';
  else
    return $('<div/>').text(msg.text).html();
}

function loadjs(command, target, filter, timeout) {
  return loadcmd({ "command": command, type: "js" }, target, filter, timeout);
}

function loadcmd(command, target, filter, timeout) {
  var $output, outmode = "", args = {};

  if (!command) return null;
  if (typeof command == "object") {
    args = command;
  } else {
    args.command = command;
  }

  if (typeof filter == "number") {
    timeout = filter; filter = null;
  }
  if (typeof target == "function") {
    filter = target; target = null;
  }

  if (target == null) {
    $output = $(null);
  }
  else if (typeof target == "object") {
    $output = target;
  }
  else if (target.startsWith("+")) {
    outmode = "+";
    $output = $(target.substr(1));
  } else {
    $output = $(target);
  }

  if (!filter)
    filter = standardTextFilter;

  if (!timeout) {
    if (args["type"] != "js" && /^(test |ota |copen .* scan)/.test(args.command))
      timeout = 300;
    else
      timeout = 20;
  }

  var lastlen = 0, xhr, timeouthd;
  var checkabort = function() {
    if (xhr.readyState != 4)
      xhr.abort("timeout");
  };
  var add_output = function(addhtml) {
    if (addhtml == null || !$output.length) {
      outmode = "+";
      return;
    }
    if (outmode == "") { $output.empty(); outmode = "+"; }
    var autoscroll = ($output.get(0).scrollTop + $output.innerHeight()) >= $output.get(0).scrollHeight;
    $output.append(addhtml);
    $output.closest('.get-window-resize').trigger('window-resize');
    if (autoscroll) $output.scrollTop($output.get(0).scrollHeight);
  };

  xhr = $.ajax({ "type": "post", "url": "/api/execute", "data": args,
    "timeout": 0,
    "beforeSend": function() {
      if ($output.length) {
        $output.addClass("loading");
        fix_minheight($output);
      }
      timeouthd = window.setTimeout(checkabort, timeout*1000);
    },
    "complete": function() {
      window.clearTimeout(timeouthd);
      if ($output.length) {
        $output.removeClass("loading");
        fix_minheight($output);
      }
    },
    "xhrFields": {
      onprogress: function(ev) {
        var request = ev.currentTarget;
        if (request.status != 200) return;
        var addtext = request.response.substring(lastlen);
        lastlen = request.response.length;
        add_output(filter({ "request": request, "text": addtext }));
        window.clearTimeout(timeouthd);
        timeouthd = window.setTimeout(checkabort, timeout*1000);
      },
    },
    "success": function(response, textStatus, request) {
      var addtext = response.substring(lastlen);
      add_output(filter({ "request": request, "text": addtext }));
    },
    "error": function(request, textStatus, errorThrown) {
      var cmdinfo = (args.command.length > 200) ? args.command.substr(0,200)+"[…]" : args.command;
      console.log("loadcmd '" + cmdinfo + "' ERROR: status=" + textStatus + ", httperror=" + errorThrown);
      var txt = xhrErrorInfo(request, textStatus, errorThrown);
      add_output(filter({ "request": request, "error": txt }));
    },
  });

  return xhr;
}

$.fn.loadcmd = function(command, filter, timeout) {
  return this.each(function() {
    loadcmd(command, $(this), filter, timeout);
  });
};


/**
 * WebSocket Connection
 */

var monitorTimer, last_monotonic = 0;
var ws, ws_inhibit = 0;
var metrics = {};
var shellhist = [""], shellhpos = 0;
var loghist = [];
const loghist_maxsize = 100;

function initSocketConnection(){
  ws = new WebSocket('ws://' + location.host + '/msg');
  ws.onopen = function(ev) {
    console.log("WebSocket OPENED", ev);
    $(".receiver").subscribe();
  };
  ws.onerror = function(ev) { console.log("WebSocket ERROR", ev); };
  ws.onclose = function(ev) { console.log("WebSocket CLOSED", ev); };
  ws.onmessage = function(ev) {
    var msg;
    try {
      msg = JSON.parse(ev.data);
    } catch (e) {
      console.error("WebSocket msg: " + e + ": " + ev.data);
      return;
    }
    for (msgtype in msg) {
      if (msgtype == "event") {
        $(".receiver").trigger("msg:event", msg.event);
        $(".monitor[data-events]").each(function(){
          var cmd = $(this).data("updcmd");
          var js = $(this).data("updjs");
          var evf = $(this).data("events");
          if ((cmd || js) && evf && msg.event.match(evf)) {
            $(this).data("updlast", now());
            loadcmd({ command: js ? js : cmd, type: js ? "js" : "cmd" }, $(this));
          }
        });
      }
      else if (msgtype == "metrics") {
        $.extend(metrics, msg.metrics);
        $(".receiver").trigger("msg:metrics", msg.metrics);
      }
      else if (msgtype == "notify") {
        processNotification(msg.notify);
        $(".receiver").trigger("msg:notify", msg.notify);
      }
      else if (msgtype == "log") {
        loghist.push(msg.log);
        if (loghist.length > loghist_maxsize) loghist.shift();
        $(".receiver").trigger("msg:log", msg.log);
      }
    }
  };
}

function monitorInit(force){
  $(".monitor").each(function(){
    var cmd = $(this).data("updcmd");
    var js = $(this).data("updjs");
    var txt = $(this).text();
    if ((cmd || js) && (force || !txt)) {
      $(this).data("updlast", now());
      loadcmd({ command: js ? js : cmd, type: js ? "js" : "cmd" }, $(this));
    }
  });
}

function monitorUpdate(){
  if (!ws || ws.readyState == ws.CLOSED){
    if (ws_inhibit != 0)
      --ws_inhibit;
    if (ws_inhibit == 0)
      initSocketConnection();
  }
  var new_monotonic = parseInt(metrics["m.monotonic"]) || 0;
  if (new_monotonic < last_monotonic)
    location.reload();
  else
    last_monotonic = new_monotonic;
  $(".monitor").each(function(){
    var cnt = $(this).data("updcnt");
    var int = $(this).data("updint");
    var last = $(this).data("updlast");
    var cmd = $(this).data("updcmd");
    var js = $(this).data("updjs");
    if (!cnt || (!cmd && !js) || (now()-last) < int)
      return;
    $(this).data("updcnt", cnt-1);
    $(this).data("updlast", now());
    loadcmd({ command: js ? js : cmd, type: js ? "js" : "cmd" }, $(this));
  });
}


function processNotification(msg) {
  var opts = { timeout: 0 };
  if (msg.type == "info") {
    opts.title = '<span class="lead text-info"><i>ⓘ</i> ' + msg.subtype + ' Info</span>';
    opts.timeout = 60;
  }
  else if (msg.type == "alert") {
    opts.title = '<span class="lead text-danger"><i>⚠</i> ' + msg.subtype + ' Alert</span>';
  }
  else if (msg.type == "error") {
    opts.title = '<span class="lead text-warning"><i>⛍</i> ' + msg.subtype + ' Error</span>';
  }
  else
    return;
  opts.body = '<pre>' + msg.value + '</pre>';
  confirmdialog(opts.title, opts.body, ["OK"], opts.timeout);
}


$.fn.subscribe = function(topics) {
  return this.each(function() {
    var subscriptions = $(this).data("subscriptions");
    if (!topics) {
      // init from data attr:
      topics = subscriptions;
      subscriptions = "";
    }
    var subs = subscriptions ? subscriptions.split(' ') : [];
    var tops = topics ? topics.split(' ') : [];
    for (var i = 0; i < tops.length; i++) {
      if (tops[i] && !subs.includes(tops[i])) {
        try {
          console.log("subscribe " + tops[i]);
          if (ws) ws.send("subscribe " + tops[i]);
          subs.push(tops[i]);
        } catch (e) {
          console.log(e);
        }
      }
    }
    $(this).data("subscriptions", subs.join(' '));
  });
};

$.fn.unsubscribe = function(topics) {
  return this.each(function() {
    var subscriptions = $(this).data("subscriptions");
    if (!topics) {
      // cleanup:
      topics = subscriptions;
    }
    var subs = subscriptions ? subscriptions.split(' ') : [];
    var tops = topics ? topics.split(' ') : [];
    var i, j;
    for (i = 0; i < tops.length; i++) {
      if (tops[i] && (j = subs.indexOf(tops[i])) >= 0) {
        try {
          console.log("unsubscribe " + tops[i]);
          if (ws) ws.send("unsubscribe " + tops[i]);
          subs.splice(j, 1);
        } catch (e) {
          console.log(e);
        }
      }
    }
    $(this).data("subscriptions", subs.join(' '));
  });
};


$.fn.reconnectTicker = function(msg) {
  $("html").addClass("loading disabled");
  ws_inhibit = 10;
  if (ws) ws.close();
  window.setInterval(function(){
    if (ws && ws.readyState == ws.OPEN)
      location.reload();
    else
      $("#reconnectTickerDots").append("•");
  }, 1000);
  return this.append(
      (msg ? msg : '<p class="lead">Rebooting now…</p>') +
      '<p>The window will automatically reload when the browser reconnects to the module.</p>' +
      '<p id="reconnectTickerDots">•</p>');
};


/**
 * UI Widgets
 */

// Plugin Maker
// credits: https://www.bitovi.com/blog/writing-the-perfect-jquery-plugin
$.pluginMaker = function(plugin) {
  $.fn[plugin.prototype.cname] = function(options) {
    var args = $.makeArray(arguments), after = args.slice(1);
    var ismethod = (typeof options == "string");
    var result;
    this.each(function() {
      // see if we have an instance
      var instance = $.data(this, plugin.prototype.cname);
      if (instance) {
        if (ismethod) {
          // call a method on the instance
          if ($.isFunction(instance[options]))
            result = instance[options].apply(instance, after);
          else
            throw "UndefinedMethod: " + plugin.prototype.cname + "." + options;
        } else if (instance.update) {
          // call update on the instance
          instance.update.apply(instance, args);
        }
      } else {
        // create the plugin
        new plugin(this, options);
      }
    });
    return (ismethod && result != undefined) ? result : this;
  };
};

// OVMS namespace:
var ovms = {uid:0};

// Widget root class:
ovms.Widget = function(el, options) {
  if (el) this.init(el, options);
}
$.extend(ovms.Widget.prototype, {
  cname: "widget",
  options: {},
  init: function(el, options) {
    this.uid = ++ovms.uid;
    this.$el = $(el);
    this.$el.data(this.cname, this);
    this.$el.addClass(this.cname);
    var dataoptions = this.$el.data("options");
    if (dataoptions != null && typeof dataoptions != "object") {
      console.error("Invalid JSON syntax: " + this.$el.data("options"));
      dataoptions = null;
    }
    this.options = $.extend({}, this.options, dataoptions, options);
  },
  update: function(options) {
    $.extend(this.options, options);
  },
});

// Dialog:
ovms.Dialog = function(el, options) {
  if (el) this.init(el, options);
}
$.extend(ovms.Dialog.prototype, ovms.Widget.prototype, {
  cname: "dialog",

  options: {
    title: '',
    body: '',
    show: false,
    remote: false,
    backdrop: true,
    keyboard: true,
    transition: 'fade',
    size: '',
    contentClass: '',
    onShow: null,
    onHide: null,
    onShown: null,
    onHidden: null,
    onUpdate: null,
    buttons: [{}],
    timeout: 0,
    input: null,
  },

  init: function(el, options) {
    if ($(el).parent().length == 0) {
      options = $.extend(options, { show: true, isDynamic: true });
    }
    ovms.Widget.prototype.init.call(this, el, options);
    this.input = options.input ? options.input : {};
    this.data = { showing: false };
    this.$buttons = [];
    // convert element to modal if not predefined by user:
    if (this.$el.children().length == 0) {
      this.$el.html('<div class="modal-dialog"><div class="modal-content"><div class="modal-header"><button type="button" class="close" data-dismiss="modal" aria-label="Close"><span aria-hidden="true">&times;</span></button><h4 class="modal-title"></h4></div><div class="modal-body"></div><div class="modal-footer"></div></div></div></div>');
      this.$el.addClass("modal");
    }
    this.$el.on('show.bs.modal', $.proxy(this.onShow, this));
    this.$el.on('hide.bs.modal', $.proxy(this.onHide, this));
    this.$el.on('shown.bs.modal', $.proxy(this.onShown, this));
    this.$el.on('hidden.bs.modal', $.proxy(this.onHidden, this));
    if (this.options.isDynamic) this.$el.appendTo('body');
    this.update(this.options);
  },

  update: function(options) {
    $.extend(this.options, options);
    // configure modal:
    this.$el.removeClass('fade').addClass(this.options.transition);
    this.$el.find('.modal-dialog').attr("class", "modal-dialog " + (this.options.size ? ("modal-" + this.options.size) : ""));
    this.$el.find('.modal-content').attr("class", "modal-content " + (this.options.contentClass || ""));
    if (options.title != null)
      this.$el.find('.modal-title').html(options.title);
    if (options.body != null)
      this.$el.find('.modal-body').html(options.body);
    if (options.buttons != null) {
      var footer = this.$el.find('.modal-footer');
      footer.empty();
      this.$buttons = [];
      for (var i = 0; i < this.options.buttons.length; i++) {
        var btn = $.extend({ label: 'Close', btnClass: 'default', autoHide: true, action: null }, this.options.buttons[i], { index: i });
        this.options.buttons[i] = btn;
        this.$buttons[i] = $('<button type="button" class="btn btn-'+btn.btnClass+'" value="'+btn.index+'">'+btn.label+'</button>')
          .appendTo(footer).on('click', $.proxy(this.onClick, this, btn));
      }
    }
    if (this.options.onUpdate)
      this.options.onUpdate.call(this.$el, this.input);
    this.$el.modal(this.options);
  },

  show: function(options) {
    if (options) this.update(options);
    this.$el.modal('show');
  },
  hide: function() {
    this.$el.modal('hide');
  },

  onShow: function() {
    if (this.options.onShow)
      this.options.onShow.call(this.$el, this.input);
  },

  onHide: function() {
    if (this.options.onHide)
      this.options.onHide.call(this.$el, this.input);
  },

  onShown: function() {
    this.data.showing = true;
    this.input.button = null;
    if (!supportsTouch) this.$el.find('.form-control, .btn').first().focus();
    if (this.options.onShown)
      this.options.onShown.call(this.$el, this.input);
    if (this.options.timeout)
      after(this.options.timeout, $.proxy(this.hide, this));
  },

  onHidden: function() {
    this.data.showing = false;
    if (this.options.isDynamic)
      this.$el.detach();
    if (this.options.onHidden)
      this.options.onHidden.call(this.$el, this.input);
    if (this.input.button != null && this.input.button.action) {
      this.input.button.action.call(this.$el, this.input);
    }
  },

  isShowing: function() {
    return this.data.showing;
  },

  triggerButton: function(button) {
    var btn;
    if (typeof button == "number") {
      btn = this.$buttons[button];
    }
    else if (typeof button == "string") {
      for (var i = 0; i < this.options.buttons.length; i++) {
        if (this.options.buttons[i].label == button) {
          btn = this.$buttons[i];
          break;
        }
      }
    }
    if (btn && !btn.prop("disabled")) {
      btn.trigger('click');
      return true;
    }
    return false;
  },

  onClick: function(button) {
    this.input.button = button;
    if (button.autoHide)
      this.hide();
    else if (button.action) {
      button.action.call(this.$el, this.input);
      this.input.button = null;
    }
  },
});
$.pluginMaker(ovms.Dialog);


// Dialog utility wrappers:

$.fn.confirmdialog = function(_title, _body, _buttons, _action, _timeout) {
  if (typeof _action == "number") { _timeout = _action; _action = null; }
  var options = { show: true, title: _title, body: _body, buttons: [], timeout: _timeout };
  if (_buttons) {
    for (var i = 0; i < _buttons.length; i++) {
      options.buttons.push({ label: _buttons[i],
        btnClass: (_buttons.length <= 2 && i==_buttons.length-1) ? "primary" : "default" });
    }
  }
  if (_action) {
    options.onHidden = function(input) { _action(input.button ? input.button.index : null); };
  }
  return this.dialog(options);
};
var confirmdialog = function() { return $.fn.confirmdialog.apply($('<div />'), arguments); };

$.fn.promptdialog = function(_type, _title, _prompt, _buttons, _action) {
  var options = { show: true, title: _title, buttons: [] };
  var uid = ++ovms.uid;
  options.body = '<label for="prompt'+uid+'">'+_prompt+'</label><input id="prompt'+uid+'" type="'+_type+'" class="form-control">';
  if (_buttons) {
    for (var i = 0; i < _buttons.length; i++) {
      options.buttons.push({ label: _buttons[i],
        btnClass: (_buttons.length <= 2 && i==_buttons.length-1) ? "primary" : "default" });
    }
  }
  options.onUpdate = function(input) {
    var footer = $(this).find('.modal-footer');
    $(this).find('input').val(input.text).on('keydown', function(ev) {
      if (ev.which == 13) footer.find('.btn-primary').trigger('click');
    });
  };
  options.onShown = function(input) {
    $(this).find('input').first().focus();
  }
  options.onHidden = function(input) {
    if (input.button) input.text = $(this).find('input').val();
    if (_action) _action(input.button ? input.button.index : null, input.text);
  };
  return this.dialog(options);
};
var promptdialog = function() { return $.fn.promptdialog.apply($('<div />'), arguments); };


// FileBrowser:
ovms.FileBrowser = function(el, options) {
  if (el) this.init(el, options);
}
$.extend(ovms.FileBrowser.prototype, ovms.Widget.prototype, {
  cname: "filebrowser",

  options: {
    path: '',
    quicknav: ['/sd/', '/store/'],
    filter: null,
    sortBy: null,
    sortDir: 1,
    onUpdate: null,
    onPathChange: null,
    onAction: null,
    input: null,
  },

  init: function(el, options) {
    ovms.Widget.prototype.init.call(this, el, options);

    this.input = options.input ? options.input : {};
    $.extend(this.input, {
      path: '',
      dir: '',
      file: '',
      noload: false,
    });
    this.data = {
      lastpath: '',
      lastdir: '',
      listdir: '',
      listsel: '',
    };
    this.xhr = null;

    if (this.$el.children().length == 0) {
      this.$el.html('<div class="fb-pathbox form-group"><label class="control-label" for="input-path-${uid}">Path (trailing slash = dir):</label><div class="input-group"><input type="text" class="form-control font-monospace" name="path" id="input-path-${uid}" value=""><div class="input-group-btn"><button type="button" class="btn btn-default fb-path-stop" disabled title="Stop">&times;</button><button type="button" class="btn btn-default fb-path-reload" title="Reload">⟲</button><button type="button" class="btn btn-default fb-path-up" title="Up">↰</button></div></div></div><div class="fb-quicknav form-group"/><div class="fb-files"><table class="table table-condensed table-hover table-scrollable font-monospace get-window-resize"><thead><tr><th class="col-xs-3 col-sm-2" data-key="size">Size</th><th class="hidden-xs col-sm-4" data-key="date">Date</th><th class="col-xs-9 col-sm-6" data-key="name">Name</th></tr></thead><tbody/></table></div>'.replace(/\$\{uid\}/g, this.uid));
    }

    this.$pathinput = this.$el.find("input[name=path]");
    this.$pathinput.on('change', $.proxy(this.setPath, this)).on('keydown', $.proxy(function(ev) {
      if (ev.which == 13) {
        var lastdir = this.input.dir;
        this.setPath();
        if (this.input.file || this.input.dir == lastdir)
          this.onAction();
        ev.preventDefault();
      }
      else if (ev.which == 27) {
        this.stopLoad();
      }
    }, this));
    this.$el.find('.fb-path-up').on('click', $.proxy(function(ev) {
      var parent = this.input.dir.replace(/\/[^/]+$/, '');
      if (parent) this.setPath(parent+'/');
      else this.setPath(this.input.dir+'/');
    }, this));
    this.$el.find('.fb-path-reload').on('click', $.proxy(function(ev) {
      this.setPath(this.input.path, true);
    }, this));
    this.$btnstop = this.$el.find('.fb-path-stop');
    this.$btnstop.on('click', $.proxy(function(ev) { this.stopLoad(); }, this));

    this.$quicknav = this.$el.find(".fb-quicknav");

    this.$filetable = this.$el.find(".fb-files>table");
    this.$filecols = this.$filetable.find("thead th");
    this.$filecols.on('click', $.proxy(function(ev) {
      var by = $(ev.currentTarget).data("key");
      var dir = $(ev.currentTarget).hasClass('sort-down') ? -1 : 1;
      if (by == this.options.sortBy) dir = -dir;
      this.sortList(by, dir);
      if (!supportsTouch) this.$pathinput.focus();
    }, this));

    this.$filebody = this.$filetable.find("tbody");
    this.$filebody.on('click', 'tr', $.proxy(function(ev) {
      this.data.listsel = $(ev.currentTarget).data("name");
      this.setPath(this.data.listdir + "/" + this.data.listsel);
      ev.preventDefault();
    }, this)).on('dblclick', 'tr', $.proxy(this.onAction, this));

    this.update(this.options);
  },

  update: function(options) {
    $.extend(this.options, options);

    this.$quicknav.empty();
    for (var i = 0; i < this.options.quicknav.length; i++) {
      this.$quicknav.append('<button type="button" class="btn btn-sm btn-default"><code>' + this.options.quicknav[i] + '</code></button>');
    }
    this.$quicknav.find("button").on('click', $.proxy(function(ev) {
      this.setPath($(ev.delegateTarget).text());
    }, this));

    if (this.options.onUpdate)
      this.options.onUpdate.call(this.$el, this.input);

    this.sortList(this.options.sortBy, this.options.sortDir);
    if (options.path !== undefined) {
      this.setPath(options.path);
    }
    else if (options.filter !== undefined) {
      this.loadDir();
    }
  },

  getInput: function() {
    return this.input;
  },

  setPath: function(newpath, reload) {
    if (typeof newpath == "string") {
      this.input.path = newpath;
      this.$pathinput.val(newpath);
    } else {
      this.input.path = this.$pathinput.val();
    }
    this.input.dir = this.input.path.replace(/\/[^/]*$/, '');
    this.input.file = this.input.path.substr(this.input.dir.length+1);

    var fn = "";
    if (this.input.dir == this.data.listdir)
      fn = this.input.path.substr(this.data.listdir.length+1);
    this.$filebody.children().each(function() {
      if ($(this).data("name") === fn) $(this).addClass("active");
      else $(this).removeClass("active");
    });

    if (reload)
      this.data.lastdir = '';

    if (this.input.path != this.data.lastpath) {
      this.input.noload = false;
      if (this.options.onPathChange)
        this.options.onPathChange.call(this.$el, this.input);
      this.data.lastpath = this.input.path;
    }

    if (this.input.dir != this.data.lastdir) {
      if (this.input.noload) {
        this.stopLoad();
        this.data.lastdir = '';
        this.$filebody.empty();
        this.data.listdir = '';
        this.data.listsel = '';
      } else {
        this.loadDir();
        this.data.lastdir = this.input.dir;
      }
    }

    if (!supportsTouch) this.$pathinput.focus();
  },

  loadDir: function() {
    var self = this;
    var fbuf = "";

    this.stopLoad();
    this.$filebody.empty();
    this.data.listdir = this.input.dir;
    this.data.listsel = '';
    if (!this.data.listdir)
      return;

    this.$btnstop.prop('disabled', false);
    this.xhr = loadcmd("vfs ls '" + this.input.dir + "'", this.$filebody, function(msg) {
      if (msg.error) {
        if (msg.error.startsWith("Request abort"))
          return '<div class="bg-info">Stopped.</div>';
        else
          return '<div class="bg-danger">'+msg.error+'</div>';
      }

      fbuf += msg.text;
      var lines = fbuf.split("\n");
      if (!lines || lines.length < 2) return "";
      fbuf = lines[lines.length-1];
      var res = '', f = {};
      for (var i = 0; i < lines.length-1; i++) {
        if (!lines[i]) continue;
        if (lines[i].startsWith("Error"))
          return '<div class="bg-danger">' + lines[i] + '</div>';
        f.size = lines[i].substring(0, 10).trim();
        f.date = lines[i].substring(10, 29).trim();
        f.name = lines[i].substring(29).trim();
        if (!f.name) {
          console.log("FileBrowser.loadDir: can't parse line: '" + lines[i] + "'");
        } else {
          f.path = self.data.listdir + '/' + f.name;
          f.isdir = (f.size == '[DIR]');
          f.bytes = self.sizeToBytes(f.size);
          f.isodate = self.dateToISO(f.date);
          f.class = "";
          if (self.options.filter && (
            (typeof self.options.filter == "string" && !f.isdir && !f.name.match(self.options.filter)) ||
            (typeof self.options.filter == "function" && !self.options.filter(f)))) {
            continue;
          }
          if (f.name == self.input.file) {
            f.class += " active";
            self.data.listsel = f.name;
          } else {
            f.class = "";
          }
          res += '<tr data-name="' + encode_html(f.name) + '" data-size="' + f.bytes +
            '" data-date="' + f.isodate + '" class="' + f.class + '">' +
            '<td class="col-xs-3 col-sm-2">' + f.size +
            '</td><td class="hidden-xs col-sm-4">' + f.date +
            '</td><td class="col-xs-9 col-sm-6">' + encode_html(f.name) + '</td></tr>';
        }
      }

      if (!self.options.sortBy)
        return res;
      if (res) {
        var scrollpos = self.$filebody.get(0).scrollTop;
        self.$filebody.detach().append(res);
        self.sortList();
        self.$filebody.appendTo(self.$filetable).scrollTop(scrollpos);
        self.$filetable.trigger('window-resize');
      }
      return null;
    }).always($.proxy(function() {
      this.$btnstop.prop('disabled', true);
    }, this));
  },

  sizeToBytes: function(size) {
    if (size == "[DIR]") return -1;
    var unit = size[size.length-1], val = parseFloat(size);
    if (unit == 'k') return val*1024;
    else if (unit == 'M') return val*1048576;
    else if (unit == 'G') return val*1073741824;
    else return val;
  },

  dateToISO: function(date) {
    var month = monthnames.indexOf(date.substr(3,3)) + 1;
    return date.substr(7,4)+'-'+(month<10?'0':'')+month+'-'+date.substr(0,2)+' '+date.substr(12);
  },

  stopLoad: function() {
    this.$btnstop.prop('disabled', true);
    if (this.xhr)
      this.xhr.abort();
    if (!supportsTouch) this.$pathinput.focus();
  },

  sortList: function(by, dir) {
    if (by == null) {
      by = this.options.sortBy;
      dir = this.options.sortDir;
    } else {
      dir = dir || 1;
      this.options.sortBy = by;
      this.options.sortDir = dir;
      this.$filecols.removeClass('sort-up sort-down')
        .filter('[data-key="'+by+'"]').addClass((dir<0) ? 'sort-down' : 'sort-up');
    }
    if (!by) return;
    var rows = $.makeArray(this.$filebody.children());
    rows.sort(function(a,b){
      if (!a.dataset[by]) return 1; if (!b.dataset[by]) return -1;
      var pri = a.dataset[by].localeCompare(b.dataset[by]);
      var sec = (by!="name") ? a.dataset["name"].localeCompare(b.dataset["name"]) : 0;
      return dir * (pri ? pri : sec);
    });
    this.$filebody.html(rows);
  },

  newDir: function() {
    this.stopLoad();
    var path = this.input.dir + "/";
    var self = this;
    promptdialog("text", "Create new directory", path + "… (empty = create this dir)", ["Cancel", "Create"], function(create, dirname) {
      if (create) {
        path = (path + dirname).replace(/\/+$/, "");
        $.post("/api/execute", { "command": "vfs mkdir '" + path + "'" }, function(result) {
          if (result.startsWith("Error"))
            confirmdialog("Error", result, ["OK"]);
          else
            self.setPath(path + "/", path == self.input.dir);
        }).fail(function(request, textStatus, errorThrown){
          confirmdialog("Error", xhrErrorInfo(request, textStatus, errorThrown), ["OK"]);
        });
      }
    });
  },

  onAction: function() {
    if (this.options.onAction)
      this.options.onAction.call(this.$el, this.input);
  },

});
$.pluginMaker(ovms.FileBrowser);


// FileDialog:
ovms.FileDialog = function(el, options) {
  if (el) this.init(el, options);
}
$.extend(ovms.FileDialog.prototype, ovms.Widget.prototype, {
  cname: "filedialog",

  options: {
    title: 'Select file',
    submit: 'Select',
    onSubmit: null,
    onCancel: null,
    path: '',
    quicknav: ['/sd/', '/store/'],
    filter: null,
    sortBy: null,
    sortDir: 1,
    select: 'f',
    showNewDir: true,
    backdrop: true,
    keyboard: true,
    transition: 'fade',
    size: 'lg',
    onUpdate: null,
    show: false,
  },

  init: function(el, options) {
    ovms.Widget.prototype.init.call(this, el, options);
    this.input = {};
    this.$fb = null;
    this.$el.dialog({
      input: this.input,
      body: '<div class="filebrowser"/>',
      onHide: $.proxy(this.onHide, this),
      onHidden: $.proxy(this.onHidden, this),
    });
    this.$fb = this.$el.find('.filebrowser').filebrowser({
      input: this.input,
      onPathChange: $.proxy(this.onPathChange, this),
      onAction: $.proxy(this.onAction, this),
    });
    this.update(this.options);
  },

  update: function(options) {
    $.extend(this.options, options);
    var newbtns = [];
    if (this.options.showNewDir) newbtns.push(
      { label: "New dir", btnClass: "default pull-left", autoHide: false, action: $.proxy(this.newDir, this) });
    newbtns.push(
      { label: "Cancel" },
      { label: this.options.submit, btnClass: "primary" });
    this.$el.dialog({
      title: this.options.title,
      buttons: newbtns,
      backdrop: this.options.backdrop,
      keyboard: this.options.keyboard,
      transition: this.options.transition,
      size: this.options.size,
    });
    this.$btnsubmit = this.$el.find(".modal-footer .btn-primary");
    this.$fb.filebrowser({
      path: (options.path != null) ? options.path : this.input.path,
      quicknav: this.options.quicknav,
      filter: this.options.filter,
      sortBy: this.options.sortBy,
      sortDir: this.options.sortDir,
    });
    this.onPathChange();
    if (this.options.onUpdate)
      this.options.onUpdate.call(this.$el, this.input);
    if (options.show != null) {
      var showing = this.$el.dialog('isShowing');
      if (options.show === true && !showing)
        this.$el.dialog('show');
      else if (options.show === false && showing)
        this.$el.dialog('hide');
    }
  },

  show: function(options) {
    if (options) this.update(options);
    this.$el.dialog('show');
  },
  hide: function() {
    this.$el.dialog('hide');
  },

  setPath: function(newpath, reload) {
    this.$fb.filebrowser('setPath', newpath, reload);
  },
  newDir: function() {
    this.$fb.filebrowser('newDir');
  },

  onPathChange: function() {
    var ok = (
      (this.options.select == 'f' && this.input.file) ||
      (this.options.select == 'd' && !this.input.file));
    this.$btnsubmit.prop("disabled", !ok);
  },
  onAction: function() {
    this.$el.dialog('triggerButton', this.options.submit);
  },
  onHide: function() {
    this.$fb.filebrowser('stopLoad');
  },
  onHidden: function(input) {
    if (input.button && input.button.label == this.options.submit) {
      if (this.options.onSubmit)
        this.options.onSubmit.call(this.$el, this.input);
    }
    else {
      if (this.options.onCancel)
        this.options.onCancel.call(this.$el, this.input);
    }
  },
});
$.pluginMaker(ovms.FileDialog);


// Template list editor:

$.fn.listEditor = function(op, data){
  return this.each(function(){
    if (op) {
      $(this).trigger('list:'+op, data);
    } else {
      // init:
      var $this = $(this);
      var el_itemid = $this.find('.list-item-id');
      var el_body = $this.find('.list-items');
      var el_template = $this.find('template').html();

      $this.on('list:addItem', function(evt, data) {
        var id = Number(el_itemid.val()) + 1;
        el_itemid.val(id);
        data = $.extend({ MODE: "upd" }, data);
        var txt = el_template.replace(/ITEM_ID/g, id).replace(/ITEM_MODE/g, data.MODE);
        for (key in data)
          txt = txt.replace(new RegExp('ITEM_'+key,'g'), encode_html(data[key]));
        txt = txt.replace(/ITEM_\w+/g, '');
        var $el = $(txt).appendTo(el_body);

        $el.find("option[data-value]").each(function(){
          $(this).prop("selected", $(this).val() == $(this).data("value"));
        });
        $el.find("input[data-value]").each(function(){
          var sel = $(this).val() == $(this).data("value");
          $(this).prop("checked", sel);
          if (sel) $(this).parent().addClass("active");
          else $(this).parent().removeClass("active");
        });

        var discl = (data.MODE == "add") ? "add-disabled" : "list-disabled";
        $el.find("select."+discl).each(function(){
          $(this).prop("disabled", true).append($('<input type="hidden">')
            .attr("name", $(this).attr("name")).attr("value", $(this).val()));
        });
        $el.find("input."+discl).prop("readonly", true);
        $el.find("button."+discl).prop("disabled", true);

        $this.trigger('list:validate');
      });

      $this.on('change keyup', 'input,select,textarea', function(ev) {
        $this.trigger('list:validate');
      });

      $this.on('click', '.list-item-add', function(evt) {
        var data = $.extend({ MODE: "add" }, $(this).data('preset'));
        $(this).trigger('list:addItem', data);
      });

      $this.on('click', '.list-item-del', function(evt) {
        $(this).closest('.list-item').remove();
        $this.trigger('list:validate');
      });

    }
  });
};


/**
 * Slider widget plugin
 */

$.fn.slider = function(options) {
  return this.each(function() {
    var $sld = $(this).closest('.slider'), data = $.extend({ checked: true }, $sld.data());
    var opts = (typeof options == "object") ? options : data;
    // init?
    if ($sld.children().length == 0) {
      var id = $sld.attr('id');
      $sld.html('\
        <div class="slider-control form-inline">\
          <input class="slider-enable" type="checkbox" checked>\
          <input class="form-control slider-value" type="number" id="input-ID" name="ID">\
          <span class="slider-unit">UNIT</span>\
          <input class="btn btn-default slider-down" type="button" value="➖">\
          <input class="btn btn-default slider-set" type="button" value="◈">\
          <input class="btn btn-default slider-up" type="button" value="➕">\
        </div>\
        <input class="slider-input" type="range">'
        .replace(/ID/g, id).replace(/UNIT/g, opts.unit||''));
    }
    // update:
    var $inp = $sld.find('.slider-value, .slider-input'), $cb = $sld.find('.slider-enable'),
      $bt = $sld.find('input[type=button]'), $sb = $bt.filter('.slider-set'),
      oldchk = data.checked, chk = (opts.checked != null) ? opts.checked : oldchk,
      dis = (opts.disabled != null) ? opts.disabled : ($sld.prop('disabled')==true);
    $.extend(data, opts);
    if (opts.unit != null) $sld.find('.slider-unit').html(opts.unit);
    if (opts.min != null) $inp.attr('min', opts.min);
    if (opts.max != null) $inp.attr('max', opts.max);
    if (opts.step != null) $inp.attr('step', opts.step);
    if (opts.default != null) {
      if ($sb.length == 1)
        $sb.data('set', opts.default);
      if (!chk)
        $inp.val(opts.default);
      data.default = Math.max(data.min, Math.min(data.max, 1*opts.default));
    }
    if (opts.value !== undefined) {
      if (opts.value === null)
        data.value = data.uservalue = data.default;
      else
        data.value = Math.max(data.min, Math.min(data.max, 1*opts.value));
      if (chk)
        $inp.attr('value', data.value).val(data.value);
      if (chk || data.uservalue == null)
        data.uservalue = data.value;
    }
    if (chk != oldchk) {
      $cb.prop('checked', chk);
      if (chk) {
        if (opts.reset || data.reset)
          data.value = data.default;
        else
          data.value = data.uservalue;
      } else {
        data.uservalue = data.value;
        data.value = data.default;
      }
      $inp.val(data.value);
    }
    $cb.prop('disabled', dis);
    $bt.prop('disabled', !chk || dis);
    $inp.prop('disabled', !chk || dis).prop('checked', chk);
    if (dis)
      $sld.addClass('disabled').prop('disabled', true).attr('disabled', true);
    else
      $sld.removeClass('disabled').prop('disabled', false).attr('disabled', false);
    $sld.data(data);
  });
};


/**
 * Highcharts
 */

var highchartsLoader;

$.fn.chart = function(options) {
  if (this.length == 0)
    return this;
  var $this = this;
  if (options === "destroy") {
    // destroy:
    var $cl = this.find(".has-chart").add(this.filter(".has-chart"));
    $cl.each(function() {
      var chart = $(this).data("chart");
      if (typeof chart == "object") {
        $(this).data("chart", null);
        chart.destroy();
      }
    });
  } else {
    // init:
    function init_charts() {
      $this.each(function() {
        var chart = Highcharts.chart(this, options, function() {
          if (this.userOptions && this.userOptions.onUpdate)
            this.userOptions.onUpdate.call(this, metrics);
        });
        $(this).data("chart", chart).addClass("has-chart get-window-resize").on("window-resize", function() {
          $(this).data("chart").reflow();
        });
        $(this).closest(".metric.chart").data("chart", chart);
      });
    }
    if (window.Highcharts) {
      init_charts();
    } else if (highchartsLoader) {
      highchartsLoader.then(init_charts);
    } else {
      highchartsLoader = $.ajax({
        url: (window.assets && window.assets["charts_js"]) || "/assets/charts.js?v=6.0.7",
        dataType: "script",
        cache: true,
        success: function(){ init_charts(); }
      });
    }
  }
  return this;
};


/**
 * DataTables
 */

var datatablesLoader;

$.fn.table = function(options) {
  if (this.length == 0)
    return this;
  var $this = this;
  if (options === "destroy") {
    // destroy:
    var $tl = this.find(".has-dataTable").add(this.filter(".has-dataTable"));
    $tl.each(function() {
      var table = $(this).data("dataTable");
      if (typeof table == "object") {
        $(this).data("dataTable", null);
        table.destroy();
      }
    });
  } else {
    // init:
    function init_tables() {
      $this.each(function() {
        $(this).one("init.dt", function(ev, settings) {
          if (settings && settings.oInstance && settings.oInit && settings.oInit.onUpdate)
            settings.oInit.onUpdate.call(settings.oInstance.api(), metrics);
        });
        var table = $(this).DataTable(options);
        $(this).data("dataTable", table).addClass("has-dataTable");
        $(this).closest(".metric.table").data("dataTable", table);
      });
    }
    if ($.fn.DataTables) {
      init_tables();
    } else if (datatablesLoader) {
      datatablesLoader.then(init_tables);
    } else {
      datatablesLoader = $.ajax({
        url: (window.assets && window.assets["tables_js"]) || "/assets/tables.js?v=1.10.18",
        dataType: "script",
        cache: true,
        success: function(){ init_tables(); }
      });
    }
  }
  return this;
};


/**
 * Framework Init
 */

$(function(){

  // Toggle night mode:
  $('body').on('click', '.toggle-night', function(event){
    var nm = $('body').toggleClass("night").hasClass("night");
    page.params["nm"] = 0+nm;
    updateLocation();
    event.stopPropagation();
    return false;
  });

  // Toggle fullscreen mode:
  document.fullScreenMode = document.fullScreen || document.mozFullScreen || document.webkitIsFullScreen;
  $(document).on('mozfullscreenchange webkitfullscreenchange fullscreenchange', function() {
    this.fullScreenMode = !this.fullScreenMode;
    if (this.fullScreenMode) {
      $('body').addClass("fullscreened");
    } else {
      $('body').removeClass("fullscreened");
    }
    $(window).trigger("resize");
  });
  $('body').on('click', '.toggle-fullscreen', function(evt) {
    element = document.body;
    if (element.requestFullscreen) {
      element.requestFullscreen();
    } else if (element.mozRequestFullScreen) {
      element.mozRequestFullScreen();
    } else if (element.webkitRequestFullscreen) {
      element.webkitRequestFullscreen(Element.ALLOW_KEYBOARD_INPUT);
    } else if (element.msRequestFullscreen) {
      element.msRequestFullscreen();
    }
    return false;
  });

  // AJAX page links, forms & buttons:
  $('body').on('click', 'a[target^="#"], form[target^="#"] .btn[type="submit"]', function(event){
    var method = $(this).data("method") || "get";
    var uri = $(this).attr("href");
    var target = $(this).attr("target");
    var data = {};
    if (method.toLowerCase() == "post") {
      var p = uri.split("?");
      if (p.length == 2) {
        uri = p[0];
        data = p[1];
      }
      if (uri == "" || uri == "#")
        uri = $("#main").data("uri");
    }
    if (!uri) {
      var frm = $(this.form);
      method = frm.attr("method") || "get";
      uri = frm.attr("action");
      target = frm.attr("target");
      data = frm.serialize();
      if (this.name)
        data += (data?"&":"") + encodeURI(this.name+"="+(this.value||"1"));
    }
    if ($(this).data("dismiss") == "modal")
      $(this).closest(".modal").removeClass("fade").modal("hide");
    if (!loaduri(target, method, uri, data))
      return true;
    event.preventDefault();
    event.stopPropagation();
    return false;
  });
  $('body').on('submit', 'form[target^="#"]', function(event) {
    var $frm = $(this);
    var method = $frm.attr("method") || "get";
    var uri = $frm.attr("action");
    var target = $frm.attr("target");
    var data = $frm.serialize();
    var $btn = $frm.find('input[type="submit"], button[type="submit"]').first();
    if ($btn.length && $btn.attr("name"))
      data += (data?"&":"") + encodeURI($btn.attr("name") +"="+ ($btn.val()||"1"));
    if ($frm.data("dismiss") == "modal" || $btn.data("dismiss") == "modal")
      $frm.closest(".modal").removeClass("fade").modal("hide");
    if (!loaduri(target, method, uri, data))
      return true;
    event.preventDefault();
    event.stopPropagation();
    return false;
  });

  // AJAX command links & buttons:
  $('body').on('click', '.btn[data-cmd]', function(event){
    var btn = $(this);
    var cmd = btn.data("cmd");
    var tgt = btn.data("target");
    var updcnt = btn.data("watchcnt") || 0;
    var updint = btn.data("watchint") || 2;
    btn.prop("disabled", true);
    $(tgt).data("updcnt", 0);
    loadcmd(cmd, tgt).then(function(){
      btn.prop("disabled", false);
      $(tgt).data("updcnt", updcnt);
      $(tgt).data("updint", updint);
      $(tgt).data("updlast", now());
    }, function(){
      btn.prop("disabled", false);
    });
    event.stopPropagation();
    return false;
  });
  $('body').on('click', '.btn[data-js]', function(event){
    var btn = $(this);
    var js = btn.data("js");
    var tgt = btn.data("target");
    var updcnt = btn.data("watchcnt") || 0;
    var updint = btn.data("watchint") || 2;
    btn.prop("disabled", true);
    $(tgt).data("updcnt", 0);
    loadjs(js, tgt).then(function(){
      btn.prop("disabled", false);
      $(tgt).data("updcnt", updcnt);
      $(tgt).data("updint", updint);
      $(tgt).data("updlast", now());
    }, function(){
      btn.prop("disabled", false);
    });
    event.stopPropagation();
    return false;
  });

  // Long touch buttons:
  var longtouchTimeout, $longtouchProgress;
  $('body').on('touchstart', '.btn-longtouch .btn, .btn.btn-longtouch', function(ev) {
    var $this = $(this);
    if ($this.prop('disabled')) return;
    var action = $this.attr("title") || $this.text();
    if (navigator.vibrate) navigator.vibrate([100,400,100,400,100,400]);
    $longtouchProgress = $('<div class="hover-progress longtouch"><div class="hover-progress-body"><div class="info">Hold touch for/to</div><div class="action">'+encode_html(action)+'</div><div class="progress"><div class="progress-bar progress-bar-info" style="width:0%"></div></div></div></div>').appendTo("body").find(".progress-bar");
    window.getComputedStyle($longtouchProgress.get(0)).width;
    $longtouchProgress.css("width", "100%");
    longtouchTimeout = window.setTimeout(function() {
      if (navigator.vibrate) navigator.vibrate(1000);
      if ($longtouchProgress) $longtouchProgress.closest(".hover-progress").remove();
      longtouchTimeout = null;
      $longtouchProgress = null;
      $this.trigger('click');
    }, 1500);
    ev.preventDefault();
  }).on('touchcancel touchend', '.btn-longtouch .btn, .btn.btn-longtouch', function(ev) {
    window.clearTimeout(longtouchTimeout);
    if (navigator.vibrate) navigator.vibrate(0);
    if ($longtouchProgress) $longtouchProgress.closest(".hover-progress").remove();
    longtouchTimeout = null;
    $longtouchProgress = null;
    ev.preventDefault();
  }).on('contextmenu', function(ev) {
    if ($longtouchProgress) {
      ev.preventDefault();
      ev.stopPropagation();
      ev.stopImmediatePropagation();
      return false;
    }
  });

  // Slider widget event handling:
  $("body").on("change", ".slider-enable", function(evt) {
    var $this = $(this), $sld = $this.closest(".slider"), $inp = $sld.find(".slider-input, .slider-value");
    $this.slider({ checked: this.checked });
    $inp.trigger("input", true).trigger("change", true);
  });
  $("body").on("input change", ".slider-value", function(evt, noprop) {
    var $this = $(this), $sld = $this.closest(".slider"), $inp = $sld.find(".slider-input");
    if (!noprop) {
      $this.slider({ value: this.value });
      $inp.trigger(evt.type, true);
    }
  });
  $("body").on("input change", ".slider-input", function(evt, noprop) {
    var $this = $(this), $sld = $this.closest(".slider"), $inp = $sld.find(".slider-value");
    if (!noprop) {
      $this.slider({ value: this.value });
      $inp.trigger(evt.type, true);
    }
  });
  $("body").on("click", ".slider-up", function(evt) {
    $(this).closest(".slider").find(".slider-input").val(function() {
      return Math.min(1*this.value + (1*this.step||1), this.max);
    }).trigger("input").trigger("change");
  });
  $("body").on("click", ".slider-down", function(evt) {
    $(this).closest(".slider").find(".slider-input").val(function() {
      return Math.max(1*this.value - (1*this.step||1), this.min);
    }).trigger("input").trigger("change");
  });
  $("body").on("click", ".slider-set", function(evt) {
    var $inp = $(this).closest(".slider").find(".slider-input");
    $inp.val($(this).data("set")).trigger("input").trigger("change");
  });

  // data-toggle="filefialog":
  $("body").on('click', '.btn[data-toggle="filedialog"]', function(evt) {
    var $this = $(this);
    var $tgt = $($this.data("target"));
    var $inp = $($this.data("input"));
    if ($tgt.length && $inp.length) {
      var val = $inp.val();
      var opt = {
        show: true,
        onSubmit: function(input) { $inp.val(input.path); }
      };
      if (val) opt.path = val;
      $tgt.filedialog(opt);
    }
  });

  // Metrics displays:
  $("body").on('msg:metrics', '.receiver', function(e, update) {
    $(this).find(".metric").each(function() {
      var $el = $(this), metric = $el.data("metric"), prec = $el.data("prec"), scale = $el.data("scale");
      if (!metric) return;
      // filter:
      var keys = metric.split(","), val;
      for (var i=0; i<keys.length; i++) {
        if ((val = update[keys[i]]) != null) break;
      }
      if (val == null) return;
      // process:
      if ($el.hasClass("text")) {
        $el.children(".value").text(val);
      } else if ($el.hasClass("number")) {
        var vf = val;
        if (scale != null) vf = Number(vf) * scale;
        if (prec != null) vf = Number(vf).toFixed(prec);
        $el.children(".value").text(vf);
      } else if ($el.hasClass("progress")) {
        var vf = val;
        if (scale != null) vf = Number(vf) * scale;
        if (prec != null) vf = Number(vf).toFixed(prec);
        var $pb = $(this.firstElementChild), min = $pb.attr("aria-valuemin"), max = $pb.attr("aria-valuemax");
        var vp = (val-min) / (max-min) * 100;
        $pb.css("width", vp+"%").attr("aria-valuenow", vp).find(".value").text(vf);
        var lw = 0; $pb.find("span").each(function(){ lw += $(this).width(); });
        if (($pb.parent().width()*vp/100) < lw) $pb.addClass("value-low"); else $pb.removeClass("value-low");
      } else if ($el.hasClass("chart")) {
        var ch = $(this).data("chart");
        if (ch && ch.userOptions && ch.userOptions.onUpdate)
          ch.userOptions.onUpdate.call(ch, update);
      } else if ($el.hasClass("table")) {
        var dt = $(this).data("dataTable");
        if (dt && dt.settings() && dt.settings()[0] && dt.settings()[0].oInit && dt.settings()[0].oInit.onUpdate)
          dt.settings()[0].oInit.onUpdate.call(dt, update);
      } else {
        $el.text(val);
      }
    });
  });

  // Modal autoclear:
  $("body").on("hidden.bs.modal", ".modal", function(evt) {
    $(this).find(".modal-autoclear").empty();
  });

  // Proxy window resize:
  $(window).on("resize", function(event){
    $(".get-window-resize").trigger("window-resize");
  });
  $("body").on("window-resize", ".table-scrollable", function(evt) {
    var bw = $(this).find('>tbody').get(0).scrollWidth;
    if (bw) $(this).find('>thead').css('width', bw);
  });

  // Monitor timer:
  if (!monitorTimer)
    monitorTimer = window.setInterval(monitorUpdate, 1000);

  // AJAX page init:
  window.onpopstate = loadPage;
  loadPage();
});
