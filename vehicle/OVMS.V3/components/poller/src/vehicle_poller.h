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

#include "vehicle_common.h"

#include <cstdint>
#include <memory>

// PollSingleRequest specific result codes:
#define POLLSINGLE_OK                   0
#define POLLSINGLE_TIMEOUT              -1
#define POLLSINGLE_TXFAILURE            -2

#define VEHICLE_POLL_TYPE_NONE          0x00

// Number of polling states supported
#define VEHICLE_POLL_NSTATES            4

// A note on "PID" and their sizes here:
//  By "PID" for the service types we mean the part of the request parameters
//  after the service type that is reflected in _every_ valid response to the request.
//  That part is used to validate the response by the poller, if it doesn't match,
//  the response won't be forwarded to the application.
//  Some requests require additional parameters as specified in ISO 14229, but implementations
//  may differ. For example, a 31b8 request on a VW ECU does not necessarily copy the routine
//  ID in the response (e.g. with 0000), so the routine ID isn't part of our "PID" here.

// Utils:
#define POLL_TYPE_HAS_16BIT_PID(type) \
  ((type) == VEHICLE_POLL_TYPE_READDATA || \
   (type) == VEHICLE_POLL_TYPE_READSCALING || \
   (type) == VEHICLE_POLL_TYPE_WRITEDATA || \
   (type) == VEHICLE_POLL_TYPE_IOCONTROL || \
   (type) == VEHICLE_POLL_TYPE_READOXSTEST || \
   (type) == VEHICLE_POLL_TYPE_ZOMBIE_VCU_16)
#define POLL_TYPE_HAS_NO_PID(type) \
  ((type) == VEHICLE_POLL_TYPE_CLEARDTC || \
   (type) == VEHICLE_POLL_TYPE_READMEMORY || \
   (type) == VEHICLE_POLL_TYPE_READ_ERDTC || \
   (type) == VEHICLE_POLL_TYPE_CLEAR_ERDTC || \
   (type) == VEHICLE_POLL_TYPE_READ_DCERDTC || \
   (type) == VEHICLE_POLL_TYPE_READ_PERMDTC || \
   (type) == VEHICLE_POLL_TYPE_OBDII_18)
#define POLL_TYPE_HAS_8BIT_PID(type) \
  (!POLL_TYPE_HAS_NO_PID(type) && !POLL_TYPE_HAS_16BIT_PID(type))

// OBD/UDS Negative Response Code
#define UDS_RESP_TYPE_NRC               0x7F  // see ISO 14229 Annex A.1
#define UDS_RESP_NRC_RCRRP              0x78  // … requestCorrectlyReceived-ResponsePending

// Poll list PID xargs utility (see info above):
#define POLL_PID_DATA(pid, datastring) \
  {.xargs={ (pid), POLL_TXDATA, sizeof(datastring)-1, reinterpret_cast<const uint8_t*>(datastring) }}


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
  uint32_t                lastused;         // Timestamp of last channel access (uptime in ms)
  } vwtp_channel_t;

class OvmsPollers;

class OvmsPoller : public InternalRamAllocated {
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
      // Used for DBC Decode
      CAN_frame_format_t format; ///< Incoming frame format used for DBC Decode
      uint8_t *raw_data;       ///< Raw data in response packet for DBC Decode
      uint8_t raw_data_len;    ///< Length of raw data in response packet.
      } poll_job_t;

    const uint32_t max_ticker = 3600;
    const uint32_t init_ticker = 9999;

    typedef enum : uint8_t { Primary, Secondary, Successful, OnceOff } poller_source_t;

