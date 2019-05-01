/**
 * Project:      Open Vehicle Monitor System
 * Module:       RE tools
 * 
 * crtdreplay: CRTD CAN dump format timing replay filter
 * 
 * Note: this filter is meant to replay a CRTD recording into RE tools serve mode
 * with about the same timing as recorded. The filter does not make any attempt at
 * recreating the exact timing.
 * 
 * Usage example:
 *    OVMS# re start
 *    OVMS# re serve format crtd
 *    OVMS# re serve mode simulate
 *    <user@pc> cat some.crtd | crtdreplay | nc ovms.local 3000
 * 
 * Delay between records is limited by default to max 1 second.
 * Overwrite with 'crtdreplay <seconds>', e.g. 'crtdreplay 0.5'.
 * 
 * Build: gcc crtdreplay.c -o crtdreplay
 * 
 * (c) 2019 Michael Balzer <dexter@dexters-web.de>
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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

int main(int argc, char *argv[])
{
  char buf[1000];
  double ref = 0, ts;
  struct timespec slp;
  double maxsleep = 1;

  if (argc >= 2) {
    if (!isdigit(argv[1][0])) {
      fprintf(stderr,
        "%s: CRTD CAN dump format timing replay filter\n"
        "Usage: cat some.crtd | crtdreplay | nc ovms.local 3000\n"
        "Delay between records is limited by default to max 1 second.\n"
        "Overwrite with 'crtdreplay <seconds>', e.g. 'crtdreplay 0.5'.\n",
        argv[0]);
      exit(1);
    }
    maxsleep = atof(argv[1]);
    fprintf(stderr, "maxsleep=%lf\n", maxsleep);
  }

  while (fgets(buf, sizeof buf, stdin)) {
    sscanf(buf, "%lf", &ts);
    if (ref) {
      double diff = ts - ref;
      if (diff > maxsleep) diff = maxsleep;
      slp.tv_sec = (time_t) diff;
      slp.tv_nsec = (diff - slp.tv_sec) * 1e9;
      nanosleep(&slp, NULL);
    }
    ref = ts;
    fputs(buf, stdout);
  }
}
