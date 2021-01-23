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
*/

#include "ovms_log.h"
static const char *TAG = "mcp2515";

#include <string.h>
#include "mcp2515.h"
#include "mcp2515_regdef.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "esp_intr.h"
#include "soc/dport_reg.h"

static IRAM_ATTR void MCP2515_isr(void *pvParameters)
  {
  mcp2515 *me = (mcp2515*)pvParameters;
  BaseType_t task_woken = pdFALSE;

  me->m_status.interrupts++;

  // we don't know the IRQ source and querying by SPI is too slow for an ISR,
  // so we let AsynchronousInterruptHandler() figure out what to do.
  CAN_queue_msg_t msg = {};
  msg.type = CAN_asyncinterrupthandler;
  msg.body.bus = me;

  //send callback request to main CAN processor task
  xQueueSendFromISR(MyCan.m_rxqueue, &msg, &task_woken);

  // Yield to minimize latency if we have woken up a higher priority task:
  if (task_woken == pdTRUE)
    {
    portYIELD_FROM_ISR();
    }
  }

mcp2515::mcp2515(const char* name, spi* spibus, spi_nodma_host_device_t host, int clockspeed, int cspin, int intpin, bool hw_cs /*=true*/)
  : canbus(name)
  {
  m_spibus = spibus;
  m_host = host;
  m_clockspeed = clockspeed;
  m_cspin = cspin;
  m_intpin = intpin;

  memset(&m_devcfg, 0, sizeof(spi_nodma_device_interface_config_t));
  m_devcfg.clock_speed_hz=m_clockspeed;     // Clock speed (in hz)
  m_devcfg.mode=0;                          // SPI mode 0
  m_devcfg.command_bits=0;
  m_devcfg.address_bits=0;
  m_devcfg.dummy_bits=0;
  if (hw_cs)
    m_devcfg.spics_io_num=m_cspin;
  else
    {
    // use software CS for this mcp2515
    m_devcfg.spics_io_num=-1;
    m_devcfg.spics_ext_io_num=m_cspin;
    }
  m_devcfg.queue_size=7;                    // We want to be able to queue 7 transactions at a time

  esp_err_t ret = spi_nodma_bus_add_device(m_host, &m_spibus->m_buscfg, &m_devcfg, &m_spi);
  assert(ret==ESP_OK);

  gpio_set_intr_type((gpio_num_t)m_intpin, GPIO_INTR_NEGEDGE);
  gpio_isr_handler_add((gpio_num_t)m_intpin, MCP2515_isr, (void*)this);

  // Initialise in powered down mode
  m_powermode = Off; // Stop an event being raised
  SetPowerMode(Off);
  }

mcp2515::~mcp2515()
  {
  gpio_isr_handler_remove((gpio_num_t)m_intpin);
  spi_nodma_bus_remove_device(m_spi);
  }


esp_err_t mcp2515::WriteReg( uint8_t reg, uint8_t value )
  {
  uint8_t buf[16];
  m_spibus->spi_cmd(m_spi, buf, 0, 3, CMD_WRITE, reg, value);
  return ESP_OK;
  }

esp_err_t mcp2515::WriteRegAndVerify( uint8_t reg, uint8_t value, uint8_t read_back_mask )
  {
  uint8_t buf[16];
  uint8_t * rcvbuf;
  uint16_t timeout = 0;

  rcvbuf = m_spibus->spi_cmd(m_spi, buf, 1, 2, CMD_READ, reg);
  uint8_t origval = rcvbuf[0];
  ESP_LOGD(TAG, "%s: Set register (0x%02x val 0x%02x->0x%02x)", this->GetName(), reg, origval, value);

  WriteReg( reg, value );

  // verify that register is actually changed by reading it back.
  do
    {
    vTaskDelay(10 / portTICK_PERIOD_MS);

    rcvbuf = m_spibus->spi_cmd(m_spi, buf, 1, 2, CMD_READ, reg);
    rcvbuf[0] &= read_back_mask; // we check for consistency only these bits (we couldn't change some read-only bits)
    ESP_LOGD(TAG, "%s:  - read register (0x%02x : 0x%02x)", this->GetName(), reg, rcvbuf[0]);
    timeout += 10;
    } while ( (rcvbuf[0] != value) && (timeout < MCP2515_TIMEOUT) );

  if (timeout >= MCP2515_TIMEOUT)
    {
    ESP_LOGE(TAG, "%s: Could not set register (0x%02x to val 0x%02x)", this->GetName(), reg, value);
    return ESP_FAIL;
    }
  return ESP_OK;
  }