// Macro for poll_pid_t termination
#define POLL_LIST_END                   { 0, 0, 0x00, 0x00, { 0, 0, 0 }, 0, 0 }

    // Interface for polls generated from the Vehicle class.
    class VehicleSignal
      {
      public:
        virtual ~VehicleSignal() { }
        // Signals for vehicle
        virtual void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length);
        virtual void IncomingPollError(const OvmsPoller::poll_job_t &job, uint16_t code);
        virtual void IncomingPollTxCallback(const OvmsPoller::poll_job_t &job, bool success);
        virtual bool Ready() const = 0;
      };
    enum class OvmsNextPollResult
      {
      NotReady,   ///< Not ready - ignore and move on.
      StillAtEnd, ///< Still at end of list.
      FoundEntry, ///< Found an entry
      ReachedEnd, ///< Reached the end.
      Ignore      ///< Ignore this (don't move to next poller index)
      };

    enum class SeriesStatus
      {
      Next,         ///< Move to Next Poll Result
      RemoveNext,   ///< Remove Series from list and keep going.
      RemoveRestart ///< Remove series and resume from start
      };

    enum class ResetMode {
      PollReset, ///< Reset for poll start
      LoopReset  ///< Reset for retry loop.
    };

    /// Class that defines a series list.
    class PollSeriesEntry
      {
      public:
        /// Set the parent poller
        virtual void SetParentPoller(OvmsPoller *poller) = 0;

        /// Destructed by pointer.
        virtual ~PollSeriesEntry() { }

        /** Move list to start.
          * @arg  mode Specify Resetting for Poll-Start or for Retry
          */
        virtual void ResetList(ResetMode mode) = 0;

        /// Find the next poll entry.
        virtual OvmsPoller::OvmsNextPollResult NextPollEntry(poll_pid_t &entry, uint8_t mybus, uint32_t pollticker, uint8_t pollstate) = 0;
        /// Process an incoming packet.
        virtual void IncomingPacket(const OvmsPoller::poll_job_t& job, uint8_t* data, uint8_t length) = 0;
        /// Process An Error
        virtual void IncomingError(const OvmsPoller::poll_job_t& job, uint16_t code) = 0;

        /// Send on an imcoming TX reply
        virtual void IncomingTxReply(const OvmsPoller::poll_job_t& job, bool success);

        /// Called when run is finished to determine what happens next.
        virtual SeriesStatus FinishRun() = 0;

        /// Called before removing from list.
        virtual void Removing() = 0;

        /// Return true if this series has any entries
        virtual bool HasPollList() const = 0;

        /** Return true if this series has entries to retry/redo.
          This should mean that the list has been finished at least once,
          but also that the remaining todo don't NEED to be done before moving on.
         */
        virtual bool HasRepeat() const = 0;
        /** Return true if this series is ok to run.
         */
        virtual bool Ready() const;
      };

    /// Named element in the series double-linked list.
    typedef struct poll_series_st
      {
      std::string name;
      std::shared_ptr<PollSeriesEntry> series;

      bool is_blocking;
      OvmsPoller::OvmsNextPollResult last_status;
      uint32_t last_status_monotonic;

      struct poll_series_st *prev, *next;
      } poll_series_t;

    /** A collection of Poll Series entries.
     *  Also behaves as the iterator controlling which is the current series/entry.
     *  No thread safety included, so relies on the mutex blocking calls to it.
     */
    class PollSeriesList
      {
      private:
        // Each end of the list.
        poll_series_t *m_first, *m_last;
        // Current poll entry.
        poll_series_t *m_iter;

        // Remove an item out of the linked list.
        void Remove( poll_series_t *iter);
        // Insert an item into the linked list (before == null means the end)
        void InsertBefore( poll_series_t *iter, poll_series_t *before);
      public:
        PollSeriesList();
        ~PollSeriesList();

        // List functions:

        /** Set/Add the named entry to the series specified.
         * @param name Name of the entry.
         * @param series Shared pointer to the series
         * @param blocking This is a blocking poll entry.
         */
        void SetEntry(const std::string &name, std::shared_ptr<PollSeriesEntry> series, bool blocking = false);
        /// Remove the named entry from the list.
        void RemoveEntry( const std::string &name, bool starts_with = false);
        /// Are there any entries/lists
        bool IsEmpty();
        /// Clear the list.
        void Clear();

        // Processing functions.

        /// Process an incoming packet.
        void IncomingPacket(const OvmsPoller::poll_job_t& job, uint8_t* data, uint8_t length);

        /// Process An Error
        void IncomingError(const OvmsPoller::poll_job_t& job, uint16_t code);

        /// Send on an imcoming TX reply
        void IncomingTxReply(const OvmsPoller::poll_job_t& job, bool success);

        /// Reset the list to beging processing
        void RestartPoll(ResetMode mode);

        /// Get the next poll entry
        OvmsPoller::OvmsNextPollResult NextPollEntry(poll_pid_t &entry, uint8_t mybus, uint32_t pollticker, uint8_t pollstate);

        /// Are there any lists that have active entries?
        bool HasPollList() const;

        /** Return true if the current item is marked as blocking.
        */
        bool PollIsBlocking()
          {
          return (m_iter != nullptr) && (m_iter->is_blocking) && (m_iter->series != nullptr);
          }

        /** Return true if this series has entries to retry/redo.
          This should mean that the list has been finished at least once,
          but also that the remaining todo don't NEED to be done before moving on.
         */
        bool HasRepeat() const;

        /** CLI Status.
         */
        void Status(int verbosity, OvmsWriter* writer);
      };

    /** Standard series.
      * The main functionality of processing an array of poll_pid_t based on the pollstate and ticker.
      */
    class StandardPollSeries : public PollSeriesEntry
      {
      protected:
        OvmsPoller *m_poller;
        uint16_t m_state_offset; // Offset of 'state' entries.

        uint8_t m_defaultbus;

        const poll_pid_t* m_poll_plist; // Head of poll list
        const poll_pid_t* m_poll_plcur; // Poll list loop cursor

      public:
        StandardPollSeries(OvmsPoller *poller, uint16_t stateoffset = 0);

        void SetParentPoller(OvmsPoller *poller) override;

        /// Set the PID list and default bus.
        void PollSetPidList(uint8_t defaultbus, const poll_pid_t* plist);

        // Move list to start.
        void ResetList(ResetMode mode) override;

        // Find the next poll entry.
        OvmsPoller::OvmsNextPollResult NextPollEntry(poll_pid_t &entry, uint8_t mybus, uint32_t pollticker, uint8_t pollstate) override;

        // Process an incoming packet. (pass through to m_poller)
        void IncomingPacket(const OvmsPoller::poll_job_t& job, uint8_t* data, uint8_t length) override;

        // Process An Error. (pass through to m_poller)
        void IncomingError(const OvmsPoller::poll_job_t& job, uint16_t code) override;

        // Called when run is finished to determine what happens next.
        SeriesStatus FinishRun() override;

        void Removing() override;

        bool HasPollList() const override;

        bool HasRepeat() const override;
      };

    // Standard Vehicle Poll series passing through various responses.
    class StandardVehiclePollSeries : public StandardPollSeries
      {
      private:
        VehicleSignal *m_signal;
      public:
        StandardVehiclePollSeries(OvmsPoller *poller, VehicleSignal *signal, uint16_t stateoffset = 0);

        // Process an incoming packet.
        void IncomingPacket(const OvmsPoller::poll_job_t& job, uint8_t* data, uint8_t length) override;

        // Process An Error
        void IncomingError(const OvmsPoller::poll_job_t& job, uint16_t code) override;

        // Send on an imcoming TX reply
        void IncomingTxReply(const OvmsPoller::poll_job_t& job, bool success) override;

        // Return true if this series is ok to run.
        bool Ready() const override;
      };

    typedef std::function<void(uint16_t type, uint32_t module_sent, uint32_t module_rec, uint16_t pid, CAN_frame_format_t format, const std::string &data)> poll_success_func;
    typedef std::function<void(uint16_t type, uint32_t module_sent, uint32_t module_rec, uint16_t pid, int errorcode)> poll_fail_func;

    /** Standard Poll Series that assembles packets to complete results.
      */
    class StandardPacketPollSeries : public StandardPollSeries
      {
      protected:
        std::string m_data;
        int m_repeat_max, m_repeat_count;
        poll_success_func m_success;
        poll_fail_func m_fail;
        CAN_frame_format_t m_format;
        bool m_pack_raw_data;
      public:
        StandardPacketPollSeries( OvmsPoller *poller, int repeat_max, poll_success_func success, poll_fail_func fail, uint16_t stateoffset = 0, bool pack_raw_data = false);

        // Move list to start.
        void ResetList(ResetMode mode) override;

        // Process an incoming packet.
        void IncomingPacket(const OvmsPoller::poll_job_t& job, uint8_t* data, uint8_t length) override;

        // Process An Error
        void IncomingError(const OvmsPoller::poll_job_t& job, uint16_t code) override;

        // Return true if this series has entries to retry/redo.
        bool HasRepeat() const override;
      };

    /** Base for Once off Poll series.
     */
    class OnceOffPollBase : public PollSeriesEntry
      {
      protected:
        enum class status_t {
          Init, ///< Initialised, entry can be sent
          Sent, ///< Entry is sent. (Next mode is 'finished');
          Stopping, ///< Success or reached retries.
          Stopped, ///< Has been stopped.
          Retry, /// Ready for retry
          RetryInit, /// Reset for retry
          RetrySent, /// Retry has sent
          };
        status_t m_sent;
        std::string m_poll_data; // Request buffer data
        poll_pid_t m_poll; // Poll Entry
        std::string *m_poll_rxbuf;    // … response buffer
        int         *m_poll_rxerr;    // … response error code (NRC) / TX failure code
        int16_t     m_retry_fail;
        CAN_frame_format_t m_format;

        // Called when the one-off is finished (for semaphore etc).
        // Called with NULL bus when on Removing
        virtual void Done(bool success);

        void SetPollPid( uint32_t txid, uint32_t rxid, const std::string &request, uint8_t protocol=ISOTP_STD, uint8_t pollbus = 0, uint16_t polltime = 1);
        void SetPollPid( uint32_t txid, uint32_t rxid, uint8_t polltype, uint16_t pid,  uint8_t protocol, const std::string &request, uint8_t pollbus = 0, uint16_t polltime = 1);
      public:
        OnceOffPollBase(const poll_pid_t &pollentry, std::string *rxbuf, int *rxerr, uint8_t retry_fail = 0);
        OnceOffPollBase(std::string *rxbuf, int *rxerr, uint8_t retry_fail = 0);

        // Move list to start.
        void ResetList(ResetMode mode) override;

        void SetParentPoller(OvmsPoller *poller) override;

        // Find the next poll entry.
        OvmsPoller::OvmsNextPollResult NextPollEntry(poll_pid_t &entry,  uint8_t mybus, uint32_t pollticker, uint8_t pollstate) override;

        // Process an incoming packet.
        void IncomingPacket(const OvmsPoller::poll_job_t& job, uint8_t* data, uint8_t length) override;

        // Process An Error
        void IncomingError(const OvmsPoller::poll_job_t& job, uint16_t code) override;

        bool HasPollList() const override;

        bool HasRepeat() const override;
      };

  private:
    /** Blocking once off poll entry used by PollSingleRequest.
     *  Signals Semaphore when done.  Used for once-off blocking calls.
     *  Because this is used poentially passing in stack variables, I'm going to
     *  leave this as private as it really needs to have Finished() called before
     *  the variables go out of scope.
     */
    class BlockingOnceOffPoll : public OnceOffPollBase
      {
      protected:
        OvmsSemaphore *m_poll_rxdone;   // … response done (ok/error)
        void Done(bool success) override;
      public:
        BlockingOnceOffPoll(const poll_pid_t &pollentry, std::string *rxbuf, int *rxerr, OvmsSemaphore *rxdone );

        // Called To null out the call-back pointers (particularly before they go out of scope).
        void Finished();
      public:
        // Called when run is finished to determine what happens next.
        SeriesStatus FinishRun() override;

        void Removing() override;
      };
  public:
   /** Once off poll entry with buffer and asynchronous result call-backs.
    */
   class OnceOffPoll : public OnceOffPollBase
      {
      protected:
        poll_success_func m_success;
        poll_fail_func m_fail;
        std::string m_data;
        int m_error;
        bool m_result_sent;

        void Done(bool success) override;
      public:
        OnceOffPoll(poll_success_func success, poll_fail_func fail,
            uint32_t txid, uint32_t rxid, const std::string &request, uint8_t protocol=ISOTP_STD, uint8_t pollbus = 0, uint8_t retry_fail = 0);
        OnceOffPoll(poll_success_func success, poll_fail_func fail,
            uint32_t txid, uint32_t rxid, uint8_t polltype, uint16_t pid,  uint8_t protocol=ISOTP_STD, uint8_t pollbus = 0, uint8_t retry_fail = 0);
        OnceOffPoll(poll_success_func success, poll_fail_func fail,
            uint32_t txid, uint32_t rxid, uint8_t polltype, uint16_t pid,  uint8_t protocol, const std::string &request, uint8_t pollbus = 0, uint8_t retry_fail = 0);

        // Called when run is finished to determine what happens next.
        SeriesStatus FinishRun() override;

        void Removing() override;
      };

  protected:
    OvmsPollers*      m_parent;

    mutable OvmsRecMutex m_poll_mutex;           // Concurrency protection for recursive calls
    uint8_t           m_poll_state;           // Current poll state
  private:
    // Poll state

    PollSeriesList    m_polls;  // User poll entries list.
    int               m_poll_repeat_count; // 'Extra repeats' for polling.
    std::shared_ptr<StandardPollSeries> m_poll_series;

    const int         max_poll_repeat = 5; // Maximum # of poll-repeats.
    uint32_t          m_poll_sent_last;

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
    uint16_t          m_poll_between_success;
    bool              m_poll_ticked;
    bool              m_poll_run_finished;

  private:
    mutable OvmsRecMutex m_poll_single_mutex;    // PollSingleRequest() concurrency protection

  protected:
    vwtp_channel_t    m_poll_vwtp;            // VWTP channel state

  protected:

    // Signals for vehicle
    void PollRunFinished();

    void IncomingPollError(const OvmsPoller::poll_job_t &job, uint16_t code)
      {
      m_polls.IncomingError(job, code);
      }
    void IncomingPollTxCallback(const OvmsPoller::poll_job_t &job, bool success);

    bool Ready() const;
  private:
    void PollerSend(poller_source_t source);

    void PollerISOTPStart(bool fromTicker);
    bool PollerISOTPReceive(CAN_frame_t* frame, uint32_t msgid);

    void PollerVWTPStart(bool fromTicker);
    bool PollerVWTPReceive(CAN_frame_t* frame, uint32_t msgid);
    void PollerVWTPEnter(vwtp_channelstate_t state);
    void PollerVWTPTicker();
    void PollerVWTPTxCallback(const CAN_frame_t* frame, bool success);

    static void DoPollerSendSuccess( void * pvParameter1, uint32_t ulParameter2 );

  public:
    bool HasBus(canbus* bus) const { return bus == m_poll.bus;}
    uint8_t CanBusNo() const { return m_poll.bus_no;}

    uint8_t PollState() const { return m_poll_state;}

  protected:

    // Poll entry manipulation: Must be called under lock of m_poll_mutex

    void ResetPollEntry(bool force = false);
    void PollerNextTick(poller_source_t source);

    bool Incoming(CAN_frame_t &frame, bool success);
    void Outgoing(const CAN_frame_t &frame, bool success);

    // Check for throttling.
    bool CanPoll() const;

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
      Keepalive,
      SuccessSep,
      Shutdown,
      ResetTimer
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
      uint32_t poll_ticker;
    } poll_source_entry_t;
    typedef struct {
      uint8_t new_state;
      canbus* bus; // optional
    } poll_state_entry_t;

    typedef struct {
      OvmsPollEntryType entry_type;
      union {
        poll_source_entry_t entry_Poll;
        poll_frame_entry_t entry_FrameRxTx;
        poll_command_entry_t entry_Command;
        poll_state_entry_t entry_PollState;
      };
    } poll_queue_entry_t;

    void Do_PollSetState(uint8_t state);

    int DoPollSingleRequest(
        const OvmsPoller::poll_pid_t &poll, std::string& response, int timeout_ms);
  public:
    OvmsPoller(canbus* can, uint8_t can_number, OvmsPollers *parent,
      const CanFrameCallback &polltxcallback);
    ~OvmsPoller();

    bool HasPollList() const;

    void ClearPollList();

    void ResetThrottle();

    void Queue_PollerSend(poller_source_t source);
    void Queue_PollerSendSuccess();
    void PollerSucceededPollNext();

    void PollSetThrottling(uint8_t sequence_max);

    void PollSetResponseSeparationTime(uint8_t septime);
    void PollSetChannelKeepalive(uint16_t keepalive_seconds);
    void PollSetTimeBetweenSuccess(uint16_t time_between_ms);

    // TODO - Work out how to make sure these are protected. Reduce/eliminate mutex time.
    void PollSetPidList(uint8_t defaultbus, const poll_pid_t* plist, VehicleSignal *signal);

    int PollSingleRequest(uint32_t txid, uint32_t rxid,
                      std::string request, std::string& response,
                      int timeout_ms=3000, uint8_t protocol=ISOTP_STD);
    int PollSingleRequest(uint32_t txid, uint32_t rxid,
                      uint8_t polltype, uint16_t pid, std::string& response,
                      int timeout_ms=3000, uint8_t protocol=ISOTP_STD);
    int PollSingleRequest(uint32_t txid, uint32_t rxid,
                      uint8_t polltype, uint16_t pid, const std::string &payload, std::string& response,
                      int timeout_ms=3000, uint8_t protocol=ISOTP_STD);

    bool PollRequest(const std::string &name, const std::shared_ptr<PollSeriesEntry> &series, int timeout_ms = 5000);
    void RemovePollRequest(const std::string &name);
    void RemovePollRequestStarting(const std::string &name);

  public:
    static const char *PollerCommand(OvmsPollCommand src, bool brief=false);
    static const char *PollerSource(poller_source_t src);
    static const char *PollResultCodeName(int code);

   void PollerStatus(int verbosity, OvmsWriter* writer);
  friend class OvmsPollers;
};

