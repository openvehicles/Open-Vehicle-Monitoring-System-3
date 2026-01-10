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
 * HandleCfgWebServer: configure web server (URL /cfg/webserver)
 */
void OvmsWebServer::HandleCfgWebServer(PageEntry_t& p, PageContext_t& c)
{
  auto lock = MyConfig.Lock();
  std::string error, warn;
  std::string ws_txqueuesize, docroot, auth_domain, auth_file;
  bool enable_files, enable_dirlist, auth_global;
  extram::string tls_cert, tls_key;

  if (c.method == "POST") {
    // process form submission:
    ws_txqueuesize = c.getvar("ws_txqueuesize");
    docroot = c.getvar("docroot");
    auth_domain = c.getvar("auth_domain");
    auth_file = c.getvar("auth_file");
    enable_files = (c.getvar("enable_files") == "yes");
    enable_dirlist = (c.getvar("enable_dirlist") == "yes");
    auth_global = (c.getvar("auth_global") == "yes");
    c.getvar("tls_cert", tls_cert);
    c.getvar("tls_key", tls_key);

    // validate:
    if (ws_txqueuesize != "" && atoi(ws_txqueuesize.c_str()) < 10) {
      error += "<li data-input=\"ws_txqueuesize\">TX queue size must be at least 10</li>";
    }
    if (docroot != "" && docroot[0] != '/') {
      error += "<li data-input=\"docroot\">Document root must start with '/'</li>";
    }
    if (docroot == "/" || docroot == "/store" || docroot == "/store/" || startsWith(docroot, "/store/ovms_config")) {
      warn += "<li data-input=\"docroot\">Document root <code>" + docroot + "</code> may open access to OVMS configuration files, consider using a sub directory</li>";
    }
    if (!tls_cert.empty() && !startsWith(tls_cert, "-----BEGIN CERTIFICATE-----")) {
      error += "<li data-input=\"tls_cert\">TLS certificate must be in PEM CERTIFICATE format</li>";
    }
    if (!tls_key.empty() && !startsWith(tls_key, "-----BEGIN PRIVATE KEY-----")) {
      error += "<li data-input=\"tls_key\">TLS private key must be in PEM PRIVATE KEY format</li>";
    }
    if (tls_cert.empty() != tls_key.empty()) {
      error += "<li data-input=\"tls_cert,tls_key\">Both TLS certificate and private key must be given</li>";
    }

    // save TLS files:
    if (error == "") {
      if (tls_cert.empty()) {
        unlink("/store/tls/webserver.crt");
        unlink("/store/tls/webserver.key");
      }
      else {
        if (save_file("/store/tls/webserver.crt", tls_cert) != 0) {
          error += "<li data-input=\"tls_cert\">Error saving TLS certificate: ";
          error += strerror(errno);
          error += "</li>";
        }
        if (save_file("/store/tls/webserver.key", tls_key) != 0) {
          error += "<li data-input=\"tls_key\">Error saving TLS private key: ";
          error += strerror(errno);
          error += "</li>";
        }
      }
    }

    if (error == "") {
      // success:
      if (ws_txqueuesize == "")   MyConfig.DeleteInstance("http.server", "ws.txqueuesize");
      else                        MyConfig.SetParamValue("http.server", "ws.txqueuesize", ws_txqueuesize);
      if (docroot == "")          MyConfig.DeleteInstance("http.server", "docroot");
      else                        MyConfig.SetParamValue("http.server", "docroot", docroot);
      if (auth_domain == "")      MyConfig.DeleteInstance("http.server", "auth.domain");
      else                        MyConfig.SetParamValue("http.server", "auth.domain", auth_domain);
      if (auth_file == "")        MyConfig.DeleteInstance("http.server", "auth.file");
      else                        MyConfig.SetParamValue("http.server", "auth.file", auth_file);

      MyConfig.SetParamValueBool("http.server", "enable.files", enable_files);
      MyConfig.SetParamValueBool("http.server", "enable.dirlist", enable_dirlist);
      MyConfig.SetParamValueBool("http.server", "auth.global", auth_global);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Webserver configuration saved.</p>"
        "<p>Note: if you changed the TLS certificate or key, you need to reboot"
        " the module to activate the change.</p>");
      if (warn != "") {
        warn = "<p class=\"lead\">Warning:</p><ul class=\"warnlist\">" + warn + "</ul>";
        c.alert("warning", warn.c_str());
      }
      OutputHome(p, c);
      c.done();
      return;
    }

    // output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else {
    // read configuration:
    ws_txqueuesize = MyConfig.GetParamValue("http.server", "ws.txqueuesize");
    docroot = MyConfig.GetParamValue("http.server", "docroot");
    auth_domain = MyConfig.GetParamValue("http.server", "auth.domain");
    auth_file = MyConfig.GetParamValue("http.server", "auth.file");
    enable_files = MyConfig.GetParamValueBool("http.server", "enable.files", true);
    enable_dirlist = MyConfig.GetParamValueBool("http.server", "enable.dirlist", true);
    auth_global = MyConfig.GetParamValueBool("http.server", "auth.global", true);
    load_file("/store/tls/webserver.crt", tls_cert);
    load_file("/store/tls/webserver.key", tls_key);

    // generate form:
    c.head(200);
  }

  c.panel_start("primary", "Webserver configuration");
  c.form_start(p.uri);

  c.fieldset_start("File Access");

  c.input_checkbox("Enable file access", "enable_files", enable_files,
    "<p>If enabled, paths not handled by the webserver itself are mapped to files below the web root path.</p>"
    "<p>Example: <code>&lt;img src=\"/icons/smiley.png\"&gt;</code> → file <code>/sd/icons/smiley.png</code>"
    " (if root path is <code>/sd</code>)</p>");
  c.input_text("Root path", "docroot", docroot.c_str(), "Default: /sd");
  c.input_checkbox("Enable directory listings", "enable_dirlist", enable_dirlist);

  c.input_checkbox("Enable global file auth", "auth_global", auth_global,
    "<p>If enabled, file access is globally protected by the admin password (if set).</p>"
    "<p>Disabling allows public access to directories without an auth file.</p>"
    "<p>To protect a directory, you can e.g. copy the default auth file:"
    " <code class=\"autoselect\">vfs cp /store/.htpasswd /sd/…/.htpasswd</code></p>");
  c.input_text("Directory auth file", "auth_file", auth_file.c_str(), "Default: .htpasswd",
    "<p>Note: sub directories do <u>not</u> inherit the parent auth file.</p>");
  c.input_text("Auth domain/realm", "auth_domain", auth_domain.c_str(), "Default: ovms");

  c.fieldset_end();

  c.fieldset_start("WebSocket Connection");

  c.input("number", "TX job queue size", "ws_txqueuesize", ws_txqueuesize.c_str(), "10-250, default: 50",
    "<p>The queue buffers metrics updates, events, logs, streaming data etc. to be sent to a connected "
    "browser / websocket client, each client has a separate queue.</p>"
    "<p>If you have many and frequent updates or a slow client connection, you can try raising "
    "the queue size to avoid dropped updates due to job queue overflows.</p>"
    "<p>Note: changes only affect new connections (reload browser window to reconnect).</p>",
    "min=\"10\" max=\"250\" step=\"10\"");

  c.fieldset_end();

  c.fieldset_start("Encryption");

  c.printf(
    "<div class=\"form-group\">\n"
      "<label class=\"control-label col-sm-3\" for=\"input-content\">TLS certificate:</label>\n"
      "<div class=\"col-sm-9\">\n"
        "<textarea class=\"form-control font-monospace\" style=\"font-size:80%%;white-space:pre;\"\n"
          "autocapitalize=\"none\" autocorrect=\"off\" autocomplete=\"off\" spellcheck=\"false\"\n"
          "placeholder=\"-----BEGIN CERTIFICATE-----&#13;&#10;...&#13;&#10;-----END CERTIFICATE-----\"\n"
          "rows=\"5\" id=\"input-tls_cert\" name=\"tls_cert\">%s</textarea>\n"
      "</div>\n"
    "</div>\n"
    , c.encode_html(tls_cert).c_str());
  c.printf(
    "<div class=\"form-group\">\n"
      "<label class=\"control-label col-sm-3\" for=\"input-content\">TLS private key:</label>\n"
      "<div class=\"col-sm-9\">\n"
        "<textarea class=\"form-control font-monospace\" style=\"font-size:80%%;white-space:pre;\"\n"
          "autocapitalize=\"none\" autocorrect=\"off\" autocomplete=\"off\" spellcheck=\"false\"\n"
          "placeholder=\"-----BEGIN PRIVATE KEY-----&#13;&#10;...&#13;&#10;-----END PRIVATE KEY-----\"\n"
          "rows=\"5\" id=\"input-tls_key\" name=\"tls_key\">%s</textarea>\n"
      "</div>\n"
    "</div>\n"
    , c.encode_html(tls_key).c_str());

  c.fieldset_end();

  c.input_button("default", "Save");
  c.form_end();
  c.panel_end(
    "<p>To enable encryption (https, wss) you need to install a TLS certificate + key. Public"
    " certification authorities (CAs) won't issue certificates for private hosts and IP addresses,"
    " so we recommend to create a self-signed TLS certificate for your module. Use a maximum key"
    " size of 2048 bit for acceptable performance.</p>"
    "<p><u>Example/template using OpenSSL</u>:</p>"
    "<samp class=\"autoselect\">openssl req -x509 -newkey rsa:2048 -sha256 -days 3650 -nodes \\\n"
    "&nbsp;&nbsp;-keyout ovms.key -out ovms.crt -subj \"/CN=ovmsname.local\" \\\n"
    "&nbsp;&nbsp;-addext \"subjectAltName=IP:192.168.4.1,IP:192.168.2.101\"</samp>"
    "<p>Change the name and add more IPs as needed. The command produces two files in your current"
    " directory, <code>ovms.crt</code> and <code>ovms.key</code>. Copy their contents into the"
    " respective fields above.</p>"
    "<p><u>Note</u>: as this is a self-signed certificate, you will need to explicitly allow the"
    " browser to access the module on the first https connect.</p>");
  c.done();
}