esp_err_t mcp2515::Start(CAN_mode_t mode, CAN_speed_t speed)
  {
  canbus::Start(mode, speed);
  uint8_t buf[16];

  m_mode = mode;
  m_speed = speed;

  // RESET commmand
  m_spibus->spi_cmd(m_spi, buf, 0, 1, CMD_RESET);
  vTaskDelay(50 / portTICK_PERIOD_MS);

  // CANINTE (interrupt enable), disable all interrupts during configuration
  WriteRegAndVerify(REG_CANINTE, 0);

  // Set CONFIG mode (abort transmisions, one-shot mode, clkout disabled)
  WriteReg(REG_CANCTRL, CANCTRL_MODE_CONFIG | CANCTRL_ABAT | CANCTRL_OSM);

  // Rx Buffer 0 control (receive all and enable buffer 1 rollover)
  WriteRegAndVerify(REG_RXB0CTRL, 0b01100100, 0b01101101);

  // BFPCTRL RXnBF PIN CONTROL AND STATUS
  WriteRegAndVerify(REG_BFPCTRL, 0b00001100);

  // Bus speed
  uint8_t cnf1 = 0;
  uint8_t cnf2 = 0;
  uint8_t cnf3 = 0;
  switch (m_speed)
    {
    case CAN_SPEED_33KBPS:
      // Recommended SWCAN settings according to GMLAN specs (GMW3089): SJW=2 and Sample Point = 86,67%
      cnf1=0x4f; // SJW=2, BRP=15
      cnf2=0xad; // BTLMODE=1, SAM=0,  PHSEG1=5 (6 Tq), PRSEG=5 (6 Tq)
      cnf3=0x81; // SOF=1, WAKFIL=0, PHSEG2=1 (2 tQ)
      break;
    case CAN_SPEED_83KBPS:
      cnf1=0x03; cnf2=0xbe; cnf3=0x07;
      break;
    case CAN_SPEED_100KBPS:
      cnf1=0x03; cnf2=0xfa; cnf3=0x87;
      break;
    case CAN_SPEED_125KBPS:
      cnf1=0x03; cnf2=0xf0; cnf3=0x86;
      break;
    case CAN_SPEED_250KBPS:
      cnf1=0x41; cnf2=0xf1; cnf3=0x85;
      break;
    case CAN_SPEED_500KBPS:
      cnf1=0x00; cnf2=0xf0; cnf3=0x86;
      break;
    case CAN_SPEED_1000KBPS:
      cnf1=0x00; cnf2=0xd0; cnf3=0x82;
      break;
    }
  m_spibus->spi_cmd(m_spi, buf, 0, 5, CMD_WRITE, REG_CNF3, cnf3, cnf2, cnf1);

  // Active/Listen Mode
  uint8_t ret;
  if (m_mode == CAN_MODE_LISTEN)
    ret=ChangeMode(CANCTRL_MODE_LISTEN);
  else
    ret=ChangeMode(CANCTRL_MODE_NORMAL);
  if (ret != ESP_OK)
      return ESP_FAIL;

  // Clear abort transmisions & one-shot mode:
  m_spibus->spi_cmd(m_spi, buf, 0, 4, CMD_BITMODIFY, REG_CANCTRL, CANCTRL_OSM | CANCTRL_ABAT, 0);

  // finally verify configuration registers
  uint8_t * rcvbuf = m_spibus->spi_cmd(m_spi, buf, 3, 2, CMD_READ, REG_CNF3);
  if ( (cnf1!=rcvbuf[2]) or (cnf2!=rcvbuf[1]) or (cnf3!=rcvbuf[0]) )
    {
    ESP_LOGE(TAG, "%s: could not change configuration registers! (read CNF 0x%02x 0x%02x 0x%02x)", this->GetName(),
      rcvbuf[2], rcvbuf[1], rcvbuf[0]);
    return ESP_FAIL;
    }

  // CANINTE (interrupt enable), all interrupts
  WriteReg(REG_CANINTE, 0b11111111);

  // And record that we are powered on
  pcp::SetPowerMode(On);

  return ESP_OK;
  }