#define VEHICLE_MAXBUSSES 4
class OvmsPollers : public InternalRamAllocated {
  public:
    enum class BusPoweroff {None, System, Vehicle};
  private:
    typedef  struct {
      canbus* can;
      BusPoweroff auto_poweroff;
    } bus_info_t;
    bus_info_t        m_canbusses[VEHICLE_MAXBUSSES];
    mutable OvmsRecMutex m_poller_mutex;
    OvmsPoller*       m_pollers[VEHICLE_MAXBUSSES];
    uint8_t           m_poll_state;           // Current poll state
    uint8_t           m_poll_sequence_max;    // Polls allowed to be sent in sequence per time tick (second), default 1, 0 = no limit
    uint8_t           m_poll_fc_septime;      // Flow control separation time for multi frame responses
    uint16_t          m_poll_ch_keepalive;    // Seconds to keep an inactive channel (e.g. VWTP) alive (default: 60)
    uint16_t          m_poll_between_success;
    uint32_t          m_poll_last;

    _Alignas(32 / CHAR_BIT)
    QueueHandle_t     m_pollqueue;
    TaskHandle_t      m_polltask;
    CanFrameCallback  m_poll_txcallback;      // Poller CAN TxCallback

    TimerHandle_t     m_timer_poller;
    int32_t           m_poll_subticker;       // Subticker count for polling
    uint16_t          m_poll_tick_ms;         // Tick length in ms.
    uint8_t           m_poll_tick_secondary;  // Number of secondary poll subticks per primary / zero

