/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
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
;
; Portions of this are based on the work of Thomas Barth, and licensed
; under MIT license.
; Copyright (c) 2017, Thomas Barth, barth-dev.de
; https://github.com/ThomasBarth/ESP32-CAN-Driver
*/

#include "ovms_log.h"
static const char *TAG = "esp32can";

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include <string.h>
#include "esp32can.h"
#include "esp32can_regdef.h"
#include "ovms_peripherals.h"
#include "ovms_module.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION_MAJOR >= 5
#include <esp_chip_info.h>
#endif
#if ESP_IDF_VERSION_MAJOR >= 4
#include <rom/gpio.h>
#include <soc/gpio_sig_map.h>
#endif


static esp32can* MyESP32can = NULL;

static portMUX_TYPE esp32can_spinlock = portMUX_INITIALIZER_UNLOCKED;
#define ESP32CAN_ENTER_CRITICAL()       portENTER_CRITICAL(&esp32can_spinlock)
#define ESP32CAN_EXIT_CRITICAL()        portEXIT_CRITICAL(&esp32can_spinlock)
#define ESP32CAN_ENTER_CRITICAL_ISR()   portENTER_CRITICAL_ISR(&esp32can_spinlock)
#define ESP32CAN_EXIT_CRITICAL_ISR()    portEXIT_CRITICAL_ISR(&esp32can_spinlock)

static inline uint32_t ESP32CAN_rxframe(esp32can *me, BaseType_t* task_woken)
  {
  static CAN_queue_msg_t msg;
  uint32_t error_irqs = 0;

  // The ESP32 CAN controller works different from the SJA1000 here.
  // 
  // https://github.com/espressif/esp-idf/issues/4276:
  // 
  //  "When bytes are received, they are written to the FIFO directly,
  //   and an overflow is not detected until the 64th byte is written.
  //   The bytes of the overflowing message will remain in the FIFO.
  //   The RMC should count all messages received (up to 64 messages)
  //   regardless of whether they were overflowing or not."
  // 
  //  "Basically, what should happen is that whenever you release the
  //   receiver buffer, and the buffer window shifts to an overflowed
  //   message, the Data overrun interrupt will be set. If that is the
  //   case, the message contents should be ignored, the clear data
  //   overrun command set, and the receiver buffer released again.
  //   Continue this process until RMC reaches zero, or until the
  //   buffer window rotates to a valid message."
  // 
  // Results from single step tests:
  // 
  //  - The interrupt flags are set with a delay after RRB / CDO
  //    → the RX loop must not use them (but can rely on the status flags)
  // 
  //  - At DOS, RMC tells us how many frames need to be discarded
  //    → on DOS, issuing CDO, then RMC times RRB resyncs the RX to the next valid frame
  // 
  //  - DOS can become set on the last RRB (i.e. without any more message
  //   in the FIFO, RMC=0 and no RI/RBS)
  //    → the RX loop must check & handle DOS independent of the other indicators
  // 
  //  - After an overflow of the receive message counter RMC (at 64), the controller
  //    cannot recover and continues to deliver wrong & corrupted frames, even if
  //    clearing the FIFO completely until RMC=0
  //    → if RMC reaches 64, the controller must be reset

  while (MODULE_ESP32CAN->SR.B.RBS | MODULE_ESP32CAN->SR.B.DOS)
    {
    if (MODULE_ESP32CAN->RMC.B.RMC == 64)
      {
      // RMC overflow => reset controller:
      MODULE_ESP32CAN->MOD.B.RM = 1;
      MODULE_ESP32CAN->MOD.B.RM = 0;
      error_irqs |= __CAN_IRQ_DATA_OVERRUN;
      me->m_status.error_resets++;
      }
    else if (MODULE_ESP32CAN->SR.B.DOS)
      {
      // FIFO overflow => clear overflow & discard <RMC> messages to resync:
      error_irqs |= __CAN_IRQ_DATA_OVERRUN;
      MODULE_ESP32CAN->CMR.B.CDO = 1;
      int8_t discard = MODULE_ESP32CAN->RMC.B.RMC;
      while (discard--)
        {
        MODULE_ESP32CAN->CMR.B.RRB = 1;
        me->m_status.rxbuf_overflow++;
        }
      }
    else
      {
      // Valid frame in receive buffer: record the origin
      memset(&msg,0,sizeof(msg));
      msg.type = CAN_frame;
      msg.body.frame.origin = me;

      // get FIR
      msg.body.frame.FIR.U = MODULE_ESP32CAN->MBX_CTRL.FCTRL.FIR.U;

      // Detect invalid frames
      if (msg.body.frame.FIR.B.DLC > sizeof(msg.body.frame.data.u8))
        {
        error_irqs |= __CAN_IRQ_INVALID_RX;
        me->m_status.invalid_rx++;

        // Request next frame:
        MODULE_ESP32CAN->CMR.B.RRB = 1;
        continue;
        }

      // check if this is a standard or extended CAN frame
      if (msg.body.frame.FIR.B.FF == CAN_frame_std)
        {
        // Standard frame: Get Message ID
        msg.body.frame.MsgID = ESP32CAN_GET_STD_ID;
        // …deep copy data bytes
        for (int k=0 ; k<msg.body.frame.FIR.B.DLC ; k++)
          msg.body.frame.data.u8[k] = MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.STD.data[k];
        }
      else
        {
        // Extended frame: Get Message ID
        msg.body.frame.MsgID = ESP32CAN_GET_EXT_ID;
        // …deep copy data bytes
        for (int k=0 ; k<msg.body.frame.FIR.B.DLC ; k++)
          msg.body.frame.data.u8[k] = MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.EXT.data[k];
        }

      // Request next frame:
      MODULE_ESP32CAN->CMR.B.RRB = 1;

      // Send frame to CAN framework:
      xQueueSendFromISR(MyCan.m_rxqueue, &msg, task_woken);
      }

    } // while (MODULE_ESP32CAN->SR.B.RBS | MODULE_ESP32CAN->SR.B.DOS)

  return error_irqs;
  }