esp_err_t mcp2515::ChangeMode( uint8_t mode )
  {
  uint8_t buf[16];
  uint8_t * rcvbuf;
  uint16_t timeout = 0;

  ESP_LOGD(TAG, "%s: Change op mode to 0x%02x", this->GetName(), mode);

  m_spibus->spi_cmd(m_spi, buf, 0, 4, CMD_BITMODIFY, REG_CANCTRL, CANCTRL_MODE, mode);

  // verify that mode is changed by polling CANSTAT register
  do
    {
    vTaskDelay(20 / portTICK_PERIOD_MS);

    rcvbuf = m_spibus->spi_cmd(m_spi, buf, 1, 2, CMD_READ, REG_CANSTAT);
    ESP_LOGD(TAG, "%s:  read CANSTAT register (0x%02x : 0x%02x)", this->GetName(), REG_CANSTAT, rcvbuf[0]);
    timeout += 20;

    } while ( ((rcvbuf[0] & CANCTRL_MODE) != mode) && (timeout < MCP2515_TIMEOUT) );

  if (timeout >= MCP2515_TIMEOUT)
    {
    ESP_LOGE(TAG, "%s: Could not change mode to 0x%02x", this->GetName(), mode);
    return ESP_FAIL;
    }
  return ESP_OK;
  }


esp_err_t mcp2515::Stop()
  {
  canbus::Stop();

  uint8_t buf[16];

  // RESET command
  m_spibus->spi_cmd(m_spi, buf, 0, 1, CMD_RESET);
  vTaskDelay(50 / portTICK_PERIOD_MS);

  // BFPCTRL RXnBF PIN CONTROL AND STATUS
  WriteRegAndVerify(REG_BFPCTRL, 0b00111100);

  // Set SLEEP mode
  ChangeMode(CANCTRL_MODE_SLEEP);

  // And record that we are powered down
  pcp::SetPowerMode(Off);

  return ESP_OK;
  }

esp_err_t mcp2515::ViewRegisters()
  {
  uint8_t buf[20];
  uint8_t cnf[3];
  // fetch configuration registers
  uint8_t * rcvbuf = m_spibus->spi_cmd(m_spi, buf, 9, 2, CMD_READ, REG_CNF3);
  cnf[0] = rcvbuf[2];
  cnf[1] = rcvbuf[1];
  cnf[2] = rcvbuf[0];
  ESP_LOGI(TAG,
    "%s: configuration registers: CNF 0x%02x 0x%02x 0x%02x",
    this->GetName(), cnf[0], cnf[1], cnf[2]);
  ESP_LOGI(TAG,
    "%s: CANINTE 0x%02x CANINTF 0x%02x EFLG 0x%02x CANSTAT 0x%02x CANCTRL 0x%02x TXB0CTRL 0x%02x",
    this->GetName(), rcvbuf[3], rcvbuf[4], rcvbuf[5], rcvbuf[6], rcvbuf[7], rcvbuf[8]);
  // read error counters
  rcvbuf = m_spibus->spi_cmd(m_spi, buf, 2, 2, CMD_READ, REG_TEC);
  uint8_t errors_tx = rcvbuf[0];
  uint8_t errors_rx = rcvbuf[1];
  ESP_LOGI(TAG, "%s: tx_errors: 0x%02x. rx_errors: 0x%02x", this->GetName(),
      errors_tx, errors_rx);
  rcvbuf = m_spibus->spi_cmd(m_spi, buf, 1, 2, CMD_READ, REG_BFPCTRL);
  ESP_LOGI(TAG, "%s: BFPCTRL 0x%02x", this->GetName(), rcvbuf[0]);
  return ESP_OK;
  }


/**
 * WriteFrame: deliver a frame to the hardware for transmission (driver internal)
 */
