/* ovms.js | (c) Michael Balzer | https://github.com/openvehicles/Open-Vehicle-Monitoring-System-3 */

function setcontent(tgt, uri, text){
  $("#nav .dropdown.open .dropdown-toggle").dropdown("toggle");
  $("#nav .navbar-collapse").collapse("hide");
  $("#nav li").removeClass("active");
  var me = $("#nav [href='"+uri+"']");
  me.parents("li").addClass("active");
  tgt[0].scrollIntoView();
  tgt.html(text).hide().fadeIn(50);
  if (me.length > 0)
    document.title = "OVMS " + (me.attr("title") || me.text());
  else
    document.title = "OVMS Console";
}

function loaduri(target, method, uri, data){
  var tgt = $(target);
  if (tgt.length != 1)
    return false;
  
  tgt.data("uri", uri);
  location.hash = "#" + uri;
  
  $.ajax({ "type": method, "url": uri, "data": data,
    "timeout": 5000,
    "beforeSend": function(){
      $("html").addClass("loading");
    },
    "complete": function(){
      $("html").removeClass("loading");
    },
    "success": function(response){
      setcontent(tgt, uri, response);
    },
    "error": function(response, status, httperror){
      var text = response.responseText || httperror || status;
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
  return $.ajax({ "type": "post", "url": "/api/execute", "data": data,
    "timeout": 5000,
    "beforeSend": function(){
      output.addClass("loading");
    },
    "complete": function(){
      output.removeClass("loading");
    },
    "success": function(response){
      output.css("min-height", output.height());
      if (outmode == "+")
        output.html(output.html() + $("<div/>").text(response).html());
      else
        output.html($("<div/>").text(response).html());
    },
    "error": function(response, status, httperror){
      var resptext = response.responseText || httperror || status;
      output.css("min-height", output.height());
      if (outmode == "+")
        output.html(output.html() + $("<div/>").text(resptext).html());
      else
        output.html($("<div/>").text(resptext).html());
    },
  });
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

function monitorUpdate(){
  $(".monitor").each(function(){
    var cnt = $(this).data("updcnt");
    var int = $(this).data("updint");
    var last = $(this).data("updlast");
    var cmd = $(this).data("updcmd");
    if (!cnt || !cmd || (now()-last) < int)
      return;
    loadcmd(cmd, $(this));
    $(this).data("updcnt", cnt-1);
    $(this).data("updlast", now());
  });
}

var monitorTimer;

$(function(){
  
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
    }
    if (!loaduri(target, method, uri, data))
      return true;
    event.stopPropagation();
    return false;
  });

  $('body').on('click', '.btn[data-cmd]', function(){
    var btn = $(this);
    var cmd = btn.data("cmd");
    var tgt = btn.data("target");
    var updcnt = btn.data("watchcnt") || 3;
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

  $('.toggle-night').on('click', function(){
    $('body').toggleClass("night");
    event.stopPropagation();
    return false;
  });

  if (!monitorTimer)
    monitorTimer = window.setInterval(monitorUpdate, 1000);
  
  $(window).on("resize", function(){
    $(".get-window-resize").trigger("window-resize");
  });
  
  window.onpopstate = getpage;
  getpage();
});
