/* ovms.js | (c) Michael Balzer | https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3 */


/**
 * Utilities
 */

function after(seconds, fn){
  window.setTimeout(fn, seconds*1000);
}

function now() {
  return Math.floor((new Date()).getTime() / 1000);
}

function getpage() {
  uri = (location.hash || "#/home").substr(1);
  if ($("#main").data("uri") != uri) {
    loaduri("#main", "get", uri, {});
  }
}
function reloadpage() {
  uri = (location.hash || "#/home").substr(1);
  loaduri("#main", "get", uri, {});
}

function encode_html(s){
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/'/g, '&apos;')
    .replace(/"/g, '&quot;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;');
}


/**
 * AJAX Pages & Commands
 */

function setcontent(tgt, uri, text){
  if (!tgt || !tgt.length) return;

  tgt.find(".receiver").unsubscribe();

  if (tgt[0].id == "main") {
    $("#nav .dropdown.open .dropdown-toggle").dropdown("toggle");
    $("#nav .navbar-collapse").collapse("hide");
    $("#nav li").removeClass("active");
    var mi = $("#nav [href='"+uri+"']");
    mi.parents("li").addClass("active");
    tgt[0].scrollIntoView();
    tgt.html(text).hide().fadeIn(50);
    if (mi.length > 0)
      document.title = "OVMS " + (mi.attr("title") || mi.text());
    else
      document.title = "OVMS Console";
  } else {
    tgt[0].scrollIntoView();
    tgt.html(text).hide().fadeIn(50);
  }

  tgt.find(".receiver").subscribe();
}

function loaduri(target, method, uri, data){
  var tgt = $(target), cont;
  if (tgt.length != 1)
    return false;
  
  tgt.data("uri", uri);
  if (tgt[0].id == "main") {
    cont = $("html");
    location.hash = "#" + uri;
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

function loadcmd(command, target){
  var data = "command=" + encodeURIComponent(command);
  var output, outmode = "";

  if (typeof target == "object") {
    output = target;
  }
  else if (target.startsWith("+")) {
    outmode = "+";
    output = $(target.substr(1));
  } else {
    output = $(target);
  }

  var lastlen = 0, xhr, timeouthd, timeout = 20;
  if (/^(test |ota |co .* scan)/.test(command)) timeout = 300;
  var checkabort = function(){ if (xhr.readyState != 4) xhr.abort("timeout"); };

  xhr = $.ajax({ "type": "post", "url": "/api/execute", "data": data,
    "timeout": 0,
    "beforeSend": function(){
      output.addClass("loading");
      var mh = parseInt(output.css("max-height")), h = output.outerHeight();
      output.css("min-height", mh ? Math.min(h, mh) : h);
      output.scrollTop(output.get(0).scrollHeight);
      timeouthd = window.setTimeout(checkabort, timeout*1000);
    },
    "complete": function(){
      window.clearTimeout(timeouthd);
      output.removeClass("loading");
      var mh = parseInt(output.css("max-height")), h = output.outerHeight();
      output.css("min-height", mh ? Math.min(h, mh) : h);
    },
    "xhrFields": {
      onprogress: function(e){
        if (e.currentTarget.status != 200)
          return;
        var response = e.currentTarget.response;
        var addtext = response.substring(lastlen);
        lastlen = response.length;
        if (outmode == "") { output.html(""); outmode = "+"; }
        output.html(output.html() + $("<div/>").text(addtext).html());
        output.scrollTop(output.get(0).scrollHeight);
        window.clearTimeout(timeouthd);
        timeouthd = window.setTimeout(checkabort, timeout*1000);
      },
    },
    "error": function(response, xhrerror, httperror){
      console.log("loadcmd '" + command + "' ERROR: xhrerror=" + xhrerror + ", httperror=" + httperror);
      var txt;
      if (response.status == 401 || response.status == 403)
        txt = "Session expired. <a class=\"btn btn-sm btn-default\" href=\"javascript:reloadpage()\">Login</a>";
      else if (response.status >= 400)
        txt = "Error " + response.status + " " + response.statusText;
      else
        txt = "Request " + (xhrerror||"failed") + ", please retry";
      if (outmode == "")
        output.html('<div class="bg-danger">'+txt+'</div>');
      else
        output.html(output.html() + '<div class="bg-danger">'+txt+'</div>');
      output.scrollTop(output.get(0).scrollHeight);
    },
  });

  return xhr;
}


/**
 * WebSocket Connection
 */

var monitorTimer, last_monotonic = 0;
var ws, ws_inhibit = 0;
var metrics = {};
var shellhist = [""], shellhpos = 0;

function initSocketConnection(){
  ws = new WebSocket('ws://' + location.host + '/msg');
  ws.onopen = function(ev) {
    console.log(ev);
    $(".receiver").subscribe();
  };
  ws.onerror = function(ev) { console.log(ev); };
  ws.onclose = function(ev) { console.log(ev); };
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
          var evf = $(this).data("events");
          if (cmd && evf && msg.event.match(evf)) {
            $(this).data("updlast", now());
            loadcmd(cmd, $(this));
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
    }
  };
}

function monitorInit(force){
  $(".monitor").each(function(){
    var cmd = $(this).data("updcmd");
    var txt = $(this).text();
    if (cmd && (force || !txt)) {
      $(this).data("updlast", now());
      loadcmd(cmd, $(this));
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
    if (!cnt || !cmd || (now()-last) < int)
      return;
    $(this).data("updcnt", cnt-1);
    $(this).data("updlast", now());
    loadcmd(cmd, $(this));
  });
}


function processNotification(msg) {
  var opts = { timeout: 0 };
  if (msg.type == "info") {
    opts.title = '<span class="lead text-info"><i>🛈</i>' + msg.subtype + ' Info</span>';
    opts.timeout = 60;
  }
  else if (msg.type == "alert") {
    opts.title = '<span class="lead text-danger"><i>⚠</i>' + msg.subtype + ' Alert</span>';
  }
  else if (msg.type == "error") {
    opts.title = '<span class="lead text-warning"><i>⛍</i>' + msg.subtype + ' Error</span>';
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


/**
 * UI Widgets
 */

// Plugin Maker
// credits: https://www.bitovi.com/blog/writing-the-perfect-jquery-plugin
$.pluginMaker = function(plugin) {
  $.fn[plugin.prototype.cname] = function(options) {
    var args = $.makeArray(arguments), after = args.slice(1);
    return this.each(function() {
      // see if we have an instance
      var instance = $.data(this, plugin.prototype.cname);
      if (instance) {
        if (typeof options == "string") {
          // call a method on the instance
          if ($.isFunction(instance[options]))
            instance[options].apply(instance, after);
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
    })
  };
};

// OVMS namespace:
var ovms = {};

// Widget root class:
ovms.Widget = function(el, options) {
  if (el) this.init(el, options);
}
$.extend(ovms.Widget.prototype, {
  cname: "widget",
  options: {},
  init: function(el, options) {
    this.$el = $(el);
    this.$el.data(this.cname, this);
    this.$el.addClass(this.cname);
    this.options = $.extend({}, this.options, options);
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
    onShown: null,
    onHidden: null,
    onUpdate: null,
    buttons: [{}],
    timeout: 0,
  },
  init: function(el, options) {
    if ($(el).parent().length == 0) {
      options = $.extend(options, { show: true, isDynamic: true });
    }
    ovms.Widget.prototype.init.call(this, el, options);
    this.input = {};
    // convert element to modal if not predefined by user:
    if (this.$el.children().length == 0) {
      this.$el.html('<div class="modal-dialog"><div class="modal-content"><div class="modal-header"><button type="button" class="close" data-dismiss="modal" aria-label="Close"><span aria-hidden="true">&times;</span></button><h4 class="modal-title"></h4></div><div class="modal-body"></div><div class="modal-footer"></div></div></div></div>');
      this.$el.addClass("modal");
    }
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
    this.$el.find('.modal-title').html(this.options.title);
    this.$el.find('.modal-body').html(this.options.body);
    var footer = this.$el.find('.modal-footer');
    footer.html('');
    for (var i = 0; i < this.options.buttons.length; i++) {
      var btn = $.extend({ label: 'Close', index: i, btnClass: 'default', autoHide: true, action: null }, this.options.buttons[i]);
      this.options.buttons[i] = btn;
      btn.$el = $('<button type="button" class="btn btn-'+btn.btnClass+'" value="'+btn.index+'">'+btn.label+'</button>')
        .appendTo(footer).on('click', $.proxy(this.onClick, this, btn));
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
  onShown: function() {
    this.input.button = null;
    this.$el.find('.form-control, .btn').first().focus();
    if (this.options.onShown)
      this.options.onShown.call(this.$el, this.input);
    if (this.options.timeout)
      after(this.options.timeout, $.proxy(this.hide, this));
  },
  onClick: function(button) {
    this.input.button = button.index;
    if (button.autoHide)
      this.hide();
    else if (button.action)
      button.action.call(this.$el, this.input);
  },
  onHidden: function() {
    if (this.options.isDynamic)
      this.$el.detach();
    if (this.options.onHidden)
      this.options.onHidden.call(this.$el, this.input);
    if (this.input.button != null) {
      var button = this.options.buttons[this.input.button];
      if (button.action)
        button.action.call(this.$el, this.input);
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
    options.onHidden = function(input) { _action(input.button); };
  }
  return this.dialog(options);
};
var confirmdialog = function() { return $.fn.confirmdialog.apply($('<div />'), arguments); };

$.fn.promptdialog = function(_type, _title, _prompt, _buttons, _action) {
  var options = { show: true, title: _title, buttons: [] };
  var id = Math.random().toString().substr(2);
  options.body = '<label for="prompt'+id+'">'+_prompt+'</label><input id="prompt'+id+'" type="'+_type+'" class="form-control">';
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
  options.onHidden = function(input) {
    if (input.button) input.text = $(this).find('input').val();
    if (_action) _action(input.button, input.text);
  };
  return this.dialog(options);
};
var promptdialog = function() { return $.fn.promptdialog.apply($('<div />'), arguments); };


// Template list editor:

$.fn.listEditor = function(op, data){
  return this.each(function(){
    if (op) {
      $(this).trigger('list:'+op, data);
    } else {
      // init:
      var el_itemid = $(this).find('.list-item-id');
      var el_body = $(this).find('.list-items');
      var el_template = $(this).find('template').html();
      $(this).on('list:addItem', function(evt, data) {
        var id = Number(el_itemid.val()) + 1;
        el_itemid.val(id);
        var txt = el_template.replace(/ITEM_ID/g, id);
        if (data) {
          for (key in data)
            txt = txt.replace(new RegExp('ITEM_'+key,'g'), encode_html(data[key]));
        }
        txt = txt.replace(/ITEM_\w+/g, '');
        $(txt).appendTo(el_body);
      });
      $(this).on('click', '.list-item-add', function(evt) {
        var data = JSON.parse($(this).data('preset') || '{}');
        $(this).trigger('list:addItem', data);
      });
      $(this).on('click', '.list-item-del', function(evt) {
        $(this).closest('.list-item').remove();
      });
    }
  });
}


/**
 * Framework Init
 */

$(function(){

  // Toggle night mode:
  $('body').on('click', '.toggle-night', function(event){
    $('body').toggleClass("night");
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

  // Slider widget:
  $("body").on("change", ".slider-enable", function(evt) {
    var slider = $(this).closest(".slider");
    slider.find("input[type=number]").prop("disabled", !this.checked).trigger("input");
    slider.find("input[type=range]").prop("disabled", !this.checked).trigger("input");
    slider.find("input[type=button]").prop("disabled", !this.checked);
  });
  $("body").on("input", ".slider-value", function(evt) {
    $(this).closest(".slider").find(".slider-input").val(this.value);
  });
  $("body").on("input", ".slider-input", function(evt) {
    if (this.disabled)
      this.value = $(this).data("default");
    $(this).closest(".slider").find(".slider-value").val(this.value);
  });
  $("body").on("click", ".slider-up", function(evt) {
    $(this).closest(".slider").find(".slider-input")
      .val(function(){return 1*this.value + 1;}).trigger("input");
  });
  $("body").on("click", ".slider-down", function(evt) {
    $(this).closest(".slider").find(".slider-input")
      .val(function(){return 1*this.value - 1;}).trigger("input");
  });

  // Modal autoclear:
  $("body").on("hidden.bs.modal", ".modal", function(evt) {
    $(this).find(".modal-autoclear").html("");
  });

  // Proxy window resize:
  $(window).on("resize", function(event){
    $(".get-window-resize").trigger("window-resize");
  });

  // Monitor timer:
  if (!monitorTimer)
    monitorTimer = window.setInterval(monitorUpdate, 1000);

  // AJAX page init:
  window.onpopstate = getpage;
  getpage();
});
