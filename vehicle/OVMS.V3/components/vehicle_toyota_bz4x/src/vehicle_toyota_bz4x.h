/*
    Project:       Open Vehicle Monitor System
    Module:        Vehicle Toyota bZ4X
    Date:          27th May 2023

   (C) 2023       Jerry Kezar <solterra@kezarnet.com>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

#ifndef __VEHICLE_TOYOTA_BZ4X_H__
#define __VEHICLE_TOYOTA_BZ4X_H__

#include "../../vehicle_toyota_etnga/src/vehicle_toyota_etnga.h"  // Include the Toyota ETNGA vehicle module header

class OvmsVehicleToyotaBz4x : public OvmsVehicleToyotaETNGA
  {
  public:
    OvmsVehicleToyotaBz4x();  // Constructor for the Toyota Bz4x vehicle module
    ~OvmsVehicleToyotaBz4x();  // Destructor for the Toyota Bz4x vehicle module
    static constexpr const char* TAG = "v-toybz4x";

  };

#endif //#ifndef __VEHICLE_TOYOTA_BZ4X_H__
