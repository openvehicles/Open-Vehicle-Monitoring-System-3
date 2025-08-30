GPIO Definitions for alternative OVMS Hardware
==============================================
The default GPIO assignments are defined in `ovm_peripherals.h`.
By switching to an alternative GPIO mapping via `make menuconfig -> Component config -> Open Vehicle Monitoring System -> Hardware Support -> Choose GPIO Mapping`, non default hardware can utilize the OVMS firmware.

Requirements
------------
- the hardware components are compatible with the default OVMS module, e.g. ESP32, Simcom modem, ESP32Can and MCP2515 based CAN bus
- the names of the pins are consistent with the firmware (see below for details)
- all hardware components need to be selected via `make menuconfig`

GPIO Names and Functions
------------------------

| Name                   | Function                                               |
| ---------------------- | ------------------------------------------------------ |
| VSPI_PIN_MISO          |   SPI input pin of ESP32 (master) - connects to MCP2515 chips  |
| VSPI_PIN_MOSI          |   SPI output pin of ESP32 (master)                              |
| VSPI_PIN_CLK           |   SPI clock pin of ESP32                               |
| VSPI_PIN_MCP2515_1_CS  |   chip select of first MCP2515                         |
| VSPI_PIN_MCP2515_1_INT |   interrupt of first MCP2515                           | 
| VSPI_PIN_MCP2515_2_CS  |   chip select of second MCP2515                        |
| VSPI_PIN_MCP2515_2_INT |   interrupt of second MCP2515                          | 
| ESP32CAN_PIN_TX        |   transmit pin of ESP32 built-in CAN bus               |
| ESP32CAN_PIN_RX        |   receive pin of ESP32 built-in CAN bus                |
| MAX7317_CAN1_EN        |   RS pin of CAN transceiver of CAN 1 via GPIO extender |
| ESP32CAN_PIN_RS        |   RS pin of CAN transceiver of ESP32 built-in CAN bus (non standard Hardware) |
| VSPI_PIN_MCP2515_SWCAN_CS | chip select of third MCP2515 vio GPIO extender     |
| VSPI_PIN_MCP2515_SWCAN_INT | interrupt of third MCP2515 via GPIO extender      |
| MODEM_GPIO_RX          |   UART RX pin connected to modem                       |
| MODEM_GPIO_TX          |   UART TX pin connected to modem                       |
| MODEM_GPIO_DTR         |   UART DTR pin connected to modem                      |
| MODEM_EGPIO_DTR        |   UART DTR pin connected to modem via GPIO extender    |
| MAX7317_MDM_DTR        |   UART DTR pin connected to modem via GPIO extender    |
| MODEM_GPIO_RING        |   UART RING pin connected to modem                     |
| MODEM_GPIO_RESET       |   modem reset pin                                      |
| MODEM_GPIO_PWR         |   modem power (inverted logic)   |
| MODEM_EGPIO_PWR         |  modem power  (inverted logic) via  GPIO extender   |
| MAX7317_MDM_EN         |  modem power  (inverted logic) via  GPIO extender (same as EGPIO_PWR)    |
| SDCARD_PIN_CLK         | SDMMC clock pin |
| SDCARD_PIN_CMD         | SDMMC CMD pin |
| SDCARD_PIN_D0          | SDMMC data 0 pin |
| SDCARD_PIN_CD          | SDMMC Card detect  pin |
| MAX7317_SW_CTL         | EXT_12V power switch |
| MAX7317_EGPIO_1        | GPIO extender pin  |
| MAX7317_EGPIO_2        | GPIO extender pin  |
| MAX7317_EGPIO_3        | GPIO extender pin  |
| MAX7317_EGPIO_4        | GPIO extender pin  |
| MAX7317_EGPIO_5        | GPIO extender pin  |
| MAX7317_EGPIO_6        | GPIO extender pin  |
| MAX7317_EGPIO_7        | GPIO extender pin  |
| MAX7317_EGPIO_8        | GPIO extender pin  |
| MAX7317_SWCAN_MODE0    | Fourth CAN bus pin |
| MAX7317_SWCAN_MODE1    | Fourth CAN bus pin |
| MAX7317_SWCAN_STATUS_LED | Fourth CAN bus status LED |
| MAX7317_SWCAN_TX_LED   | Fourth CAN bus TX LED |
| MAX7317_SWCAN_RX_LED   | Fourth CAN bus RX LED |


Some Remarks
------------

- The RS pin of the CAN transceiver SN65HVD233 or similar, allows to switch between a listen only mode (high level) and standard mode (low level)
- The RS pin is only directly connected to a GPIO for the built-in ESP32CAN bus.
- The RS pin of the other CAN buses is driven by the IO pins of the MCP2515 CAN bus controller (via SPI commands)
- All names containing `MAX7317` or `EGPIO` are only required, if the GPIO extender chip MAX7317 is present and activated via `menuconfig` 
  - Exception: `MODEM_EGPIO_PWR` and `MODEM_EGPIO_DTR` have to be set even if NO GPIO extender is present. Set to the same value as `MODEM_GPIO_PWR` and `MODEM_GPIO_DTR` respectively. 
- The modem power pin: Simcom recommends to utilize an open collector driver (NPN transistor) for the PWR pin. This is present in the default OVMS module. The logic is therefore inverted in the Simcom modem drivers.
- The Chip Select and Interrupt pins have to be defined for CAN bus 1-3, even, if not all CAN buses are present. Not existing signals can be set `-1` e.g. `#define VSPI_PIN_MCP2515_2_INT  -1`. This avoids compilation problems, but will cause an error message due to an invalid GPIO pin number.

How to create a GPIO Mapping file
---------------------------------
- Create a file in `components/gpio_maps` e.g `mygpios.h`
- Assign the GPIO pins to the corresponding name via `#define` statements. 
- Enter the new file name via `menuconfig` as the GPIO map. If a different directory is chosen for the location of the map, the full path has to be specified.

Example snippet
---------------

````
#define VSPI_PIN_MISO             19
#define VSPI_PIN_MOSI             23
#define VSPI_PIN_CLK              18

````

Be aware, that the numbers are the GPIO numbers and NOT the ESP32 pin numbers.


