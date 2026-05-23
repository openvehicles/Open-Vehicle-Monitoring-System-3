/*
;    Project:       Open Vehicle Monitor System
;    Subject:       Support for Wireguard VPN protocol (client)
;    (c)            Ludovic LANGE
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

#include "ovms_log.h"
static const char *TAG = "wireguard";
#include "ovms_command.h"
#include "ovms_config.h"
#include "ovms_events.h"
#include "ovms_wireguard.h"

#define MY_WIREGUARD_CLIENT_INIT_PRIORITY 8900

OvmsWireguardClient MyWireguardClient
    __attribute__((init_priority(MY_WIREGUARD_CLIENT_INIT_PRIORITY)));

static wireguard_config_t wg_config = ESP_WIREGUARD_CONFIG_DEFAULT();

#define WG_PARAM_NAME "network.wireguard"

// Private key of the WireGuard device
#define WG_PRIVATE_KEY "local_private_key"
// Local IP address of the WireGuard device.
#define WG_LOCAL_IP_ADDRESS "local_ip_address"
// Netmask of the local network the WireGuard device belongs to.
#define WG_LOCAL_IP_NETMASK "local_ip_netmask"
// Local port to listen.
#define WG_LOCAL_PORT "local_port"

// Public key of the remote peer.
#define WG_PEER_PUBLIC_KEY "peer_public_key"
// Address of the remote peer.
#define WG_PEER_ADDRESS "peer_endpoint"

// Port number of the remote peer.
#define WG_PEER_PORT "peer_port"

// Wireguard pre-shared symmetric key
#define WG_PRESHARED_KEY "preshared_key"

// A seconds interval, between 1 and 65535 inclusive, of how often to
// send an authenticated empty packet to the peer for the purpose of
// keeping a stateful firewall or NAT mapping valid persistently
#define WG_PERSISTENT_KEEPALIVE "persistent_keepalive"

static esp_err_t wireguard_setup(wireguard_ctx_t *ctx) {
  esp_err_t err = ESP_FAIL;

  ESP_LOGI(TAG, "Initializing WireGuard.");

  if (wg_config.private_key) {
    free((void *)wg_config.private_key);
  }
  wg_config.private_key =
      strdup(MyConfig.GetParamValue(WG_PARAM_NAME, WG_PRIVATE_KEY).c_str());

  wg_config.listen_port =
      MyConfig.GetParamValueInt(WG_PARAM_NAME, WG_LOCAL_PORT);

  if (wg_config.public_key) {
    free((void *)wg_config.public_key);
  }
  wg_config.public_key =
      strdup(MyConfig.GetParamValue(WG_PARAM_NAME, WG_PEER_PUBLIC_KEY).c_str());

  if (wg_config.preshared_key) {
    free((void *)wg_config.preshared_key);
  }
  std::string ids =
      MyConfig.GetParamValue(WG_PARAM_NAME, WG_PRESHARED_KEY).c_str();
  if (ids != "") {
    wg_config.preshared_key = strdup(ids.c_str());
  } else {
    wg_config.preshared_key = NULL;
  }

  if (wg_config.allowed_ip) {
    free((void *)wg_config.allowed_ip);
  }
  wg_config.allowed_ip = strdup(
      MyConfig.GetParamValue(WG_PARAM_NAME, WG_LOCAL_IP_ADDRESS).c_str());

  if (wg_config.allowed_ip_mask) {
    free((void *)wg_config.allowed_ip_mask);
  }
  wg_config.allowed_ip_mask = strdup(
      MyConfig.GetParamValue(WG_PARAM_NAME, WG_LOCAL_IP_NETMASK).c_str());

  if (wg_config.endpoint) {
    free((void *)wg_config.endpoint);
  }
  wg_config.endpoint =
      strdup(MyConfig.GetParamValue(WG_PARAM_NAME, WG_PEER_ADDRESS).c_str());

  wg_config.port =
      MyConfig.GetParamValueInt(WG_PARAM_NAME, WG_PEER_PORT, 51820);
  wg_config.persistent_keepalive =
      MyConfig.GetParamValueInt(WG_PARAM_NAME, WG_PERSISTENT_KEEPALIVE);

  err = esp_wireguard_init(&wg_config, ctx);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_wireguard_init: %s", esp_err_to_name(err));
  }

  return err;
}

// COMMANDS
// --------

void wg_status(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc,
               const char *const *argv) {
  OvmsWireguardClient *me = &MyWireguardClient;
  if (me == NULL) {
    writer->puts("Error: WireguardClient could not be found");
    return;
  }

  writer->printf("Connection status:  %s\n",
                 (me->started) ? "started" : "stopped");

  if (me->started) {
    esp_err_t err;
    err = esp_wireguardif_peer_is_up(&me->ctx);
    writer->printf("Peer status:  %s\n", (err == ESP_OK) ? "up" : "down");
  }
}

void wg_start(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc,
              const char *const *argv) {
  writer->puts("Starting WireguardClient...");
  MyWireguardClient.TryStart();
}

void wg_stop(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc,
             const char *const *argv) {
  writer->puts("Stopping WireguardClient...");
  MyWireguardClient.TryStop();
}

void wg_restart(int verbosity, OvmsWriter *writer, OvmsCommand *cmd, int argc,
                const char *const *argv) {
  writer->puts("Restarting WireguardClient...");
  MyWireguardClient.TryStop();
  MyWireguardClient.TryStart();
}

void wg_set_default(int verbosity, OvmsWriter *writer, OvmsCommand *cmd,
                    int argc, const char *const *argv) {
  if (MyWireguardClient.started) {
    esp_err_t err;
    err = esp_wireguard_set_default(&MyWireguardClient.ctx);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_wireguard_set_default: %s", esp_err_to_name(err));
    }
  }
}

// EVENTS
// ------

void OvmsWireguardClient::EventSystemStart(std::string event, void *data) {
  this->UpdateSetup();
}

void OvmsWireguardClient::EventConfigChanged(std::string event, void *data) {
  OvmsConfigParam *p = (OvmsConfigParam *)data;

  if (p->GetName().compare(WG_PARAM_NAME) == 0) {
    bool was_started = this->started;
    this->TryStop();
    esp_err_t err;
    err = this->UpdateSetup();
    if ((err == ESP_OK) && (was_started)) {
      this->TryStart();
    }
  }
}

void OvmsWireguardClient::EventTicker(std::string event, void *data) {
  if (this->started) {
    esp_err_t err;
    err = esp_wireguardif_peer_is_up(&this->ctx);
    if (err == ESP_OK) {
      ESP_LOGD(TAG, "Peer is up");
    } else {
      ESP_LOGD(TAG, "Peer is down");
    }
  }
}

void OvmsWireguardClient::EventNetUp(std::string event, void *data) {
  ESP_LOGI(TAG, "Starting Wireguard client");
  this->TryStart();
}

void OvmsWireguardClient::EventNetDown(std::string event, void *data) {
  ESP_LOGI(TAG, "Stopping Wireguard client");
  this->TryStop();
}

void OvmsWireguardClient::EventNetReconfigured(std::string event, void *data) {
  esp_err_t err;
  ESP_LOGI(TAG, "Network was reconfigured: restarting Wireguard client");
  this->TryStop();
  this->TryStart();
}

// Constructor / destructor
OvmsWireguardClient::OvmsWireguardClient() {
  ESP_LOGI(TAG, "Initialising Wireguard Client (" STR(
                    MY_WIREGUARD_CLIENT_INIT_PRIORITY) ")");

  esp_log_level_set("esp_wireguard", ESP_LOG_DEBUG);
  esp_log_level_set("wireguardif", ESP_LOG_DEBUG);
  esp_log_level_set("wireguard", ESP_LOG_DEBUG);

  MyConfig.RegisterParam(WG_PARAM_NAME, "Wireguard (VPN) Configuration", true,
                         true);

  OvmsCommand *cmd_wg = MyCommandApp.RegisterCommand(
      "wireguard", "Wireguard framework", wg_status, "", 0, 0, false);
  cmd_wg->RegisterCommand("status", "Show wireguard status", wg_status, "", 0,
                          0, false);
  cmd_wg->RegisterCommand("start", "Start wireguard connexion", wg_start, "", 0,
                          0, false);
  cmd_wg->RegisterCommand("stop", "Stop wireguard connexion", wg_stop, "", 0, 0,
                          false);
  cmd_wg->RegisterCommand("restart", "Restart wireguard connexion", wg_restart,
                          "", 0, 0, false);
  cmd_wg->RegisterCommand("default", "Set interface as default route",
                          wg_set_default, "", 0, 0, false);

#undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(
      TAG, "system.start",
      std::bind(&OvmsWireguardClient::EventSystemStart, this, _1, _2));
  MyEvents.RegisterEvent(
      TAG, "config.changed",
      std::bind(&OvmsWireguardClient::EventConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(
      TAG, "ticker.10",
      std::bind(&OvmsWireguardClient::EventTicker, this, _1, _2));
  MyEvents.RegisterEvent(
      TAG, "network.up",
      std::bind(&OvmsWireguardClient::EventNetUp, this, _1, _2));
  MyEvents.RegisterEvent(
      TAG, "network.down",
      std::bind(&OvmsWireguardClient::EventNetDown, this, _1, _2));
  MyEvents.RegisterEvent(
      TAG, "network.reconfigured",
      std::bind(&OvmsWireguardClient::EventNetReconfigured, this, _1, _2));

  this->started = false;
}

OvmsWireguardClient::~OvmsWireguardClient() {}

// Internal methods
esp_err_t OvmsWireguardClient::UpdateSetup() {
  esp_err_t err;
  err = wireguard_setup(&this->ctx);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "wireguard_setup: %s", esp_err_to_name(err));
  }
  return err;
}

// Start only if config allows it
void OvmsWireguardClient::TryStart() {
  esp_err_t err;
  if (!(this->started)) {
    ESP_LOGI(TAG, "Connecting to the peer.");
    err = esp_wireguard_connect(&this->ctx);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_wireguard_connect: %s", esp_err_to_name(err));
    } else {
      this->started = true;
    }
  }
}

// Stop only if already started
void OvmsWireguardClient::TryStop() {
  if (this->started) {
    esp_wireguard_disconnect(&this->ctx);
    this->started = false;
  }
}