static IRAM_ATTR void ESP32CAN_isr(void *pvParameters)
  {
  esp32can *me = (esp32can*)pvParameters;
  BaseType_t task_woken = pdFALSE;
  uint32_t interrupt;
  uint32_t error_irqs = 0;

  ESP32CAN_ENTER_CRITICAL_ISR();

  // Read interrupt status and clear flags
  while ((interrupt = MODULE_ESP32CAN->IR.U & 0xff) != 0)
    {
    me->m_status.interrupts++;

    // Errata workaround: TWAI_ERRATA_FIX_BUS_OFF_REC
    //
    // Add SW workaround for REC change during bus-off
    //
    // When the bus-off condition is reached, the REC should be
    // reset to 0 and frozen (via LOM) by the driver's ISR. However
    // on the ESP32, there is an edge case where the REC will
    // increase before the driver's ISR can respond in time (e.g.,
    // due to the rapid occurrence of bus errors), thus causing the
    // REC to be non-zero after bus-off. A non-zero REC can prevent
    // bus-off recovery as the bus-off recovery condition is that
    // both TEC and REC become Enabling this option will add a
    // workaround in the driver to forcibly reset REC to zero on
    // reaching bus-off.

    // "Force REC to 0 by re-triggering bus-off (by setting TEC to 0 then 255)"
    if ((interrupt & __CAN_IRQ_ERR_WARNING) != 0)
      {
      uint32_t status = MODULE_ESP32CAN->SR.U;
      if ((status & __CAN_STS_BUS_OFF) != 0 && (status & __CAN_STS_ERR_WARNING) != 0)
        {
        // Freeze TEC/REC by entering listen only mode
        MODULE_ESP32CAN->MOD.B.LOM = 1;

        // Re-trigger bus-off
        MODULE_ESP32CAN->TXERR.B.TXERR = 0;
        MODULE_ESP32CAN->TXERR.B.TXERR = 0xff;

        // Clear the re-triggered bus-off interrupt, collect any new bits
        interrupt |= MODULE_ESP32CAN->IR.U & 0xff;

        // Exit reset mode
        MODULE_ESP32CAN->MOD.B.RM = 0;
        }

      if ((status & __CAN_STS_BUS_OFF) == 0 &&
          (status & __CAN_STS_ERR_WARNING) != 0)
        {
        // Entering bus off halts in progress tx
        MyESP32can->m_state &= ~CAN_M_STATE_TX_BUF_OCCUPIED;
        }
      }

    // Handle RX frame(s) available & FIFO overflow interrupts:
    if ((interrupt & (__CAN_IRQ_RX|__CAN_IRQ_DATA_OVERRUN)) != 0)
      {
      interrupt |= ESP32CAN_rxframe(me, &task_woken);
      }

    //
    // Errata workaround: TWAI_ERRATA_FIX_TX_INTR_LOST
    //
    // Add SW workaround for TX interrupt lost
    //
    // On the ESP32, when a transmit interrupt occurs, and interrupt
    // register is read on the same APB clock cycle, the transmit
    // interrupt could be lost. Enabling this option will add a
    // workaround that checks the transmit buffer status bit to
    // recover any lost transmit interrupt.

    // Handle TX interrupt:
    if ((interrupt & __CAN_IRQ_TX) != 0 ||
        ((MyESP32can->m_state & CAN_M_STATE_TX_BUF_OCCUPIED) != 0 &&
        (MODULE_ESP32CAN->SR.U & __CAN_STS_TXDONE) != 0))
      {
      MyESP32can->m_state &= ~CAN_M_STATE_TX_BUF_OCCUPIED;
      CAN_queue_msg_t msg;
      // The TX interrupt occurs when the TX buffer becomes available, which may be due
      //  to transmission success or abortion. A real SJA1000 would tell the actual result
      //  by the SR.3 TCS bit, but the ESP32CAN also sets TCS on aborts. So there is no
      //  way to tell if the frame was really aborted, we can only rely on our own abort
      //  request status:
      if (me->m_tx_abort)
        {
        // Clear abort command:
        MODULE_ESP32CAN->CMR.B.AT = 0;
        me->m_tx_abort = false;
        msg.type = CAN_txfailedcallback;
        }
      else
        {
        msg.type = CAN_txcallback;
        }
      msg.body.frame = me->m_tx_frame;
      msg.body.bus = me;
      xQueueSendFromISR(MyCan.m_rxqueue, &msg, &task_woken);
      }

    // Collect error interrupts:
    error_irqs |= interrupt &
        (__CAN_IRQ_ERR_WARNING    // IR.2 Error Interrupt (warning state change)
        |__CAN_IRQ_DATA_OVERRUN	  // IR.3 Data Overrun Interrupt
        |__CAN_IRQ_ERR_PASSIVE    // IR.5 Error Passive Interrupt (passive state change)
        |__CAN_IRQ_BUS_ERR        // IR.7 Bus Error Interrupt
        |__CAN_IRQ_INVALID_RX     // Invalid RX Frame (synthetic)
        );

    // Handle wakeup interrupt:
    if ((interrupt & __CAN_IRQ_WAKEUP) != 0)
      {
      // Todo
      }
    } // while interrupt

  // Get error counters:
  uint32_t rxerr = MODULE_ESP32CAN->RXERR.U;
  uint32_t txerr = MODULE_ESP32CAN->TXERR.U;
  uint32_t status = MODULE_ESP32CAN->SR.U;
  if (status & __CAN_STS_BUS_OFF)
    {
    rxerr |= 0x100;
    txerr |= 0x100;
    }

  // Handle error interrupts:
  if (error_irqs)
    {
    uint32_t ecc = MODULE_ESP32CAN->ECC.U;
    uint32_t error_flags = error_irqs << 16 | (status & 0b11001110) << 8 | (ecc & 0xff);

    // Check for TX failure:
    //  We consider the TX to have failed if a bus error is detected during the
    //  transmission attempt and the controller gave up on retrying (= entered
    //  passive mode, txerr >= 128).
    if ((status & (__CAN_STS_TXDONE|__CAN_STS_TXFREE)) == 0 &&
        (error_irqs & __CAN_IRQ_BUS_ERR) != 0 &&
        (ecc & __CAN_ECC_DIR) == 0 &&
        txerr >= 128)
      {
      // Set abort command:
      me->m_tx_abort = true;
      MODULE_ESP32CAN->CMR.B.AT = 1;
      // Note: another TX IRQ will occur from the abort, see above
      }

    // Request error log?
    if (me->m_status.error_flags != error_flags ||
        me->m_status.errors_rx != rxerr ||
        me->m_status.errors_tx != txerr)
      {
      me->m_status.error_flags = error_flags;
      me->m_status.errors_rx = rxerr;
      me->m_status.errors_tx = txerr;
      me->m_status.error_time = monotonictime;
      // …only necessary if no abort has been issued:
      if (!me->m_tx_abort)
        {
        CAN_queue_msg_t msg;
        if (ecc != 0 || (status & (__CAN_STS_DATA_OVERRUN|__CAN_STS_BUS_OFF|__CAN_IRQ_INVALID_RX)) != 0)
          msg.type = CAN_logerror;
        else
          msg.type = CAN_logstatus;
        msg.body.bus = me;
        xQueueSendFromISR(MyCan.m_rxqueue, &msg, &task_woken);
        }
      }
    }
  else
    {
    // Update status:
    if (rxerr == 0 && txerr == 0)
      me->m_status.error_flags = 0;
    me->m_status.errors_rx = rxerr;
    me->m_status.errors_tx = txerr;
    }

  ESP32CAN_EXIT_CRITICAL_ISR();

  // Yield to minimize latency if we have woken up a higher priority task:
  if (task_woken == pdTRUE)
    {
    portYIELD_FROM_ISR();
    }
  }

