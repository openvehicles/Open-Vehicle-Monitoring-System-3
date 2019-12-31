# OVMS3 Plugin: Edimax

**Smart Plug control for Edimax models SP-1101W, SP-2101W et al**

Version 1.0 by Michael Balzer <dexter@dexters-web.de>

Note: may need `HTTP.request()` digest auth support to work with newer Edimax firmware (untested)

The smart plug can be bound to a defined location. Automatic periodic recharging or charge stop can
be configured via SOC levels.

## Installation

- Save `edimax.js` as `/store/scripts/lib/edimax.js`
- Add line to `/store/scripts/ovmsmain.js`:
  - `edimax = require("lib/edimax");`
- Issue `script reload`
- Install `edimax.htm` web plugin, recommended setup:
  - Page:    `/usr/edimax`
  - Label:   Edimax Smart Plug
  - Menu:    Config
  - Auth:    Cookie

## Configuration

Use the web frontend for simple configuration.

    Param   Instance            Description
    usr     edimax.ip           Edimax IP address
    usr     edimax.user         … username, "admin" by default
    usr     edimax.pass         … password, "1234" by default
    usr     edimax.location     optional: restrict auto switch to this location
    usr     edimax.soc_on       optional: switch on if SOC at/below
    usr     edimax.soc_off      optional: switch off if SOC at/above

## Usage

    script eval edimax.get()
    script eval edimax.set("on" | "off")
    script eval edimax.info()

Note: `get()` & `set()` do an async update (if the location matches), the result is logged.
Use `info()` to show the current state.

## Events

    usr.edimax.on
    usr.edimax.off
    usr.edimax.error

