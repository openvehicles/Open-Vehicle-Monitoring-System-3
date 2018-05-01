/* Compare strings while treating digits characters numerically.
   2    Copyright (C) 1997-2016 Free Software Foundation, Inc.
   3    This file is part of the GNU C Library.
   4    Contributed by Jean-François Bignolles <bignolle@ecoledoc.ibp.fr>, 1997.
   5
   6    The GNU C Library is free software; you can redistribute it and/or
   7    modify it under the terms of the GNU Lesser General Public
   8    License as published by the Free Software Foundation; either
   9    version 2.1 of the License, or (at your option) any later version.
  10
  11    The GNU C Library is distributed in the hope that it will be useful,
  12    but WITHOUT ANY WARRANTY; without even the implied warranty of
  13    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  14    Lesser General Public License for more details.
  15
  16    You should have received a copy of the GNU Lesser General Public
  17    License along with the GNU C Library; if not, see
  18    <http://www.gnu.org/licenses/>.  */

#ifndef __STRVERSCMP_H__
#define __STRVERSCMP_H__

#ifdef __cplusplus
extern "C" {
#endif

int strverscmp (const char *s1, const char *s2);

#ifdef __cplusplus
}
#endif

#endif //#ifndef __STRVERSCMP_H__
