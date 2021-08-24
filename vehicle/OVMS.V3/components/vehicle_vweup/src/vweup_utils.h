/**
 * Project:      Open Vehicle Monitor System
 * Module:       Vehicle VW e-Up
 * Submodule:    Metrics Utilities
 * 
 * (c) 2021  Michael Balzer <dexter@dexters-web.de>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __VWEUP_UTILS_H__
#define __VWEUP_UTILS_H__

/**
 * SmoothExp: exponential smoothing container with adaptive entry phase for numeric types
 */
template <typename ValType>
class SmoothExp
{
public:
  SmoothExp(int smoothcnt=0, ValType value=0, bool defined=false) {
    Init(value, smoothcnt, defined);
  }

  void Init(int smoothcnt, ValType value=0, bool defined=false) {
    m_value = value;
    m_alpha = 0;
    m_smoothcnt = (smoothcnt > 1) ? smoothcnt : 1;
    m_cnt = (defined) ? m_smoothcnt-1 : 0;
  }

  ValType Add(ValType val) {
    if (m_cnt < m_smoothcnt) {
      m_cnt++;
      m_alpha = 1.0 / m_cnt;
    }
    m_value = m_alpha * val + (1.0 - m_alpha) * m_value;
    return m_value;
  }

  ValType Value() {
    return m_value;
  }

private:
  ValType     m_value;
  ValType     m_alpha;
  int         m_smoothcnt;
  int         m_cnt;
};

#endif // __VWEUP_UTILS_H__
