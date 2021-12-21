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

#ifndef __ESP32CAN_REGDEF_H__
#define __ESP32CAN_REGDEF_H__

// Start address of CAN registers
#define MODULE_ESP32CAN                             ((volatile ESP32CAN_Module_t    *)0x3ff6b000)

// Get standard message ID
#define ESP32CAN_GET_STD_ID                         (((uint32_t)MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.STD.ID[0] << 3) | \
                                                     (MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.STD.ID[1] >> 5))

// Get extended message ID
#define ESP32CAN_GET_EXT_ID                         (((uint32_t)MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.EXT.ID[0] << 21) | \
                                                     (MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.EXT.ID[1] << 13) | \
                                                     (MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.EXT.ID[2] << 5) | \
                                                     (MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.EXT.ID[3] >> 3 ))

// Set standard message ID
#define ESP32CAN_SET_STD_ID(x)                      MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.STD.ID[0] = ((x) >> 3);	\
                                                    MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.STD.ID[1] = ((x) << 5);

// Set extended message ID
#define ESP32CAN_SET_EXT_ID(x)                      MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.EXT.ID[0] = ((x) >> 21);	\
                                                    MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.EXT.ID[1] = ((x) >> 13);	\
                                                    MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.EXT.ID[2] = ((x) >> 5);	\
                                                    MODULE_ESP32CAN->MBX_CTRL.FCTRL.TX_RX.EXT.ID[3] = ((x) << 3);	\

// Status register (SR)
typedef enum
  {
  __CAN_STS_RXBUF=              BIT(0),             // SR.0 Receive Buffer Status (1=message(s) in buffer)
  __CAN_STS_DATA_OVERRUN=       BIT(1),             // SR.1 Data Overrun Status (1=message(s) lost)
  __CAN_STS_TXFREE=             BIT(2),             // SR.2 Transmit Buffer Status (1=released)
  __CAN_STS_TXDONE=             BIT(3),             // SR.3 Transmission Complete Status (1=successful)
  __CAN_STS_RXPEND=             BIT(4),             // SR.4 Receive Status (1=receiving a message)
  __CAN_STS_TXPEND=             BIT(5),             // SR.5 Transmit Status (1=transmitting a message)
  __CAN_STS_ERR_WARNING=        BIT(6),             // SR.6 Error Status (1=warning; error count >= 96)
  __CAN_STS_BUS_OFF=            BIT(7),             // SR.7 Bus Status (1=bus-off)
  } ESP32CAN_STS_t;

// Error code capture (ECC)
#define __CAN_ECC_ERRC          0b11000000
#define __CAN_ECC_DIR           0b00100000
#define __CAN_ECC_SEGMENT       0b00011111

// Interrupt status register (IR)
typedef enum
  {
  __CAN_IRQ_RX=                 BIT(0),             // IR.0 Receive Interrupt
  __CAN_IRQ_TX=                 BIT(1),             // IR.1 Transmit Interrupt
  __CAN_IRQ_ERR_WARNING=        BIT(2),             // IR.2 Error Interrupt (warning state change)
  __CAN_IRQ_DATA_OVERRUN=       BIT(3),             // IR.3 Data Overrun Interrupt
  __CAN_IRQ_WAKEUP=             BIT(4),             // IR.4 Wake-Up Interrupt
  __CAN_IRQ_ERR_PASSIVE=        BIT(5),             // IR.5 Error Passive Interrupt (passive state change)
  __CAN_IRQ_ARB_LOST=           BIT(6),             // IR.6 Arbitration Lost Interrupt
  __CAN_IRQ_BUS_ERR=            BIT(7),             // IR.7 Bus Error Interrupt
  __CAN_IRQ_INVALID_RX=         BIT(8),             // Invalid RX Frame (synthetic)
  } ESP32CAN_IRQ_t;

/* 
 * Setting this interrupt enable register bit with ESP32 revision
 * 2 and higher divides the Baud Rate Prescaler (BRP) by 2.
 */
#define __CAN_IER_BRP_DIV __CAN_IRQ_WAKEUP

/* The BRP is 6 bits wide */
#define BRP_MAX ((1 << 6) - 1)

/** \brief OCMODE options. */
typedef enum
  {
  __CAN_OC_BOM=                 0b00,               // bi-phase output mode
  __CAN_OC_TOM=                 0b01,               // test output mode
  __CAN_OC_NOM=                 0b10,               // normal output mode
  __CAN_OC_COM=                 0b11,               // clock output mode
  } ESP32CAN_OCMODE_t;