    bool              m_shut_down;
    bool              m_ready;
    bool              m_paused;
    bool              m_user_paused;
    typedef enum {trace_Off = 0x00, trace_Poller = 0x1, trace_TXRX = 0x2, trace_Times = 0x4, trace_Duktape = 0x8, trace_All= 0x3} tracetype_t;
    uint8_t           m_trace;                // Current Trace flags.
    uint32_t          m_overflow_count[2];    // Keep track of overflows.
                                              //
    OvmsMutex         m_filter_mutex;
    canfilter         m_filter;
    bool              m_filtered;

    void PollerTxCallback(const CAN_frame_t* frame, bool success);
    void PollerRxCallback(const CAN_frame_t* frame, bool success);

    void PollerTask();
    static void OvmsPollerTask(void *pvParameters);

    void Queue_PollerFrame(const CAN_frame_t &frame, bool success, bool istx);

    void Queue_Command(OvmsPoller::OvmsPollCommand cmd, uint16_t param = 0);
    static void vehicle_poller_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_pause_on(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_pause_off(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void vehicle_poller_trace(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void poller_times(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    // OvmsPoller Object

    // OvmsPoller.RegisterBus(bus, mode, speed [,dbcfile])
    static duk_ret_t DukOvmsPollerRegisterBus(duk_context *ctx);
    // OvmsPoller.PowerDownBus(bus)
    static duk_ret_t DukOvmsPollerPowerDownBus(duk_context *ctx);
    // OvmsPoller.GetPaused
    static duk_ret_t DukOvmsPollerPaused(duk_context *ctx);
    // OvmsPoller.GetUserPaused
    static duk_ret_t DukOvmsPollerUserPaused(duk_context *ctx);
    // OvmsPoller.Pause
    static duk_ret_t DukOvmsPollerPause(duk_context *ctx);
    // OvmsPoller.Resume
    static duk_ret_t DukOvmsPollerResume(duk_context *ctx);

    // OvmsPoller.Trace
    static duk_ret_t DukOvmsPollerSetTrace(duk_context *ctx);
    // OvmsPoller.GetTraceStatus
    static duk_ret_t DukOvmsPollerGetTrace(duk_context *ctx);

    // OvmsPoller.Times Sub-object
    // OvmsPoller.Times.GetEnabled
    static duk_ret_t DukOvmsPollerTimesGetStarted(duk_context *ctx);
    // OvmsPoller.Times.Start Times
    static duk_ret_t DukOvmsPollerTimesStart(duk_context *ctx);
    // OvmsPoller.Times.Start Times
    static duk_ret_t DukOvmsPollerTimesStop(duk_context *ctx);
    // OvmsPoller.Times.Reset
    static duk_ret_t DukOvmsPollerTimesReset(duk_context *ctx);
    // OvmsPoller.Times.GetStatus
    static duk_ret_t DukOvmsPollerTimesGetStatus(duk_context *ctx);

    // OvmsPoller.Poll.Add
    static duk_ret_t DukOvmsPollerPollAdd(duk_context *ctx);
    // OvmsPoller.Poll.Remove
    static duk_ret_t DukOvmsPollerPollRemove(duk_context *ctx);
    // OvmsPoller.Poll.GetState
    static duk_ret_t DukOvmsPollerPollGetState(duk_context *ctx);
    // OvmsPoller.Poll.SetState
    static duk_ret_t DukOvmsPollerPollSetState(duk_context *ctx);
    // OvmsPoller.Poll.SetTrace
    static duk_ret_t DukOvmsPollerPollSetTrace(duk_context *ctx);

    // OvmsPoller.Poll.Request
    static duk_ret_t DukOvmsPollerPollRequest(duk_context *ctx);

  public:
    static void Duk_GetRequestFromObject( duk_context *ctx, duk_idx_t obj_idx,
        std::string default_bus, bool allow_bus,
        canbus*& device, uint32_t &txid, uint32_t &rxid, uint8_t &protocol,
        uint16_t &type, uint16_t &pid, std::string &request,
        int &timeout, int &error, std::string &errordesc);
  private:
#endif

    typedef struct {
      std::string desc;
      float avg_n, avg_utlzn_ms, max_time, avg_time, max_val;
    } times_trace_elt_t;
    typedef struct {
      std::list<times_trace_elt_t> items;
      float tot_n, tot_utlzn_ms, tot_time;
    } times_trace_t;

    void PollerTimesReset();
    void PollerStatus(int verbosity, OvmsWriter* writer);
    void SetUserPauseStatus(bool paused, int verbosity, OvmsWriter* writer);
    bool LoadTimesTrace( metric_unit_t ratio_unit, times_trace_t &trace);
  public:
    bool PollerTimesTrace( OvmsWriter* writer);
    bool IsTracingTimes() const { return (m_trace & trace_Times) != 0; }
    typedef std::function<void(canbus*, void *)> PollCallback;
    typedef std::function<void(const CAN_frame_t &)> FrameCallback;
    bool HasTrace( tracetype_t trace) const { return (m_trace & trace) != 0; }
    // CAN RX filtering.
    void ClearFilters();
    void AddFilter(uint8_t bus, uint32_t id_from=0, uint32_t id_to=UINT32_MAX);
    void AddFilter(const char* filterstring);
    bool RemoveFilter(uint8_t bus, uint32_t id_from=0, uint32_t id_to=UINT32_MAX);
  private:
    ovms_callback_register_t<PollCallback> m_runfinished_callback, m_pollstateticker_callback;
    ovms_callback_register_t<FrameCallback> m_framerx_callback;

    // Key for the poller time logging.
    typedef struct poller_key_st{
      OvmsPoller::OvmsPollEntryType entry_type;
      union {
        uint32_t Frame_MsgId;
        OvmsPoller::poller_source_t Poll_src;
        OvmsPoller::OvmsPollCommand Command_cmd;
      };
      uint8_t busnumber;
      // Constructor to convert from the queue entry to the key.
      poller_key_st( const OvmsPoller::poll_queue_entry_t &entry);
    } poller_key_t;
    static const uint32_t average_sep_s = 10;
    static const uint32_t average_sep_mic_s = average_sep_s * 1000000;//10s
    // Timer data for poller time logging.
    typedef struct average_value_st {
      uint64_t next_end;
      average_accum_util_t<uint16_t, 8> avg_n;
      uint32_t max_time;
      average_accum_util_t<uint32_t, 8> avg_utlzn;
      uint16_t max_val;
      average_util_t<uint32_t, 16> avg_time;

      average_value_st()
        : next_end(0), max_time(0), max_val(0)
        {
        }

      void reset()
        {
        paused();
        avg_n.reset();
        avg_utlzn.reset();
        avg_time.reset();
        max_time = 0;
        max_val = 0;
        }
      void paused() { next_end = 0;}
      void add_time(uint32_t time_spent, uint64_t time_added);
      void catchup(uint64_t time_added);
    } average_value_t;

    struct poller_key_less_t {
      bool operator()(const poller_key_t &lhs, const poller_key_t &rhs) const;
    };
    // Store for timing for different packet types.
    std::map<poller_key_t, average_value_t, poller_key_less_t> m_poll_time_stats;
    OvmsMutex m_stats_mutex;

  public:
    void RegisterRunFinished(const std::string &name, PollCallback fn) { m_runfinished_callback.Register(name, fn);}
    void DeregisterRunFinished(const std::string &name) { m_runfinished_callback.Deregister(name);}
    void RegisterPollStateTicker(const std::string &name, PollCallback fn) { m_pollstateticker_callback.Register(name, fn);}
    void DeregisterPollStateTicker(const std::string &name) { m_pollstateticker_callback.Deregister(name);}

    void RegisterFrameRx(const std::string &name, FrameCallback fn) {
      m_framerx_callback.Register(name, fn);
      CheckStartPollTask(true);
    }
    void DeregisterFrameRx(const std::string &name) { m_framerx_callback.Deregister(name);}
  private:
    void PollRunFinished(canbus *bus)
      {
      m_runfinished_callback.Call(
        [bus](const std::string &name, const PollCallback &cb)
          {
          cb(bus, nullptr);
          });
      }
    void PollerStateTicker(canbus *bus)
      {
      m_pollstateticker_callback.Call(
        [bus](const std::string &name, const PollCallback &cb)
          {
          cb(bus, nullptr);
          });
      }
    void PollerFrameRx(CAN_frame_t &frame)
      {
      m_framerx_callback.Call(
        [frame](const std::string &name, FrameCallback cb)
          {
          cb(frame);
          });
      }

    void Ticker1(std::string event, void* data);
    void Ticker1_Shutdown(std::string event, void* data);
    void EventSystemShuttingDown(std::string event, void* data);
    void ConfigChanged(std::string event, void* data);
    void LoadPollerTimerConfig();

    void VehicleOn(std::string event, void* data);
    void VehicleChargeStart(std::string event, void* data);
    void VehicleOff(std::string event, void* data);
    void VehicleChargeStop(std::string event, void* data);

    void NotifyPollerTrace();

  public:
    OvmsPollers();
    ~OvmsPollers();

    void StartingUp();
    void ShuttingDown();
    void ShuttingDownVehicle();

    OvmsPoller *GetPoller(canbus *can, bool force = false );

    void PollSetTicker(uint16_t tick_time_ms, uint8_t secondary_ticks);

    void QueuePollerSend(OvmsPoller::poller_source_t src, uint8_t busno = 0 , uint32_t pollticker = 0);

    void PollSetPidList(canbus* defbus, const OvmsPoller::poll_pid_t* plist, OvmsPoller::VehicleSignal *signal);

    bool PollRequest(canbus* defbus, const std::string &name, const std::shared_ptr<OvmsPoller::PollSeriesEntry> &series, int timeout_ms = 5000 );

    void PollRemove(canbus* defbus, const std::string &name);

    bool HasPollList(canbus* bus = nullptr);

    void CheckStartPollTask( bool force = false);

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
    void PollSetTimeBetweenSuccess(uint16_t time_between_ms)
      {
      Queue_Command(OvmsPoller::OvmsPollCommand::SuccessSep, time_between_ms);
      }
    // signal poller
    void PollerResetThrottle();

    void VehiclePollTicker();

    void PausePolling(bool user_poll = false)
      {
      Queue_Command(OvmsPoller::OvmsPollCommand::Pause, (int)user_poll );
      }
    void ResumePolling(bool user_poll = false)
      {
      Queue_Command(OvmsPoller::OvmsPollCommand::Resume, (int)user_poll);
      }
    bool IsUserPaused() const
      {
      return m_user_paused;
      }
    bool IsPaused() const
      {
      return m_paused;
      }
    void PollSetState(uint8_t state, canbus* bus = nullptr);
    uint8_t PollState() const { return m_poll_state;}

    uint32_t LastPollCmdReceived() const
      {
      return m_poll_last;
      }
    uint8_t GetBusNo(canbus* bus) const;
    canbus* GetBus(uint8_t busno) const;
    canbus* RegisterCanBus(int busno, CAN_mode_t mode, CAN_speed_t speed, dbcfile* dbcfile, BusPoweroff autoPower);

    esp_err_t RegisterCanBus(int busno, CAN_mode_t mode, CAN_speed_t speed, dbcfile* dbcfile, BusPoweroff autoPower, canbus*& bus,int verbosity, OvmsWriter* writer );

    void PowerDownCanBus(int busno);
    bool HasPollTask() const
      {
      return (Atomic_Get(m_polltask) != nullptr);
      }
    bool Ready() const { return m_ready;}
    void Ready(bool ready) { m_ready = ready;}

    // AutoInit stub for startup.
    void AutoInit() { Ready(true); };

    friend class OvmsPoller;
    friend class DukTapePoller;

};
extern OvmsPollers MyPollers;

#endif // __VEHICLE_POLLER_H__
