/*
;    Project:       Open Vehicle Monitor System
;    Date:          November 2025
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include "ovms_webserver.h"

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())

/**
 * HandleCfgBackup: config backup/restore (URL /cfg/backup)
 */
void OvmsWebServer::HandleCfgBackup(PageEntry_t& p, PageContext_t& c)
{
  c.head(200);
  c.print(
    "<style>\n"
    "#backupbrowser tbody { height: 186px; }\n"
    "</style>\n"
    "\n"
    "<div class=\"panel panel-primary\">\n"
      "<div class=\"panel-heading\"><span class=\"hidden-xs\">Configuration</span> Backup &amp; Restore</div>\n"
      "<div class=\"panel-body\">\n"
        "<div class=\"filebrowser\" id=\"backupbrowser\" />\n"
        "<div class=\"action-menu text-right\">\n"
          "<button type=\"button\" class=\"btn btn-default pull-left\" id=\"action-newdir\">New dir</button>\n"
          "<button type=\"button\" class=\"btn btn-primary\" id=\"action-backup\" disabled>Create backup</button>\n"
          "<button type=\"button\" class=\"btn btn-primary\" id=\"action-restore\" disabled>Restore backup</button>\n"
        "</div>\n"
        "<pre id=\"log\" style=\"margin-top:15px\"/>\n"
      "</div>\n"
      "<div class=\"panel-footer\">\n"
        "<p>Use this tool to create or restore backups of your system configuration &amp; scripts.\n"
          "User files or directories in <code>/store</code> will not be included or restored.\n"
          "ZIP files are password protected (hint: use 7z to unzip/create on a PC).</p>\n"
        "<p>Note: the module will perform a reboot after successful restore.</p>\n"
      "</div>\n"
    "</div>\n"
    "\n"
    "<script>\n"
    "(function(){\n"
      "var suggest = '/sd/backup/cfg-' + new Date().toISOString().substr(2,8).replaceAll('-','') + '.zip';\n"
      "var zip = { exists: false, iszip: false };\n"
      "var $panel = $('.panel-body');\n"
    "\n"
      "function updateButtons(enable) {\n"
        "if (enable === false) {\n"
          "$panel.addClass(\"loading disabled\");\n"
          "return;\n"
        "}\n"
        "$panel.removeClass(\"loading disabled\");\n"
        "if (!zip.iszip) {\n"
          "$('#action-backup').prop('disabled', true);\n"
          "$('#action-restore').prop('disabled', true);\n"
        "} else if (zip.exists) {\n"
          "$('#action-backup').prop('disabled', false).text(\"Overwrite backup\");\n"
          "$('#action-restore').prop('disabled', false);\n"
        "} else {\n"
          "$('#action-backup').prop('disabled', false).text(\"Create backup\");\n"
          "$('#action-restore').prop('disabled', true);\n"
        "}\n"
      "};\n"
    "\n"
      "$('#backupbrowser').filebrowser({\n"
        "input: zip,\n"
        "path: suggest,\n"
        "quicknav: ['/sd/', '/sd/backup/', suggest],\n"
        "filter: function(f) { return f.isdir || f.name.match('\\\\.zip$'); },\n"
        "sortBy: 'date',\n"
        "sortDir: -1,\n"
        "onPathChange: function(input) {\n"
          "input.iszip = (input.file.match('\\\\.zip$') != null);\n"
          "if (!input.iszip) {\n"
            "updateButtons();\n"
            "$('#log').html('<div class=\"bg-warning\">Please select/enter a zip file</div>');\n"
          "} else {\n"
            "loadcmd(\"vfs stat \" + input.path).always(function(data) {\n"
              "if (typeof data != \"string\" || data.startsWith(\"Error\")) {\n"
                "input.exists = false;\n"
                "$('#log').empty();\n"
              "} else {\n"
                "input.exists = true;\n"
                "input.url = getPathURL(input.path);\n"
                "$('#log').text(data).append(input.url ? '\\n<a href=\"'+input.url+'\">Download '+input.file+'</a>' : '');\n"
              "}\n"
              "updateButtons();\n"
            "});\n"
          "}\n"
        "},\n"
      "});\n"
    "\n"
      "$('#action-newdir').on('click', function(ev) {\n"
        "$('#backupbrowser').filebrowser('newDir');\n"
      "});\n"
    "\n"
      "$('#action-backup').on('click', function(ev) {\n"
        "$('#backupbrowser').filebrowser('stopLoad');\n"
        "promptdialog(\"password\", \"Create backup\", \"ZIP password / empty = use module password\", [\"Cancel\", \"Create backup\"], function(ok, password) {\n"
          "if (ok) {\n"
            "updateButtons(false);\n"
            "loadcmd(\"config backup \" + zip.path + ((password) ? \" \" + password : \"\"), \"#log\", 120).done(function(output) {\n"
              "if (output.indexOf(\"Error\") < 0) {\n"
                "zip.exists = true;\n"
                "zip.url = getPathURL(zip.path);\n"
                "$('#log').append(zip.url ? '\\n<a href=\"'+zip.url+'\">Download '+zip.file+'</a>' : '');\n"
                "$('#backupbrowser').filebrowser('loadDir');\n"
              "}\n"
            "}).always(function() {\n"
              "updateButtons();\n"
            "});\n"
          "}\n"
        "});\n"
      "});\n"
    "\n"
      "$('#action-restore').on('click', function(ev) {\n"
        "$('#backupbrowser').filebrowser('stopLoad');\n"
        "promptdialog(\"password\", \"Restore backup\", \"ZIP password / empty = use module password\", [\"Cancel\", \"Restore backup\"], function(ok, password) {\n"
          "if (ok) {\n"
            "var rebooting = false;\n"
            "updateButtons(false);\n"
            "loadcmd(\"config restore \" + zip.path + ((password) ? \" \" + password : \"\"), \"#log\", function(msg) {\n"
              "if (msg.text && msg.text.indexOf(\"rebooting\") >= 0) {\n"
                "rebooting = true;\n"
                "msg.request.abort();\n"
              "}\n"
              "return rebooting ? null : standardTextFilter(msg);\n"
            "}, 120).always(function(data, status) {\n"
              "updateButtons();\n"
              "if (rebooting) $('#log').reconnectTicker();\n"
            "});\n"
          "}\n"
        "});\n"
      "});\n"
    "\n"
    "})();\n"
    "</script>\n"
  );
  c.done();
}
