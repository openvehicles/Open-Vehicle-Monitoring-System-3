#!/usr/bin/env python3
"""Log OVMS MQTT metrics to a TSV sidecar during a CAN capture.

Companion to tests/capture.sh: subscribes to the vehicle's metric topics on the
configured broker and appends `epoch<TAB>retained<TAB>metric<TAB>value` rows
until killed (SIGTERM/SIGINT). Retained rows (the broker snapshot delivered on
subscribe) are flagged `1` so analysis can separate snapshot from live updates.

GPS/location metrics are dropped by default — candumps/ is committed and the
repo carries no PII. Pass --gps to keep them (local-only analysis).

Server facts (broker, user, topic, password source) live in mqtt_log.json next
to this script — gitignored, this repo is public. Copy mqtt_log.json.example
and fill in. Password: OVMS_MQTT_PW env var, else the config's password_cmd is
run and its stdout used — plug in any secret store (pass, op, secret-tool, ...).
The value never appears in argv or in the repo.

Usage:
  mqtt_log.py OUTFILE [--gps]
"""

import json
import os
import signal
import ssl
import subprocess
import sys
import time

import paho.mqtt.client as mqtt

CONF_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "mqtt_log.json")
PII_SUBSTRINGS = ("/p/latitude", "/p/longitude", "/p/location")


def get_password(conf: dict) -> str:
    pw = os.environ.get("OVMS_MQTT_PW")
    if pw:
        return pw
    return subprocess.check_output(conf["password_cmd"], shell=True).decode().strip()


def main() -> int:
    args = [a for a in sys.argv[1:] if a != "--gps"]
    keep_gps = "--gps" in sys.argv[1:]
    if len(args) != 1:
        print(__doc__, file=sys.stderr)
        return 2
    outfile = args[0]

    try:
        with open(CONF_PATH) as f:
            conf = json.load(f)
    except Exception as e:
        print(f"mqtt_log: no config ({e}) — copy mqtt_log.json.example, see docstring",
              file=sys.stderr)
        return 1

    try:
        password = get_password(conf)
    except Exception as e:
        print(f"mqtt_log: no broker password ({e}) — metrics logging disabled", file=sys.stderr)
        return 1

    topic = conf["topic"]  # e.g. "ovms/<user>/<vehicleid>/metric/#"
    out = open(outfile, "a", buffering=1)
    prefix_len = len(topic) - 1  # strip everything up to ".../metric/"

    def on_message(client, userdata, msg):
        metric = msg.topic[prefix_len:].replace("/", ".")
        if not keep_gps and any(s in msg.topic for s in PII_SUBSTRINGS):
            return
        value = msg.payload.decode("utf-8", "replace").replace("\t", " ").replace("\n", " ")
        out.write(f"{time.time():.3f}\t{int(msg.retain)}\t{metric}\t{value}\n")

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(conf["user"], password)
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
    client.on_message = on_message
    client.on_connect = lambda c, u, f, reason, prop=None: (
        c.subscribe(topic)
        if str(reason) == "Success"
        else print(f"mqtt_log: connect failed: {reason}", file=sys.stderr)
    )

    try:
        client.connect(conf["broker"], conf.get("port", 8883), keepalive=30)
    except Exception as e:
        print(f"mqtt_log: cannot reach broker ({e}) — metrics logging disabled", file=sys.stderr)
        return 1

    stop = {"flag": False}

    def handle_sig(signum, frame):
        stop["flag"] = True

    signal.signal(signal.SIGTERM, handle_sig)
    signal.signal(signal.SIGINT, handle_sig)

    client.loop_start()
    while not stop["flag"]:
        time.sleep(0.2)
    client.loop_stop()
    client.disconnect()
    out.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
