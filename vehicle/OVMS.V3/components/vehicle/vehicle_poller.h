/*
;    Project:       Open Vehicle Monitor System
;    Date:          9th April 2023

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
#ifndef __VEHICLE_POLLER_H__
#define __VEHICLE_POLLER_H__

#include <cstdint>

// ISO TP:
//  (see https://en.wikipedia.org/wiki/ISO_15765-2)

#define ISOTP_FT_SINGLE                 0
#define ISOTP_FT_FIRST                  1
#define ISOTP_FT_CONSECUTIVE            2
#define ISOTP_FT_FLOWCTRL               3

// Protocol variant:
#define ISOTP_STD                       0     // standard addressing (11 bit IDs)
#define ISOTP_EXTADR                    1     // extended addressing (19 bit IDs)
#define ISOTP_EXTFRAME                  2     // extended frame mode (29 bit IDs)
#define VWTP_16                         16    // VW/VAG Transport Protocol 1.6 (placeholder, unsupported)
#define VWTP_20                         20    // VW/VAG Transport Protocol 2.0

// Argument tag:
#define POLL_TXDATA                     0xff  // poll_pid_t using xargs for external payload up to 4095 bytes

// Number of polling states supported
#define VEHICLE_POLL_NSTATES            4

// VWTP_20 channel states:
typedef enum
  {
  VWTP_Closed = 0,                          // Channel not connecting/connected
  VWTP_ChannelClose,                        // Close request has been sent
  VWTP_ChannelSetup,                        // Setup request has been sent
  VWTP_ChannelParams,                       // Params request has been sent
  VWTP_Idle,                                // Connection established, idle
  VWTP_StartPoll,                           // Transit state: begin request transmission
  VWTP_Transmit,                            // Request transmission phase
  VWTP_Receive,                             // Response reception phase
  VWTP_AbortXfer,                           // Transit state: abort request/response
  } vwtp_channelstate_t;

// VWTP_20 channel:
typedef struct
  {
  vwtp_channelstate_t     state;            // VWTP channel state
  canbus*                 bus;              // CAN bus
  uint16_t                baseid;           // Protocol base CAN MsgID (usually 0x200)
  uint8_t                 moduleid;         // Logical address (ID) of destination module
  uint16_t                txid;             // CAN MsgID we transmit on
  uint16_t                rxid;             // CAN MsgID we listen to
  uint8_t                 blocksize;        // Number of blocks (data frames) to send between ACKs
  uint32_t                acktime;          // Max time [us] to wait for ACK (not yet implemented)
  uint32_t                septime;          // Min separation time [us] between blocks (data frames)
  uint32_t                txseqnr;          // TX packet sequence number
  uint32_t                rxseqnr;          // RX packet sequence number
  uint32_t                lastused;         // Timestamp of last channel access
  } vwtp_channel_t;


class OvmsPoller {
  public:
    typedef struct
      {
      uint32_t txmoduleid;                      // transmission CAN ID (address), 0x7df = OBD2 broadcast
      uint32_t rxmoduleid;                      // expected response CAN ID or 0 for broadcasts
      uint16_t type;                            // UDS poll type / OBD2 "mode", see VEHICLE_POLL_TYPE_…
      union
        {
        uint16_t pid;                           // PID (shortcut for requests w/o payload)
        struct
          {
          uint16_t pid;                         // PID for requests with additional payload
          uint8_t datalen;                      // payload length (bytes)
          uint8_t data[6];                      // inline payload data (single frame request)
          } args;
        struct
          {
          uint16_t pid;                         // PID for requests with additional payload
          uint8_t tag;                          // needs to be POLL_TXDATA
          uint16_t datalen;                     // payload length (bytes, max 4095)
          const uint8_t* data;                  // pointer to payload data (single/multi frame request)
          } xargs;
        };
      uint16_t polltime[VEHICLE_POLL_NSTATES];  // poll intervals in seconds for used poll states
      uint8_t  pollbus;                         // 0 = default CAN bus from PollSetPidList(), 1…4 = specific
      uint8_t  protocol;                        // ISOTP_STD / ISOTP_EXTADR / ISOTP_EXTFRAME / VWTP_20
      } poll_pid_t;

    typedef struct
      {
      canbus* bus;            ///< Bus to poll on.
      uint8_t bus_no;

      uint32_t protocol;      ///< ISOTP_STD / ISOTP_EXTADR / ISOTP_EXTFRAME / VWTP_20.
      uint16_t type;          ///< Expected type
      uint16_t pid;           ///< Expected PID
      uint32_t moduleid_sent; ///< ModuleID last sent
      uint32_t moduleid_low;  ///< Expected response moduleid low mark
      uint32_t moduleid_high; ///< Expected response moduleid high mark
      uint32_t moduleid_rec;  ///< Actual received moduleid.
      uint16_t mlframe;       ///< Frame number for multi frame response
      uint16_t mloffset;      ///< Current Byte offset of multi frame response
      uint16_t mlremain;      ///< Bytes remaining for multi frame response
      poll_pid_t entry;       ///< Currently processed entry of poll list (copy)
      uint32_t ticker;        ///< Polling tick count
      } poll_job_t;

    typedef enum : uint8_t { Primary, Secondary, Successful, OnceOff } poller_source_t;

// Macro for poll_pid_t termination
#define POLL_LIST_END                   { 0, 0, 0x00, 0x00, { 0, 0, 0 }, 0, 0 }

  class ParentSignal {
  public:
    virtual ~ParentSignal() { }
    // Signals for vehicle
    virtual void PollRunFinished() = 0;

    virtual void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length);
    virtual void IncomingPollError(const OvmsPoller::poll_job_t &job, uint16_t code);
    virtual void IncomingPollTxCallback(const OvmsPoller::poll_job_t &job, bool success);

    virtual void IncomingPollRxFrame(canbus* bus, CAN_frame_t* frame, bool success) = 0;
    virtual bool Ready() = 0;
    virtual uint8_t GetBusNo(canbus* bus) = 0;
    virtual canbus* GetBus(uint8_t busno) = 0;

    virtual void PollerSend(uint8_t busno, OvmsPoller::poller_source_t source) = 0;
  };

  protected:
    ParentSignal *m_parent_signal;

    OvmsRecMutex      m_poll_mutex;           // Concurrency protection for recursive calls
    uint8_t           m_poll_state;           // Current poll state
  private:
    // Poll state
    uint8_t           m_poll_default_bus;

    enum class PollMode : short { Standard, OnceOff, OnceOffDone };
    PollMode m_poll_mode;

    poll_pid_t  m_poll_once;                  // Once-off Poll
    const poll_pid_t* m_poll_plist;           // Head of poll list
    const poll_pid_t* m_poll_plcur;           // Poll list loop cursor

  protected:
    poll_job_t        m_poll;
    const uint8_t*    m_poll_tx_data;         // Payload data for multi frame request
    uint16_t          m_poll_tx_remain;       // Payload bytes remaining for multi frame request
    uint16_t          m_poll_tx_offset;       // Payload offset of multi frame request
    uint16_t          m_poll_tx_frame;        // Frame number for multi frame request
    uint8_t           m_poll_wait;            // Wait counter for a reply from a sent poll or bytes remaining.
                                              // Gets set = 2 when a poll is sent OR when bytes are remaining after receiving.
                                              // Gets set = 0 when a poll is received.
                                              // Gets decremented with every second/tick in PollerSend().
                                              // PollerSend() aborts when > 0.
                                              // Why set = 2: When a poll gets send just before the next ticker occurs
                                              //              PollerSend() decrements to 1 and doesn't send the next poll.
                                              //              Only when the reply doesn't get in until the next ticker occurs
                                              //              PollserSend() decrements to 0 and abandons the outstanding reply (=timeout)

    CanFrameCallback  m_poll_txcallback;      // Poller CAN TxCallback
    uint32_t          m_poll_txmsgid;         // Poller last TX CAN ID (frame MsgID)


  private:
    uint8_t           m_poll_sequence_max;    // Polls allowed to be sent in sequence per time tick (second), default 1, 0 = no limit
    uint8_t           m_poll_sequence_cnt;    // Polls already sent in the current time tick (second)
    uint8_t           m_poll_fc_septime;      // Flow control separation time for multi frame responses
    uint16_t          m_poll_ch_keepalive;    // Seconds to keep an inactive channel (e.g. VWTP) alive (default: 60)

  private:
    OvmsRecMutex      m_poll_single_mutex;    // PollSingleRequest() concurrency protection
    std::string*      m_poll_single_rxbuf;    // … response buffer
    int               m_poll_single_rxerr;    // … response error code (NRC) / TX failure code
    OvmsSemaphore     m_poll_single_rxdone;   // … response done (ok/error)

  protected:
    vwtp_channel_t    m_poll_vwtp;            // VWTP channel state

  protected:

    // Signals for vehicle
    void PollRunFinished();

    // Polling Response
    void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length);

    void IncomingPollError(const OvmsPoller::poll_job_t &job, uint16_t code);
    void IncomingPollTxCallback(const OvmsPoller::poll_job_t &job, bool success);

    bool Ready();
  private:
    void PollerSend(poller_source_t source);

    void PollerISOTPStart(bool fromTicker);
    bool PollerISOTPReceive(CAN_frame_t* frame, uint32_t msgid);

    void PollerVWTPStart(bool fromTicker);
    bool PollerVWTPReceive(CAN_frame_t* frame, uint32_t msgid);
    void PollerVWTPEnter(vwtp_channelstate_t state);
    void PollerVWTPTicker();
    void PollerVWTPTxCallback(const CAN_frame_t* frame, bool success);

  public:
    bool HasBus(canbus* bus) { return bus == m_poll.bus;}
  protected:

    enum class OvmsNextPollResult
      {
      StillAtEnd, ///< Still at end of list.
      FoundEntry, ///< Found an entry
      ReachedEnd, ///< Reached the end.
      Ignore      ///< Ignore this (don't move to next poller index)
      };

    // Poll entry manipulation: Must be called under lock of m_poll_mutex

    void ResetPollEntry();
    OvmsNextPollResult NextPollEntry(poller_source_t source, poll_pid_t *entry);
    void PollerNextTick(poller_source_t source);

    void Incoming(CAN_frame_t &frame, bool success);
    void Outgoing(const CAN_frame_t &frame, bool success);

    // Check for throttling.
    bool CanPoll();

    enum class OvmsPollEntryType : uint8_t
      {
      Poll,
      FrameRx,
      FrameTx,
      Command,
      PollState
      };
    enum class OvmsPollCommand : uint8_t
      {
      Pause,
      Resume,
      Throttle,
      ResponseSep,
      Keepalive
      };
    typedef struct {
        CAN_frame_t frame;
        bool success;
    } poll_frame_entry_t;
    typedef struct {
      OvmsPollCommand cmd;
      uint16_t parameter;
    } poll_command_entry_t;
    typedef struct {
      uint8_t busno ;
      poller_source_t source;
    } poll_source_entry_t;

    typedef struct {
      OvmsPollEntryType entry_type;
      union {
        poll_source_entry_t entry_Poll;
        poll_frame_entry_t entry_FrameRxTx;
        poll_command_entry_t entry_Command;
        uint8_t entry_PollState;
      };
    } poll_queue_entry_t;

    void Do_PollSetState(uint8_t state);

  public:
    OvmsPoller(canbus* can, uint8_t can_number, ParentSignal *parent_signal,
      const CanFrameCallback &polltxcallback);
    ~OvmsPoller();

    bool HasPollList();

    void ResetThrottle();

    void Queue_PollerSend(poller_source_t source);

    void PollSetThrottling(uint8_t sequence_max);

    void PollSetResponseSeparationTime(uint8_t septime);
    void PollSetChannelKeepalive(uint16_t keepalive_seconds);

    // TODO - Work out how to make sure these are protected. Reduce/eliminate mutex time.
    void PollSetPidList(uint8_t defaultbus, const poll_pid_t* plist);

    int PollSingleRequest(uint32_t txid, uint32_t rxid,
                      std::string request, std::string& response,
                      int timeout_ms=3000, uint8_t protocol=ISOTP_STD);
    int PollSingleRequest(uint32_t txid, uint32_t rxid,
                      uint8_t polltype, uint16_t pid, std::string& response,
                      int timeout_ms=3000, uint8_t protocol=ISOTP_STD);

  public:
    static const char *PollerCommand(OvmsPollCommand src);
    static const char *PollerSource(poller_source_t src);
    static const char *PollResultCodeName(int code);
  friend class OvmsPollers;
};

#define VEHICLE_MAXBUSSES 4
class OvmsPollers {
  private:
    OvmsRecMutex      m_poller_mutex;
    OvmsPoller*       m_pollers[VEHICLE_MAXBUSSES];
    OvmsPoller::ParentSignal* m_parent_callback;
    uint8_t           m_poll_state;           // Current poll state
    uint8_t           m_poll_sequence_max;    // Polls allowed to be sent in sequence per time tick (second), default 1, 0 = no limit
    uint8_t           m_poll_fc_septime;      // Flow control separation time for multi frame responses
    uint16_t          m_poll_ch_keepalive;    // Seconds to keep an inactive channel (e.g. VWTP) alive (default: 60)

    QueueHandle_t m_pollqueue;
    TaskHandle_t m_polltask;
    CanFrameCallback  m_poll_txcallback;      // Poller CAN TxCallback

    void PollerTxCallback(const CAN_frame_t* frame, bool success);
    void PollerRxCallback(const CAN_frame_t* frame, bool success);

    void PollerTask();
    static void OvmsPollerTask(void *pvParameters);

    void Queue_PollerFrame(const CAN_frame_t &frame, bool success, bool istx);

    void Queue_Command(OvmsPoller::OvmsPollCommand cmd, uint16_t param = 0);
  public:
    OvmsPollers( OvmsPoller::ParentSignal *parent);
    ~OvmsPollers();

    OvmsPoller *GetPoller(canbus *can, bool force = false );

    void QueuePollerSend(OvmsPoller::poller_source_t src, uint8_t busno = 0 );

    void PollSetPidList(canbus* defbus, const OvmsPoller::poll_pid_t* plist);

    bool HasPollList(canbus* bus = nullptr);

    void CheckStartPollTask();

    void PollSetThrottling(uint8_t sequence_max)
      {
      Queue_Command(OvmsPoller::OvmsPollCommand::Throttle, sequence_max);
      }
    void PollSetResponseSeparationTime(uint8_t septime)
      {
      Queue_Command(OvmsPoller::OvmsPollCommand::ResponseSep, septime);
      }
    void PollSetChannelKeepalive(uint16_t keepalive_seconds)
      {
      Queue_Command(OvmsPoller::OvmsPollCommand::Keepalive, keepalive_seconds);
      }
    // signal poller
    void PollerResetThrottle();

    void PausePolling()
      {
      Queue_Command(OvmsPoller::OvmsPollCommand::Pause);
      }
    void ResumePolling()
      {
      Queue_Command(OvmsPoller::OvmsPollCommand::Resume);
      }
    void PollSetState(uint8_t state);

};

#endif // __VEHICLE_POLLER_H__
