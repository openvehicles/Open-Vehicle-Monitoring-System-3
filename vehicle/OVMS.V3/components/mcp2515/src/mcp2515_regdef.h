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

#ifndef __MCP2515_REGDEF_H__
#define __MCP2515_REGDEF_H__

// MCP2515 SPI commands:
#define CMD_RESET               0b11000000
#define CMD_READ                0b00000011
#define CMD_WRITE               0b00000010
#define CMD_RTS                 0b10000000
#define CMD_BITMODIFY           0b00000101
#define CMD_READ_RXBUF          0b10010000
#define CMD_LOAD_TXBUF          0b01000000
#define CMD_READ_STATUS         0b10100000

// CANCTRL (Control) register flags
#define CANCTRL_MODE            0b11100000    // Mask
#define CANCTRL_MODE_CONFIG     0b10000000
#define CANCTRL_MODE_LISTEN     0b01100000
#define CANCTRL_MODE_LOOPBACK   0b01000000
#define CANCTRL_MODE_SLEEP      0b00100000
#define CANCTRL_MODE_NORMAL     0b00000000
#define CANCTRL_ABAT            0b00010000    // Abort All Pending Transmissions
#define CANCTRL_OSM             0b00001000    // One shot mode
#define CANCTRL_CLKEN           0b00000100    // CLKOUT pin enable

// CANINTE & CANINTF (Interrupt) register flags
#define CANINTF_MERRF           0b10000000    // RX/TX Message error
#define CANINTF_WAKIF           0b01000000    // Wakeup
#define CANINTF_ERRIF           0b00100000    // Error state change (details in EFLG)
#define CANINTF_TX2IF           0b00010000    // TX buffer 2 empty
#define CANINTF_TX1IF           0b00001000    // TX buffer 1 empty
#define CANINTF_TX0IF           0b00000100    // TX buffer 0 empty
#define CANINTF_TX012IF         0b00011100    // Mask: any/all TX buffers empty
#define CANINTF_RX1IF           0b00000010    // RX buffer 1 full
#define CANINTF_RX0IF           0b00000001    // RX buffer 0 full
#define CANINTF_RX01IF          0b00000011    // Mask: any/all RX buffers full

// EFLG (Error) register flags
#define EFLG_RX1OVR             0b10000000    // Receive Buffer 1 Overflow
#define EFLG_RX0OVR             0b01000000    // Receive Buffer 0 Overflow
#define EFLG_RX01OVR            0b11000000    // Mask: any/all RX Buffer Overflow
#define EFLG_TXBO               0b00100000    // Bus-Off Error (TEC == 255)
#define EFLG_TXEP               0b00010000    // Transmit Error-Passive (TEC >= 128)
#define EFLG_RXEP               0b00001000    // Receive Error-Passive (REC >= 128)
#define EFLG_EPASS              0b00011000    // Mask: TX or RX Error-Passive Mode
#define EFLG_TXWAR              0b00000100    // Transmit Error Warning (TEC >= 96)
#define EFLG_RXWAR              0b00000010    // Receive Error Warning (REC >= 96)
#define EFLG_EWARN              0b00000001    // Error Warning (TXWAR or RXWAR set)

// TXBnCTRL (Transmission Buffer Control) register flags
#define TXBCTRL_ABTF            0b01000000    // Message Aborted
#define TXBCTRL_MLOA            0b00100000    // Message Lost Arbitration
#define TXBCTRL_TXERR           0b00010000    // Transmission Error (bus error)
#define TXBCTRL_TXREQ           0b00001000    // Message Transmit Request (TX pending)

// CMD_READ_STATUS flags
#define STATUS_TX2IF            0b10000000    // CANINTF.TX2IF
#define STATUS_TX2REQ           0b01000000    // TXB2CNTRL.TXREQ
#define STATUS_TX1IF            0b00100000    // CANINTF.TX1IF
#define STATUS_TX1REQ           0b00010000    // TXB1CNTRL.TXREQ
#define STATUS_TX0IF            0b00001000    // CANINTF.TX0IF
#define STATUS_TX0REQ           0b00000100    // TXB0CNTRL.TXREQ
#define STATUS_RX1IF            0b00000010    // CANINTF.RX1IF
#define STATUS_RX0IF            0b00000001    // CANINTF.RX0IF
#define STATUS_TX012IF          0b10101000    // Mask: any/all TXnIF
#define STATUS_TX012REQ         0b01010100    // Mask: any/all TXnREQ
#define STATUS_RX01IF           0b00000011    // Mask: any/all RXnIF

// Register addresses
#define REG_CANSTAT             0x0E
#define REG_CANCTRL             0x0F
#define REG_BFPCTRL             0x0C
#define REG_TEC                 0x1C
#define REG_REC                 0x1D
#define REG_CNF3                0x28
#define REG_CNF2                0x29
#define REG_CNF1                0x2A
#define REG_CANINTE             0x2B
#define REG_CANINTF             0x2C
#define REG_EFLG                0x2D
#define REG_TXRTSCTRL           0x0D
#define REG_TXB0CTRL            0x30
#define REG_TXB1CTRL            0x40
#define REG_TXB2CTRL            0x50
#define REG_RXB0CTRL            0x60

#define MCP2515_TIMEOUT         100           // Timeout for register verification, in milliseconds

#endif //#ifndef __MCP2515_REGDEF_H__