static void ESP32CAN_init(void *pvParameters)
  {
  esp32can *me = (esp32can*) pvParameters;
  // Install CAN ISR:
  esp_intr_alloc(ETS_CAN_INTR_SOURCE, ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_LEVEL3, ESP32CAN_isr, me, NULL);
  vTaskDelete(NULL);
  }

esp32can::esp32can(const char* name, int txpin, int rxpin)
  : canbus(name)
  {
  m_txpin = (gpio_num_t)txpin;
  m_rxpin = (gpio_num_t)rxpin;
  MyESP32can = this;

  m_powermode = Off;
  m_tx_abort = false;

#if defined(CONFIG_OVMS_HW_REMAP_GPIO) && defined(ESP32CAN_PIN_RS)
  // Configure the RS PIN of the CAN transceiver
  gpio_set_direction((gpio_num_t)ESP32CAN_PIN_RS,GPIO_MODE_OUTPUT);
#endif // defined(CONFIG_OVMS_HW_REMAP_GPIO) && defined(ESP32CAN_PIN_RS)

  MyESP32can->SetTransceiverMode(CAN_MODE_LISTEN);

  // Enter controller reset mode:
  ESP_LOGD(TAG, "Reset mode state on init: %d", MODULE_ESP32CAN->MOD.B.RM);
  MODULE_ESP32CAN->MOD.B.RM = 1;

  // Launch ISR allocator task on core 0:
  TaskHandle_t task = NULL;
  xTaskCreatePinnedToCore(ESP32CAN_init, "esp32can init", 2048, (void*)this, 23, &task, CORE(0));
  AddTaskToMap(task);

  // Register esp32can specific commands:
  OvmsCommand* cmd_can = MyCommandApp.RegisterCommand("can", "CAN framework");
  OvmsCommand* cmd_canx = cmd_can->RegisterCommand(name, "CANx framework");
  OvmsCommand* cmd_setaf = cmd_canx->RegisterCommand("setaccfilter", "Set ESP32CAN (SJA1000) acceptance filter");
  cmd_setaf->RegisterCommand("single", "Single filter mode", shell_setaccfilter,
    "<mask> <code>\n"
    "Specify mask and code as 32 bit hexadecimal values.\n"
    "To disable all filters, set mask -1 and code 0.\n"
    "Bit layout: see SJA1000 specification, section 6.4.15 Acceptance Filter.",
    2, 2);
  cmd_setaf->RegisterCommand("dual", "Dual filter mode", shell_setaccfilter,
    "<mask> <code>\n"
    "Specify mask and code as 32 bit hexadecimal values.\n"
    "To disable all filters, set mask -1 and code 0.\n"
    "Bit layout: see SJA1000 specification, section 6.4.15 Acceptance Filter.",
    2, 2);
  }