esp_err_t mcp2515::WriteFrame(const CAN_frame_t* p_frame)
  {
  uint8_t buf[16];
  uint8_t id[4];

  // check for free TX buffer:
  uint8_t txbuf;
  uint8_t* p = m_spibus->spi_cmd(m_spi, buf, 1, 1, CMD_READ_STATUS);

  if ((p[0] & 0b01010100) == 0)  // any buffers busy?
    txbuf = 0b000;  // all clear - use TxB0
  else
    return ESP_FAIL;

  if (p_frame->FIR.B.FF == CAN_frame_std)
    {
    // Transmit a standard frame
    id[0] = p_frame->MsgID >> 3;     // HIGH 8 bits of standard ID
    id[1] = p_frame->MsgID << 5;     // LOW 3 bits of standard ID
    id[2] = 0;
    id[3] = 0;
    }
  else
    {
    // Transmit an extended frame
    id[0] = (p_frame->MsgID >> 21) & 0xff;    // HIGH bits
    id[1] = ((p_frame->MsgID >> 13) & 0xe0)   // Next middle bits of extended ID; set the EXT bit too.
          + ((p_frame->MsgID >> 16) & 0x03)
          + 0x08;
    id[2] = (p_frame->MsgID >> 8) & 0xff;     // MID 8 bits of extended ID
    id[3] = (p_frame->MsgID & 0xff);          // LOW 8 bits of extended ID
    }

  // MCP2515 load transmit buffer:
  m_spibus->spi_cmd(m_spi, buf, 0, 14, CMD_LOAD_TXBUF | txbuf,
    id[0], id[1], id[2], id[3], p_frame->FIR.B.DLC,
    p_frame->data.u8[0],
    p_frame->data.u8[1],
    p_frame->data.u8[2],
    p_frame->data.u8[3],
    p_frame->data.u8[4],
    p_frame->data.u8[5],
    p_frame->data.u8[6],
    p_frame->data.u8[7]);

  // MCP2515 request to send:
  m_spibus->spi_cmd(m_spi, buf, 0, 1, CMD_RTS | (txbuf ? txbuf : 0b001));

  return ESP_OK;
  }


/**
 * Write: transmit or queue a frame for transmission (API)
 */
esp_err_t mcp2515::Write(const CAN_frame_t* p_frame, TickType_t maxqueuewait /*=0*/)
  {
  OvmsMutexLock lock(&m_write_mutex);

  if (m_mode != CAN_MODE_ACTIVE)
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

  // OK, stats & logging:
  canbus::Write(p_frame, maxqueuewait);

  return ESP_OK;
  }


