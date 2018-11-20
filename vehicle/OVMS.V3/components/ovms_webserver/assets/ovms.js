/* ovms.js | (c) Michael Balzer | https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3 */

function setcontent(tgt, uri, text){
  if (tgt[0].id == "main") {
    $("#nav .dropdown.open .dropdown-toggle").dropdown("toggle");
    $("#nav .navbar-collapse").collapse("hide");
    $("#nav li").removeClass("active");
    var me = $("#nav [href='"+uri+"']");
    me.parents("li").addClass("active");
  }
  tgt[0].scrollIntoView();
  tgt.html(text).hide().fadeIn(50);
  if (tgt[0].id == "main") {
    if (me.length > 0)
      document.title = "OVMS " + (me.attr("title") || me.text());
    else
      document.title = "OVMS Console";
  }
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

var monitorTimer, last_monotonic = 0;
var ws, ws_inhibit = 0;
var metrics = {};
var shellhist = [""], shellhpos = 0;

function initSocketConnection(){
  ws = new WebSocket('ws://' + location.host + '/msg');
  ws.onopen = function(ev)  { console.log(ev); };
  ws.onerror = function(ev) { console.log(ev); };
  ws.onclose = function(ev) { console.log(ev); };
  ws.onmessage = function(ev) {
    msg = JSON.parse(ev.data);
    if (msg && msg.event) {
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
    else if (msg && msg.metrics) {
      $.extend(metrics, msg.metrics);
      $(".receiver").trigger("msg:metrics", msg.metrics);
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

function encode_html(s){
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/'/g, '&apos;')
    .replace(/"/g, '&quot;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;');
}

$.fn.listEditor = function(op, data){
  this.each(function(){
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

  // AJAX links/buttons:
  
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
  
  if (!monitorTimer)
    monitorTimer = window.setInterval(monitorUpdate, 1000);
  
  $(window).on("resize", function(event){
    $(".get-window-resize").trigger("window-resize");
  });
  
  $("body").on("hidden.bs.modal", ".modal", function(evt) {
    $(this).find(".modal-autoclear").html("");
  });
  
  window.onpopstate = getpage;
  getpage();
});