esp32can::~esp32can()
  {
  MyESP32can = NULL;
  }

esp_err_t esp32can::InitController()
  {
  bool brp_div = 0;
  double __tq; // Time quantum

  // Set to PELICAN mode
  MODULE_ESP32CAN->CDR.B.CAN_M=0x1;

  // Synchronization jump width is the same for all baud rates
  MODULE_ESP32CAN->BTR0.B.SJW=0x1;

  // TSEG2 is the same for all baud rates
  MODULE_ESP32CAN->BTR1.B.TSEG2=0x1;

  // select time quantum and set TSEG1
  switch (MyESP32can->m_speed)
    {
    case CAN_SPEED_1000KBPS:
      MODULE_ESP32CAN->BTR1.B.TSEG1=0x4;
      __tq = 0.125;
      break;
    default:
      MODULE_ESP32CAN->BTR1.B.TSEG1=0xc;
      __tq = ((float)1000/static_cast<int>(MyESP32can->m_speed)) / 16;
    }

  // Set baud rate prescaler
  int brp = round((((APB_CLK_FREQ * __tq) / 2) - 1)/1000000)-1;
  esp_chip_info_t chip;
  esp_chip_info(&chip);
  if (brp > BRP_MAX)
    {
    /* ESP32 revision 2 and higher have a divide by 2 bit */
    if (chip.revision < 2)
      return ESP_FAIL;
    brp = brp / 2;
    brp_div = 1;
    if (brp > BRP_MAX)
      return ESP_FAIL;
    }
  MODULE_ESP32CAN->BTR0.B.BRP = (uint8_t)brp;

  /* Set sampling
   * 1 -> triple; the bus is sampled three times; recommended for low/medium speed buses     (class A and B) where filtering spikes on the bus line is beneficial
   * 0 -> single; the bus is sampled once; recommended for high speed buses (SAE class C)*/
  MODULE_ESP32CAN->BTR1.B.SAM=0x1;

  // Enable all interrupts except arbitration loss (can be ignored):
  uint32_t ier = 0xff & ~__CAN_IRQ_ARB_LOST;
  // Turn off BRP_DIV if we're V2 or higher (and don't want it set)
  if (chip.revision >= 2 && !brp_div)
      ier &= ~__CAN_IER_BRP_DIV;
  MODULE_ESP32CAN->IER.U = ier;

  // No acceptance filtering, as we want to fetch all messages
  MODULE_ESP32CAN->MBX_CTRL.ACC.CODE[0].B.ACR = 0;
  MODULE_ESP32CAN->MBX_CTRL.ACC.CODE[1].B.ACR = 0;
  MODULE_ESP32CAN->MBX_CTRL.ACC.CODE[2].B.ACR = 0;
  MODULE_ESP32CAN->MBX_CTRL.ACC.CODE[3].B.ACR = 0;
  MODULE_ESP32CAN->MBX_CTRL.ACC.MASK[0].B.AMR = 0xff;
  MODULE_ESP32CAN->MBX_CTRL.ACC.MASK[1].B.AMR = 0xff;
  MODULE_ESP32CAN->MBX_CTRL.ACC.MASK[2].B.AMR = 0xff;
  MODULE_ESP32CAN->MBX_CTRL.ACC.MASK[3].B.AMR = 0xff;

  // Set to normal mode
  MODULE_ESP32CAN->OCR.B.OCMODE=__CAN_OC_NOM;

  // Active/Listen Mode
  if (m_mode == CAN_MODE_LISTEN)
    MODULE_ESP32CAN->MOD.B.LOM = 1;
  else
    MODULE_ESP32CAN->MOD.B.LOM = 0;

  // Clear error counters
  MODULE_ESP32CAN->TXERR.U = 0;
  MODULE_ESP32CAN->RXERR.U = 0;
  (void)MODULE_ESP32CAN->ECC;

  // Clear interrupt flags
  (void)MODULE_ESP32CAN->IR.U;
  m_tx_abort = false;
  return ESP_OK;
  }

