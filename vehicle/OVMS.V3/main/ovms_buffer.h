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

#ifndef __OVMS_BUFFER_H__
#define __OVMS_BUFFER_H__

#include <string>
#include <stdint.h>

class OvmsBuffer
  {
  public:
    OvmsBuffer(size_t size, void* userdata = 0);
    virtual ~OvmsBuffer();

  public:
    size_t Size();
    size_t FreeSpace();
    size_t UsedSpace();

  public:
    void EmptyAll();
    bool Push(uint8_t byte);
    bool Push(uint8_t *byte, size_t count);
    uint8_t Pop();
    size_t Pop(size_t count, uint8_t *dest);
    uint8_t Peek();
    size_t Peek(size_t count, uint8_t *dest);
    void Diagnostics();

  public:
    int HasLine();
    std::string ReadLine();

  public:
    int PollSocket(int sock, long timeoutms);

  public:
    void* m_userdata;

  protected:
    uint8_t *m_buffer;
    int m_head;
    int m_tail;
    size_t m_size;
    size_t m_used;
  };

#endif //#ifndef __OVMS_BUFFER_H__
