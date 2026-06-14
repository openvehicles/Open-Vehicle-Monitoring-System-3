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

#ifndef __OVMS_MODULE_H__

class OvmsWriter;

extern void AddTaskToMap(TaskHandle_t task);

/**
 * module_check_heap_integrity: perform heap integrity check, output results to memory buffer (0-terminated)
 * 
 * Note: the buffer will only contain details if heapok == false.
 * 
 * @param buf           -- buffer address
 * @param size          -- buffer capacity
 * @return heapok       -- true = heap integrity valid
 */
extern bool module_check_heap_integrity(char* buf, size_t size);

/**
 * module_check_heap_integrity: perform heap integrity check, output results to OvmsWriter
 * 
 * Note: will additionally output a "heap OK" info if the heap is valid.
 * 
 * @param verbosity     -- channel capacity
 * @param OvmsWriter    -- channel
 * @return heapok       -- true = heap integrity valid
 */
extern bool module_check_heap_integrity(int verbosity, OvmsWriter* writer);

/**
 * module_check_heap_alert: check for and send one-off alert notification on heap corruption
 * 
 *    To enable the check every 5 minutes, set config "module" "debug.heap" to "yes".
 * 
 *    To add custom checks, call from your code, or register event scripts as needed.
 *    Example: perform heap integrity check when the server V2 gets stopped:
 *      vfs echo "module check alert" /store/events/server.v2.stopped/90-checkheap
 * 
 * @param verbosity     -- optional: channel capacity (default 0)
 * @param OvmsWriter    -- optional: channel (default NULL)
 * @return heapok       -- false = heap corrupted/full
 */
extern bool module_check_heap_alert(int verbosity=0, OvmsWriter* writer=NULL);

#define __OVMS_MODULE_H__

#endif //#ifndef __OVMS_MODULE_H__