esp_err_t esp32can::Start(CAN_mode_t mode, CAN_speed_t speed)
  {
  // Check speed availability on current hardware:
  switch (speed)
    {
    case CAN_SPEED_33KBPS:
    case CAN_SPEED_50KBPS:
    case CAN_SPEED_83KBPS:
      esp_chip_info_t chip;
      esp_chip_info(&chip);
      if (chip.revision < 2)
        {
        ESP_LOGW(TAG, "%d only supported with ESP32 V2 and higher", speed);
        return ESP_FAIL;
        }
    default:
      break;
    }

  // Restarting an already started bus (e.g. for mode change)?
  if (m_mode != CAN_MODE_OFF)
    {
    Stop();
    }

  canbus::Start(mode, speed);

  m_mode = mode;
  m_speed = speed;

  // Enable module
  DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_CAN_CLK_EN);
  DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_CAN_RST);

  // Configure TX pin
  gpio_set_level(MyESP32can->m_txpin, 1);
  gpio_set_direction(MyESP32can->m_txpin,GPIO_MODE_OUTPUT);
  gpio_matrix_out(MyESP32can->m_txpin,CAN_TX_IDX,0,0);
  gpio_pad_select_gpio(MyESP32can->m_txpin);

  // Configure RX pin
  gpio_set_direction(MyESP32can->m_rxpin,GPIO_MODE_INPUT);
  gpio_matrix_in(MyESP32can->m_rxpin,CAN_RX_IDX,0);
  gpio_pad_select_gpio(MyESP32can->m_rxpin);

  ESP32CAN_ENTER_CRITICAL();

  esp_err_t err = InitController();

  // clear statistics:
  ClearStatus();

  // Showtime. Release Reset Mode.
  MODULE_ESP32CAN->MOD.B.RM = 0;

  ESP32CAN_EXIT_CRITICAL();

  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "Failed to set speed to %d", speed);
    return err;
    }

  MyESP32can->SetTransceiverMode(mode);

  // And record that we are powered on
  pcp::SetPowerMode(On);

  return ESP_OK;
  }

esp_err_t esp32can::Stop()
  {
  OvmsMutexLock lock(&m_write_mutex);

  canbus::Stop();

  // Clear TX queue
  xQueueReset(m_txqueue);

  MyESP32can->SetTransceiverMode(CAN_MODE_LISTEN);

  ESP32CAN_ENTER_CRITICAL();

  // Abort TX
  MODULE_ESP32CAN->CMR.B.TR = 0;
  MODULE_ESP32CAN->CMR.B.AT = 1;
  MODULE_ESP32CAN->CMR.B.AT = 0;

  // Enter reset mode
  MODULE_ESP32CAN->MOD.B.RM = 1;

  ESP32CAN_EXIT_CRITICAL();

  // And record that we are powered down
  pcp::SetPowerMode(Off);
  m_mode = CAN_MODE_OFF;

  return ESP_OK;
  }

/**
 * SetTransceiverMode: enable or disable write driver of transceiver
 * Pin RS of SN65 to 0 or 1 
 * 
 */
void esp32can::SetTransceiverMode(CAN_mode_t mode)
  {
  int rs_state = (mode == CAN_MODE_ACTIVE) ? 0 : 1;
  
#ifdef CONFIG_OVMS_COMP_MAX7317
  // Enable TX driver of matching SN65 transceiver
  MyPeripherals->m_max7317->Output(MAX7317_CAN1_EN, rs_state);
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317

#if defined(CONFIG_OVMS_HW_REMAP_GPIO) && defined(ESP32CAN_PIN_RS)
  // Enable TX driver of matching SN65 transceiver
  gpio_set_level((gpio_num_t)ESP32CAN_PIN_RS, rs_state);
#endif // defined(CONFIG_OVMS_HW_REMAP_GPIO) && defined(ESP32CAN_PIN_RS)
  }

