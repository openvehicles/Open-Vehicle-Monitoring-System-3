// Copyright 2020 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license as follows:
/*
The MIT License (MIT)

Copyright (c) 2020 Joshua J Baker

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE./
*/

#include <string.h>
#include "glob_match.h"

bool glob_match(const char *pat, const char *str) {
  return match(pat, -1, str, -1, true);
}


// match returns true if str matches pattern. This is a very
// simple wildcard match where '*' matches on any number characters
// and '?' matches on any one character.
//
// pattern:
//   { term }
// term:
// 	 '*'         matches any sequence of non-Separator characters
// 	 '?'         matches any single non-Separator character
// 	 c           matches character c (c != '*', '?')
// 	'\\' c       matches character c (if isglob is false)
bool match(const char *pat, long plen, const char *str, long slen, bool isglob)  {
    if (plen < 0) plen = strlen(pat);
    if (slen < 0) slen = strlen(str);
    while (plen > 0) {
        if ( !isglob && (pat[0] == '\\')) {
            if (plen == 1) return false;
            pat++; plen--; 
        } else if (pat[0] == '*') {
            if (plen == 1) return true;
            if (pat[1] == '*') {
                pat++; plen--;
                continue;
            }
            if (match(pat+1, plen-1, str, slen, isglob)) return true;
            if (slen == 0) return false;
            str++; slen--;
            continue;
        }
        if (slen == 0) return false;
        if (pat[0] != '?' && str[0] != pat[0]) return false;
        pat++; plen--;
        str++; slen--;
    }
    return slen == 0 && plen == 0;
}