// CAN controller (SJA1000).

typedef struct
  {
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int RM:1; 						/**< \brief MOD.0 Reset Mode */
      unsigned int LOM:1;            			/**< \brief MOD.1 Listen Only Mode */
      unsigned int STM:1;                     /**< \brief MOD.2 Self Test Mode */
      unsigned int AFM:1;                   	/**< \brief MOD.3 Acceptance Filter Mode */
      unsigned int SM:1;            			/**< \brief MOD.4 Sleep Mode */
      unsigned int reserved_27:27;            /**< \brief \internal Reserved */
      } B;
    } MOD;
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int TR:1; 						/**< \brief CMR.0 Transmission Request */
      unsigned int AT:1;            			/**< \brief CMR.1 Abort Transmission */
      unsigned int RRB:1;                     /**< \brief CMR.2 Release Receive Buffer */
      unsigned int CDO:1;                   	/**< \brief CMR.3 Clear Data Overrun */
      unsigned int GTS:1;            			/**< \brief CMR.4 Go To Sleep */
      unsigned int reserved_27:27;            /**< \brief \internal Reserved */
      } B;
    } CMR;
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int RBS:1; 					/**< \brief SR.0 Receive Buffer Status */
      unsigned int DOS:1;            			/**< \brief SR.1 Data Overrun Status */
      unsigned int TBS:1;                     /**< \brief SR.2 Transmit Buffer Status */
      unsigned int TCS:1;                   	/**< \brief SR.3 Transmission Complete Status */
      unsigned int RS:1;            			/**< \brief SR.4 Receive Status */
      unsigned int TS:1;            			/**< \brief SR.5 Transmit Status */
      unsigned int ES:1;            			/**< \brief SR.6 Error Status */
      unsigned int BS:1;            			/**< \brief SR.7 Bus Status */
      unsigned int reserved_24:24;            /**< \brief \internal Reserved */
      } B;
    } SR;
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int RI:1; 						/**< \brief IR.0 Receive Interrupt */
      unsigned int TI:1;            			/**< \brief IR.1 Transmit Interrupt */
      unsigned int EI:1;                     	/**< \brief IR.2 Error Interrupt */
      unsigned int DOI:1;                   	/**< \brief IR.3 Data Overrun Interrupt */
      unsigned int WUI:1;            			/**< \brief IR.4 Wake-Up Interrupt */
      unsigned int EPI:1;            			/**< \brief IR.5 Error Passive Interrupt */
      unsigned int ALI:1;            			/**< \brief IR.6 Arbitration Lost Interrupt */
      unsigned int BEI:1;            			/**< \brief IR.7 Bus Error Interrupt */
      unsigned int reserved_24:24;            /**< \brief \internal Reserved */
      } B;
    } IR;
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int RIE:1; 					/**< \brief IER.0 Receive Interrupt Enable */
      unsigned int TIE:1;            			/**< \brief IER.1 Transmit Interrupt Enable */
      unsigned int EIE:1;                     /**< \brief IER.2 Error Interrupt Enable */
      unsigned int DOIE:1;                   	/**< \brief IER.3 Data Overrun Interrupt Enable */
      unsigned int WUIE:1;            		/**< \brief IER.4 Wake-Up Interrupt Enable */
      unsigned int EPIE:1;            		/**< \brief IER.5 Error Passive Interrupt Enable */
      unsigned int ALIE:1;            		/**< \brief IER.6 Arbitration Lost Interrupt Enable */
      unsigned int BEIE:1;            		/**< \brief IER.7 Bus Error Interrupt Enable */
      unsigned int reserved_24:24;            /**< \brief \internal Reserved  */
      } B;
    } IER;
  uint32_t RESERVED0;
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int BRP:6; 					/**< \brief BTR0[5:0] Baud Rate Prescaler */
      unsigned int SJW:2;            			/**< \brief BTR0[7:6] Synchronization Jump Width*/
      unsigned int reserved_24:24;            /**< \brief \internal Reserved  */
      } B;
    } BTR0;
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int TSEG1:4; 					/**< \brief BTR1[3:0] Timing Segment 1 */
      unsigned int TSEG2:3;            		/**< \brief BTR1[6:4] Timing Segment 2*/
      unsigned int SAM:1;            			/**< \brief BTR1.7 Sampling*/
      unsigned int reserved_24:24;            /**< \brief \internal Reserved  */
      } B;
    } BTR1;
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int OCMODE:2; 					/**< \brief OCR[1:0] Output Control Mode, see # */
      unsigned int OCPOL0:1;                  /**< \brief OCR.2 Output Control Polarity 0 */
      unsigned int OCTN0:1;                   /**< \brief OCR.3 Output Control Transistor N0 */
      unsigned int OCTP0:1;            		/**< \brief OCR.4 Output Control Transistor P0 */
      unsigned int OCPOL1:1;            		/**< \brief OCR.5 Output Control Polarity 1 */
      unsigned int OCTN1:1;            		/**< \brief OCR.6 Output Control Transistor N1 */
      unsigned int OCTP1:1;            		/**< \brief OCR.7 Output Control Transistor P1 */
      unsigned int reserved_24:24;            /**< \brief \internal Reserved  */
      } B;
    } OCR;
  uint32_t RESERVED1[2];
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int ALC:8; 					/**< \brief ALC[7:0] Arbitration Lost Capture */
      unsigned int reserved_24:24;            /**< \brief \internal Reserved  */
      } B;
    } ALC;
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int ECC:8; 					/**< \brief ECC[7:0] Error Code Capture */
      unsigned int reserved_24:24;            /**< \brief \internal Reserved  */
      } B;
    } ECC;
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int EWLR:8; 					/**< \brief EWLR[7:0] Error Warning Limit */
      unsigned int reserved_24:24;            /**< \brief \internal Reserved  */
      } B;
    } EWLR;
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int RXERR:8; 					/**< \brief RXERR[7:0] Receive Error Counter */
      unsigned int reserved_24:24;            /**< \brief \internal Reserved  */
      } B;
    } RXERR;
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int TXERR:8; 					/**< \brief TXERR[7:0] Transmit Error Counter */
      unsigned int reserved_24:24;            /**< \brief \internal Reserved  */
      } B;
    } TXERR;
  union
    {
    struct
      {
      uint32_t CODE[4];						/**< \brief Acceptance Message ID */
      uint32_t MASK[4];						/**< \brief Acceptance Mask */
      uint32_t RESERVED2[5];
      } ACC;										/**< \brief Acceptance filtering */
    struct
      {
      CAN_FIR_t	FIR;						/**< \brief Frame information record */
      union
        {
        struct
          {
          uint32_t ID[2];					/**< \brief Standard frame message-ID*/
          uint32_t data[8];				/**< \brief Standard frame payload */
          uint32_t reserved[2];
          } STD;								/**< \brief Standard frame format */
        struct
          {
          uint32_t ID[4];					/**< \brief Extended frame message-ID*/
          uint32_t data[8];				/**< \brief Extended frame payload */
          } EXT;								/**< \brief Extended frame format */
        } TX_RX;									/**< \brief RX/TX interface */
      } FCTRL;										/**< \brief Function control regs */
    } MBX_CTRL;										/**< \brief Mailbox control */
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int RMC:8; 					/**< \brief RMC[7:0] RX Message Counter */
      unsigned int reserved_24:24;            /**< \brief \internal Reserved Enable */
      } B;
    } RMC;
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int RBSA:8; 					/**< \brief RBSA[7:0] RX Buffer Start Address  */
      unsigned int reserved_24:24;            /**< \brief \internal Reserved Enable */
      } B;
    } RBSA;
  union
    {
    uint32_t U;								/**< \brief Unsigned access */
    struct
      {
      unsigned int COD:3; 					/**< \brief CDR[2:0] CLKOUT frequency selector based of fOSC*/
      unsigned int COFF:1; 					/**< \brief CDR.3 CLKOUT off*/
      unsigned int reserved_1:1; 				/**< \brief \internal Reserved */
      unsigned int RXINTEN:1; 				/**< \brief CDR.5 This bit allows the TX1 output to be used as a dedicated receive interrupt output*/
      unsigned int CBP:1; 					/**< \brief CDR.6 allows to bypass the CAN input comparator and is only possible in reset mode.*/
      unsigned int CAN_M:1; 					/**< \brief CDR.7 If CDR.7 is at logic 0 the CAN controller operates in BasicCAN mode. If set to logic 1 the CAN controller operates in PeliCAN mode. Write access is only possible in reset mode*/
      unsigned int reserved_24:24;            /**< \brief \internal Reserved  */
      } B;
    } CDR;
  uint32_t IRAM[2];
 } ESP32CAN_Module_t;

#endif // __ESP32CAN_REGDEF_H__