/**
 * ChangeResetMode: enter/leave reset mode with verification & timeout
 * 
 * Note: this must be called within an ESP32CAN_ENTER_CRITICAL section.
 */
esp_err_t esp32can::ChangeResetMode(unsigned int newmode, int timeout_us /*=50*/)
  {
  MODULE_ESP32CAN->MOD.B.RM = newmode;
  while (MODULE_ESP32CAN->MOD.B.RM != newmode && timeout_us > 0)
    {
    MODULE_ESP32CAN->MOD.B.RM = newmode;
    ets_delay_us(1);
    timeout_us--;
    }
  if (MODULE_ESP32CAN->MOD.B.RM != newmode)
    {
    ESP_LOGE(TAG, "%s failed to %s reset mode", m_name, newmode ? "enter" : "leave");
    return ESP_FAIL;
    }
  return ESP_OK;
  }


/**
 * SetAcceptanceFilter: configure ESP32CAN specific hardware RX filter
 * 
 * Call this via casting the bus to esp32can or via MyPeripherals->m_esp32can.
 * 
 * See SJA1000 specification section 6.4.15 on how to define acceptance filters.
 * 
 * In single filter mode, code & mask define a single long filter, covering the
 * full 29 bit address + RTR bit of an extended frame or the 11 bit address +
 * RTR bit + the first two data bytes of a standard frame.
 * 
 * In dual filter mode, code & mask are split into two filters, with 20/12 bits
 * for standard frames, and 16/16 bits for extended frames.
 * See SJA1000 specification for the rather crooked bit layout in dual mode.
 * 
 * The esp32can_filter_config_t includes bit fields for simplified assembly, e.g.:
 *   cfg.code.bss.id = 0x19f;
 *   cfg.mask.bss.id = 0x7ff;
 *
 * Att: code bits become relevant by setting the corresponding mask bit to 0.
 */
esp_err_t esp32can::SetAcceptanceFilter(const esp32can_filter_config_t& cfg)
  {
  // Block TX in case of filter reconfiguration while started
  OvmsMutexLock lock(&m_write_mutex);

  ESP32CAN_ENTER_CRITICAL();

  // Enter reset mode
  bool prev_resetmode = MODULE_ESP32CAN->MOD.B.RM;
  if (!prev_resetmode && ChangeResetMode(1) != ESP_OK)
    {
    ESP32CAN_EXIT_CRITICAL();
    return ESP_FAIL;
    }

  // Set filter mode
  MODULE_ESP32CAN->MOD.B.AFM = (cfg.single_filter) ? 1 : 0;

  // Swap code and mask to match big endian registers
  uint32_t code_swapped = __builtin_bswap32(cfg.code.u32);
  uint32_t mask_swapped = __builtin_bswap32(cfg.mask.u32);
  for (int i = 0; i < 4; i++)
    {
    MODULE_ESP32CAN->MBX_CTRL.ACC.CODE[i].B.ACR = ((code_swapped >> (i * 8)) & 0xFF);
    MODULE_ESP32CAN->MBX_CTRL.ACC.MASK[i].B.AMR  = ((mask_swapped >> (i * 8)) & 0xFF);
    }

  // Exit reset mode
  if (!prev_resetmode && ChangeResetMode(0) != ESP_OK)
    {
    ESP32CAN_EXIT_CRITICAL();
    return ESP_FAIL;
    }

  ESP32CAN_EXIT_CRITICAL();
  return ESP_OK;
  }


/**
 * shell_setaccfilter: shell command wrapper for SetAcceptanceFilter()
 * 
 * Syntax: can <bus> setaccfilter <single|dual> <mask> <code>
 */
void esp32can::shell_setaccfilter(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetParent()->GetName();
  esp32can* sbus = (esp32can*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->printf("ERROR: cannot find CAN bus %s\n", bus);
    return;
    }

  esp32can_filter_config_t cfg = {};
  cfg.single_filter = (strcmp(cmd->GetName(), "single") == 0);
  cfg.mask.u32 = strtoul(argv[0], NULL, 16);
  cfg.code.u32 = strtoul(argv[1], NULL, 16);

  esp_err_t res = sbus->SetAcceptanceFilter(cfg);
  if (res == ESP_OK)
    {
    writer->printf("Acceptance filter for %s set to %s filter mode with mask %08X, code %08X.\n",
      bus, cmd->GetName(), cfg.mask.u32, cfg.code.u32);
    }
  else
    {
    writer->printf("ERROR: failed to configure acceptance filter for %s\n", bus);
    }
  }


/**
 * WriteFrame: deliver a frame to the hardware for transmission (driver internal)
 */
