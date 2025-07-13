//
// GPIO Map for Lilygo T-Call A7670E V 1.0 on rev A motherboard
//

#define VSPI_PIN_MISO             19
#define VSPI_PIN_MOSI             23
#define VSPI_PIN_CLK              18

// GPIO pin mapping for Lilygo T-Call A7670 V1.0 and V1.1 (modem interface)
//      Function   |   V1.0   |   V1.1
//      -----------+----------+----------
//      Modem TX   |   26     |   27
//      Modem RX   |   25     |   26
//      Modem DTR  |   14     |   32
//      Modem RING |   13     |   33
//      Modem PWR  |    4     |    4
//      Modem RESET|   27     |    5 

#define ESP32CAN_PIN_TX           32
#define ESP32CAN_PIN_RX           34
#define ESP32CAN_PIN_RS           33       // RS pin of CAN transceiver 

#define MODEM_GPIO_RX             25    
#define MODEM_GPIO_TX             26   
#define MODEM_GPIO_DTR            14   
#define MODEM_GPIO_RING           13    // NOT USED
#define MODEM_GPIO_RESET          27    //  NOT YET USED (active low)

#define MODEM_GPIO_PWR             4    // switch POWERKEY of Modem (active high)
#define VSPI_PIN_MCP2515_1_CS     22
#define VSPI_PIN_MCP2515_1_INT    35
//    #define ADC_CHANNEL_12V            0    // Measure OBD 12V - GPIO36/SENSOR_VP
#define MODEM_EGPIO_PWR           MODEM_GPIO_PWR // avoid problem with modem intialization
#define MODEM_EGPIO_DTR           MODEM_GPIO_DTR
#define VSPI_PIN_MCP2515_2_INT    -1    // dummy definition to avoid compiler error
#define VSPI_PIN_MCP2515_2_CS     -1
