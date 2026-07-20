#pragma once
#include "can.h"

typedef struct {
    union { struct { unsigned eid:18; unsigned :3; unsigned sid:11; } b; uint32_t u32; uint8_t u8[4]; } mask[2];
    union { struct { unsigned eid:18; unsigned :1; unsigned exide:1; unsigned :1; unsigned sid:11; } b; uint32_t u32; uint8_t u8[4]; } filter[6];
} mcp2515_filter_config_t;

class mcp2515 : public canbus {
public:
    int SetAcceptanceFilter(const mcp2515_filter_config_t&) { return 0; }
};