esp_err_t esp32can::WriteFrame(const CAN_frame_t* p_frame)
  {
  ESP32CAN_ENTER_CRITICAL();
  uint8_t __byte_i; // Byte iterator

  // Clear abort command:
  MODULE_ESP32CAN->CMR.B.AT = 0;

  // check if TX buffer is available:
  if (MODULE_ESP32CAN->SR.B.TBS == 0)
    {
    ESP32CAN_EXIT_CRITICAL();
    return ESP_FAIL;
    }

  // copy frame information record
  MODULE_ESP32CAN->MBX_CTRL.FCTRL.FIR.U=p_frame->FIR.U;

  if (p_frame->FIR.B.FF==CAN_frame_std)
    { // Standard frame
    // Write message ID
    ESP32CAN_SET_STD_ID(p_frame->MsgID);
    // Copy the frame data to the hardware
    for (__byte_i=0 ; __byte_i<p_frame->FIR.B.DLC ; __byte_i++)
      MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.STD.data[__byte_i]=p_frame->data.u8[__byte_i];
    }
  else
    { // Extended frame
    // Write message ID
    ESP32CAN_SET_EXT_ID(p_frame->MsgID);
    // Copy the frame data to the hardware
    for (__byte_i=0 ; __byte_i<p_frame->FIR.B.DLC ; __byte_i++)
      MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.EXT.data[__byte_i]=p_frame->data.u8[__byte_i];
    }

  // Transmit frame
  MODULE_ESP32CAN->CMR.B.TR=1;
  MyESP32can->m_state |= CAN_M_STATE_TX_BUF_OCCUPIED;

  ESP32CAN_EXIT_CRITICAL();
  return ESP_OK;
  }


/**
 * Write: transmit or queue a frame for transmission (API)
 */
esp_err_t esp32can::Write(const CAN_frame_t* p_frame, TickType_t maxqueuewait /*=0*/)
  {
  OvmsMutexLock lock(&m_write_mutex);

  if (m_powermode != On || m_mode != CAN_MODE_ACTIVE)
    {
    ESP_LOGW(TAG,"Cannot write %s when not in ACTIVE mode",m_name);
    return ESP_FAIL;
    }

  // if there are frames waiting in the TX queue, add the new one there as well:
  if (uxQueueMessagesWaiting(m_txqueue))
    {
    return QueueWrite(p_frame, maxqueuewait);
    }

  // try to deliver the frame to the hardware:
  if (WriteFrame(p_frame) != ESP_OK)
    {
    return QueueWrite(p_frame, maxqueuewait);
    }

  // stats & logging:
  canbus::Write(p_frame, maxqueuewait);

  return ESP_OK;
  }


void esp32can::TxCallback(CAN_frame_t* p_frame, bool success)
  {
  // Application callbacks & logging:
  canbus::TxCallback(p_frame, success);

  // TX buffer has become available; send next queued frame (if any):
    {
    OvmsMutexLock lock(&m_write_mutex);
    CAN_frame_t frame;
    while (xQueueReceive(m_txqueue, (void*)&frame, 0) == pdTRUE)
      {
      if (WriteFrame(&frame) == ESP_FAIL)
        {
        // This should not happen:
        ESP_LOGE(TAG, "TxCallback: fatal error: TX buffer not available");
        CAN_queue_msg_t msg;
        msg.type = CAN_txfailedcallback;
        msg.body.frame = frame;
        msg.body.bus = this;
        xQueueSend(MyCan.m_rxqueue, &msg, 0);
        }
      else
        {
        canbus::Write(&frame, 0);
        break;
        }
      }
    }
  }


void esp32can::BusTicker10(std::string event, void* data)
  {
  // Check for a stuck bus-off error state:
  // The workaround following TWAI_ERRATA_FIX_BUS_OFF_REC in ESP32CAN_isr() sometimes fails.
  // If this happens, the error IRQ settles with only IR.2 Error Interrupt (warning state change)
  // set. The controller fails to recover from this situation, we need to perform a full reset.
  if ((m_status.error_flags >> 16 == __CAN_IRQ_ERR_WARNING) &&
      (monotonictime - m_status.error_time >= 10))
    {
    ESP_LOGE(TAG, "%s stuck bus-off error state (errflags=0x%08" PRIx32 ") detected - resetting bus",
             m_name, m_status.error_flags);
    Reset();
    m_status.error_flags = 0;
    m_status.error_resets++;
    m_watchdog_timer = monotonictime;
    }

  // Pass on to standard handler:
  canbus::BusTicker10(event, data);
  }


void esp32can::SetPowerMode(PowerMode powermode)
  {
  pcp::SetPowerMode(powermode);
  switch (powermode)
    {
    case On:
      if (m_mode != CAN_MODE_OFF) Start(m_mode,m_speed);
      break;
    case Sleep:
    case DeepSleep:
    case Off:
      Stop();
      break;
    default:
      break;
    };
  }