// This function serves as asynchronous interrupt handler for both rx and tx tasks as well as error states
// Returns true if this function needs to be called again (another frame may need handling or all error interrupts are not yet handled)
bool mcp2515::AsynchronousInterruptHandler(CAN_frame_t* frame, bool * frameReceived)
  {
  uint8_t buf[16];

  *frameReceived = false;
  CAN_log_type_t log_status = CAN_LogNone;

  // read interrupts (CANINTF 0x2c), errors (EFLG 0x2d) and transmission status (TXB0CTRL 0x30):
  uint8_t *p = m_spibus->spi_cmd(m_spi, buf, 5, 2, CMD_READ, REG_CANINTF);
  uint8_t intstat = p[0];
  uint8_t errflag = p[1];
  uint8_t txb0ctrl = p[4];

  if (intstat == 0)
    {
    // all interrupts handled
    return false;
    }

  // handle RX buffers and other interrupts sequentially:
  int intflag;
  if (intstat & CANINTF_RX0IF)
    {
    // RX buffer 0 is full, handle it
    intflag = 0x01;
    }
  else if (intstat & CANINTF_RX1IF)
    {
    // RX buffer 1 is full, handle it
    intflag = 0x02;
    }
  else
    {
    // other interrupts:
    intflag = intstat;
    }

  m_status.error_flags = (intstat << 24) | (errflag << 16) | intflag;

  if (intflag <= 2)
    {
    // The indicated RX buffer has a message to be read
    memset(frame,0,sizeof(*frame));
    frame->origin = this;

    // read RX buffer and clear interrupt flag:
    uint8_t *p = m_spibus->spi_cmd(m_spi, buf, 13, 1, CMD_READ_RXBUF + ((intflag==1) ? 0 : 4));

    if (p[1] & 0x08) //check for extended mode=1, or std mode=0
      {
      frame->FIR.B.FF = CAN_frame_ext;           // Extended mode
      frame->MsgID = ((uint32_t)p[0]<<21)
                    + (((uint32_t)p[1]&0xe0)<<13)
                    + (((uint32_t)p[1]&0x03)<<16)
                    + ((uint32_t)p[2]<<8)
                    + ((uint32_t)p[3]);
      }
    else
      {
      frame->FIR.B.FF = CAN_frame_std;
      frame->MsgID = ((uint32_t)p[0] << 3) + (p[1] >> 5);  // Standard mode
      }

    frame->FIR.B.DLC = p[4] & 0x0f;

    memcpy(&frame->data,p+5,8);
    *frameReceived=true;
    }

  // handle other interrupts that came in at the same time:

  if (intstat & CANINTF_TX012IF)
    {
    // TX buffer(s) have become available; clear IRQs and fill up:
    m_spibus->spi_cmd(m_spi, buf, 0, 4, CMD_BITMODIFY, REG_CANINTF, intstat & CANINTF_TX012IF, 0);
    m_status.error_flags |= 0x0100;

    // Note: the TXnIF bits only get set on successful transmission (see TX flowchart)
    // Queue "tx success" callback:
    CAN_queue_msg_t msg;
    msg.type = CAN_txcallback;
    msg.body.frame = m_tx_frame;
    msg.body.bus = this;
    xQueueSend(MyCan.m_rxqueue, &msg, 0);
    }

  if (intstat & (CANINTF_MERRF | CANINTF_WAKIF | CANINTF_ERRIF))
    {
    // Error interrupts:
    //  MERRF = message tx/rx error
    //  WAKIF = wakeup
    //  ERRIF = overflow / error state change (details in errflag)

    // Check for RX overflow:
    if (errflag & EFLG_RX1OVR) // RXB1 overflow
      {
      m_status.rxbuf_overflow++;
      m_status.error_flags |= 0x0200;
      //ESP_LOGW(TAG, "CAN Bus 2/3 receive overflow; Frame lost.");
      log_status = CAN_LogStatus_Error;
      }
    if (errflag & EFLG_RX0OVR) // RXB0 overflow.  No data lost in this case (it went into RXB1)
      {
      m_status.error_flags |= 0x0400;
      }

    // Read error counters:
    p = m_spibus->spi_cmd(m_spi, buf, 2, 2, CMD_READ, REG_TEC);
    m_status.errors_tx = p[0];
    m_status.errors_rx = p[1];
    if (errflag & EFLG_TXBO)
      {
      m_status.errors_tx |= 0x100;
      m_status.errors_rx |= 0x100;
      }

    // Check for TX failure:
    //  We consider the TX to have failed if a bus error is detected during the
    //  transmission attempt and the controller gave up on retrying (= entered
    //  passive mode, txerr >= 128).
    if ((intstat & CANINTF_MERRF) != 0 &&
        (txb0ctrl & TXBCTRL_TXREQ) != 0 &&
        m_status.errors_tx >= 128)
      {
      //ESP_LOGE(TAG, "AsynchronousInterruptHandler: error while sending frame. msgId 0x%x", m_tx_frame.MsgID);

      // Abort TX to cancel further retransmission attempts:
      // … set ABAT, poll for TXREQ to become clear, clear ABAT:
      m_spibus->spi_cmd(m_spi, buf, 0, 4, CMD_BITMODIFY, REG_CANCTRL, CANCTRL_ABAT, CANCTRL_ABAT);
      do
        p = m_spibus->spi_cmd(m_spi, buf, 1, 1, CMD_READ_STATUS);
      while (p[0] & STATUS_TX012REQ);
      m_spibus->spi_cmd(m_spi, buf, 0, 4, CMD_BITMODIFY, REG_CANCTRL, CANCTRL_ABAT, 0);

      // … get TXERR & ABTF flags:
      p = m_spibus->spi_cmd(m_spi, buf, 1, 2, CMD_READ, REG_TXB0CTRL);
      bool tx_aborted = ((p[0] & (TXBCTRL_ABTF | TXBCTRL_TXERR)) != 0);

      // … and clear TX IRQs in case the abort request came too late:
      m_spibus->spi_cmd(m_spi, buf, 0, 4, CMD_BITMODIFY, REG_CANINTF, CANINTF_TX012IF, 0);

      // Queue TX callback:
      CAN_queue_msg_t msg;
      msg.type = tx_aborted ? CAN_txfailedcallback : CAN_txcallback;
      msg.body.frame = m_tx_frame;
      msg.body.bus = this;
      xQueueSend(MyCan.m_rxqueue, &msg, 0);
      // …which will log the error as well
      }
    else
      {
      // Request log entry for other errors:
      log_status = CAN_LogStatus_Error;
      }
    }
  else
    {
    // No error interrupt signaled, but we want to follow the error recovery:
    if (m_status.errors_tx || m_status.errors_rx)
      {
      // Read error counters:
      p = m_spibus->spi_cmd(m_spi, buf, 2, 2, CMD_READ, REG_TEC);
      m_status.errors_tx = p[0];
      m_status.errors_rx = p[1];
      if (errflag & EFLG_TXBO)
        {
        m_status.errors_tx |= 0x100;
        m_status.errors_rx |= 0x100;
        }
      if (!m_status.errors_tx && !m_status.errors_rx)
        m_status.error_flags = 0;
      }
    }

  // clear RX buffer overflow flags:
  if (errflag & EFLG_RX01OVR)
    {
    m_status.error_flags |= 0x0800;
    m_spibus->spi_cmd(m_spi, buf, 0, 4, CMD_BITMODIFY, REG_EFLG, errflag & EFLG_RX01OVR, 0);
    }

  // Log bus error state change:
  uint8_t log_errflag = errflag & ~EFLG_RX01OVR;
  if (log_errflag && log_errflag != m_last_errflag)
    {
    ESP_LOGW(TAG, "%s EFLG: %s%s%s%s%s%s", this->GetName(),
      (errflag & EFLG_TXBO)  ? "Bus-off " : "",
      (errflag & EFLG_TXEP)  ? "TX_Err_Passv " : "",
      (errflag & EFLG_RXEP)  ? "RX_Err_Passv " : "",
      (errflag & EFLG_TXWAR) ? "TX_Err_Warn " : "",
      (errflag & EFLG_RXWAR) ? "RX_Err_Warn " : "",
      (errflag & EFLG_EWARN) ? "EWARN " : "" );
    }
  if (log_errflag == 0 && (m_last_errflag & ~EFLG_RX01OVR) != 0)
    {
    // Recovered:
    log_status = CAN_LogStatus_Statistics;
    }
  m_last_errflag = log_errflag;

  // clear error & wakeup interrupts:
  if (intstat & (CANINTF_MERRF | CANINTF_WAKIF | CANINTF_ERRIF))
    {
    m_status.error_flags |= 0x1000;
    m_spibus->spi_cmd(m_spi, buf, 0, 4, CMD_BITMODIFY, REG_CANINTF,
      intstat & (CANINTF_MERRF | CANINTF_WAKIF | CANINTF_ERRIF), 0);
    }

  if (log_status != CAN_LogNone)
    {
    LogStatus(log_status);
    }

  // Read the interrupt pin status and if it's still active (low), require another interrupt handling iteration
  return !gpio_get_level((gpio_num_t)m_intpin);
  }


void mcp2515::TxCallback(CAN_frame_t* p_frame, bool success)
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


void mcp2515::SetPowerMode(PowerMode powermode)
  {
  pcp::SetPowerMode(powermode);
  switch (powermode)
    {
    case  On:
      ESP_LOGI(TAG, "%s: SetPowerMode on", this->GetName());
      if (m_mode != CAN_MODE_OFF)
        {
        Start(m_mode, m_speed);
        }
      break;
    case Sleep:
    case DeepSleep:
    case Off:
      ESP_LOGI(TAG, "%s: SetPowerMode off", this->GetName());
      Stop();
      break;
    default:
      break;
    }
  }
