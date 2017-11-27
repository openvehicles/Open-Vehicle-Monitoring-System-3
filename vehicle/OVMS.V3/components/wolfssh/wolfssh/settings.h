/* settings.h
 *
 * Copyright (C) 2014-2016 wolfSSL Inc.
 *
 * This file is part of wolfSSH.
 *
 * wolfSSH is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfSSH is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wolfSSH.  If not, see <http://www.gnu.org/licenses/>.
 */


/*
 * The settings header provides a method for developers to provide the
 * internal preprocessor defines that the configure script provides to
 * environments that do not use the configure script.
 */


#pragma once

#include <wolfssh/visibility.h>

#ifdef __cplusplus
extern "C" {
#endif


/* user settings here */
#ifndef WMALLOC_USER
    #define USE_WOLFSSH_MEMORY  /* default memory handlers */
#endif /* WMALLOC_USER */


#ifdef __cplusplus
}
#endif