/**
 * GetErrorFlagsDesc: decode error flags into human readable text
 */
bool esp32can::GetErrorFlagsDesc(std::string &buffer, uint32_t error_flags)
  {
  // error_flags = error_irqs << 16 | (status & 0b11001110) << 8 | (ecc & 0xff)
  uint16_t irqs  = (error_flags >> 16) & 0xffff;
  uint8_t status = (error_flags >> 8) & 0xff;
  uint8_t ecc    = error_flags & 0xff;

  std::ostringstream ss;

  // Note: some IRQ bits are normally masked out, decode assuming all can occur:
  if (irqs)
    {
    if (irqs & __CAN_IRQ_INVALID_RX)      ss << " | " << "IR.8 Invalid RX frame";
    if (irqs & __CAN_IRQ_BUS_ERR)         ss << " | " << "IR.7 Bus error";
    if (irqs & __CAN_IRQ_ARB_LOST)        ss << " | " << "IR.6 Arbitration lost";
    if (irqs & __CAN_IRQ_ERR_PASSIVE)     ss << " | " << "IR.5 Error-passive state";
    if (irqs & __CAN_IRQ_WAKEUP)          ss << " | " << "IR.4 Wakeup";
    if (irqs & __CAN_IRQ_DATA_OVERRUN)    ss << " | " << "IR.3 Data overrun";
    if (irqs & __CAN_IRQ_ERR_WARNING)     ss << " | " << "IR.2 Error-warning state";
    if (irqs & __CAN_IRQ_TX)              ss << " | " << "IR.1 TX buffer free";
    if (irqs & __CAN_IRQ_RX)              ss << " | " << "IR.0 RX buffer not empty";
    }
  
  if (status)
    {
    if (ss.tellp() > 0) ss << "\n";
    if (status & __CAN_STS_BUS_OFF)       ss << " | " << "SR.7 Bus-off state";
    if (status & __CAN_STS_ERR_WARNING)   ss << " | " << "SR.6 Error count >= 96";
    if (status & __CAN_STS_TXPEND)        ss << " | " << "SR.5 TX pending";
    if (status & __CAN_STS_RXPEND)        ss << " | " << "SR.4 RX pending";
    if (status & __CAN_STS_TXDONE)        ss << " | " << "SR.3 TX done";
    if (status & __CAN_STS_TXFREE)        ss << " | " << "SR.2 TX buffer free";
    if (status & __CAN_STS_DATA_OVERRUN)  ss << " | " << "SR.1 Data overrun";
    if (status & __CAN_STS_RXBUF)         ss << " | " << "SR.0 RX buffer not empty";
    }
  
  if (ecc)
    {
    if (ss.tellp() > 0) ss << "\n";
    switch ((ecc & __CAN_ECC_ERRC) >> 6)
      {
      case 0b00: ss << " | " << "ECC Bit error"; break;
      case 0b01: ss << " | " << "ECC Form error"; break;
      case 0b10: ss << " | " << "ECC Stuff error"; break;
      case 0b11: ss << " | " << "ECC Other error"; break;
      }
    ss
      << " in "
      << ((ecc & __CAN_ECC_DIR) ? "RX" : "TX")
      << ", segment ";
    switch (ecc & __CAN_ECC_SEGMENT)
      {
      case 0b00011: ss << "start of frame"; break;
      case 0b00010: ss << "ID.28 to ID.21"; break;
      case 0b00110: ss << "ID.20 to ID.18"; break;
      case 0b00100: ss << "bit SRTR"; break;
      case 0b00101: ss << "bit IDE"; break;
      case 0b00111: ss << "ID.17 to ID.13"; break;
      case 0b01111: ss << "ID.12 to ID.5"; break;
      case 0b01110: ss << "ID.4 to ID.0"; break;
      case 0b01100: ss << "bit RTR"; break;
      case 0b01101: ss << "reserved bit 1"; break;
      case 0b01001: ss << "reserved bit 0"; break;
      case 0b01011: ss << "data length code"; break;
      case 0b01010: ss << "data field"; break;
      case 0b01000: ss << "CRC sequence"; break;
      case 0b11000: ss << "CRC delimiter"; break;
      case 0b11001: ss << "acknowledge slot"; break;
      case 0b11011: ss << "acknowledge delimiter"; break;
      case 0b11010: ss << "end of frame"; break;
      case 0b10010: ss << "intermission"; break;
      case 0b10001: ss << "active error flag"; break;
      case 0b10110: ss << "passive error flag"; break;
      case 0b10011: ss << "tolerate dominant bits"; break;
      case 0b10111: ss << "error delimiter"; break;
      case 0b11100: ss << "overload flag"; break;
      default: ss << (int)(ecc & __CAN_ECC_SEGMENT);
      }
    }
  
  buffer = ss.str();
  return true;
  }
