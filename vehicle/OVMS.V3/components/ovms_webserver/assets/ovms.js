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

$('body').on('click', 'a[target^="#"], form[target^="#"] .btn', function(event){
  
  var method = "get";
  var uri = $(this).attr("href");
  var target = $(this).attr("target");
  var data = {};
  
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

function getpage() {
  uri = (location.hash || "#/home").substr(1);
  if ($("#main").data("uri") != uri) {
    loaduri("#main", "get", uri, {});
  }
}

$(function(){
  window.onpopstate = getpage;
  getpage();
});
