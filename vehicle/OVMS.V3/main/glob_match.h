// Copyright 2020 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#pragma once

#include <stdbool.h>

bool glob_match(const char* pattern, const char* target);
bool match(const char *pat, long plen, const char *str, long slen, bool isglob);
