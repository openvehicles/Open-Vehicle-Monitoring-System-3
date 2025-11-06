void OvmsVehicleMaxt90::IncomingFrameCan1(CAN_frame_t* p_frame)
{
  // Let base class do any generic processing:
  OvmsVehicle::IncomingFrameCan1(p_frame);

  const uint8_t* d = p_frame->data.u8;

  switch (p_frame->MsgID)
  {
    // ─────────────────────────────────────────────
    // 0x266: Steering wheel
    // ─────────────────────────────────────────────
    case 0x266:
    {
      // 16-bit little-endian from bytes 0+1:
      int16_t steer_raw = (int16_t)((uint16_t(d[1]) << 8) | uint16_t(d[0]));

      // For now, just log it:
      ESP_LOGD(TAG, "0x266 steering raw=0x%04x (%d)", uint16_t(steer_raw), steer_raw);

      // If you later want a custom metric:
      // MyMetrics.InitInt("x.maxt90.steer_raw", 0, 0, Other, false)->SetValue(steer_raw);
      break;
    }

    // ─────────────────────────────────────────────
    // 0x281: Lock/unlock + indicators + reverse + key positions
    //
    // From your notes (bitfield in bytes 3+4, little-endian word):
    //   0001 = unlock pulse / "Bil på ett hakk" (1st key position?)
    //   0002 = "Bil på 2/3 hakk"
    //   0800 = flash left
    //   1000 = flash right
    //   2000 = reverse
    //
    // IMPORTANT: this appears to be an *event/pulse* frame, not a persistent state.
    // So we treat 0x0001 as an "unlock command" and force locked = false here,
    // rather than trying to derive full lock state from this ID.
    // ─────────────────────────────────────────────
    case 0x281:
    {
      uint16_t w = uint16_t(d[3]) | (uint16_t(d[4]) << 8);

      bool unlock_pulse = (w & 0x0001) != 0;      // seen when you press driver's unlock
      bool key_pos_2_3  = (w & 0x0002) != 0;      // placeholder decode
      bool flash_left   = (w & 0x0800) != 0;      // 0800
      bool flash_right  = (w & 0x1000) != 0;      // 1000
      bool reverse_gear = (w & 0x2000) != 0;      // 2000 (fixed from 0x0020)

      // Treat 0x0001 as an UNLOCK command, not "locked":
      if (unlock_pulse)
      {
        bool was_locked = StdMetrics.ms_v_env_locked->AsBool();
        if (was_locked) {
          ESP_LOGI(TAG, "0x281 unlock pulse: raw=0x%04x -> locked=false", w);
        } else {
          ESP_LOGD(TAG, "0x281 unlock pulse while already unlocked: raw=0x%04x", w);
        }
        StdMetrics.ms_v_env_locked->SetValue(false);
      }

      // Just log everything so you can refine the bit meanings later:
      static uint16_t last_281 = 0xffff;
      if (w != last_281) {
        ESP_LOGD(TAG,
          "0x281 raw=0x%04x L=%d R=%d unlock=%d key23=%d rev=%d",
          w, flash_left, flash_right, unlock_pulse, key_pos_2_3, reverse_gear);
        last_281 = w;
      }

      // If you later want indicators as metrics you could add:
      //   MyMetrics.InitBool("x.maxt90.ind_left",   0, false, Other, false)->SetValue(flash_left);
      //   MyMetrics.InitBool("x.maxt90.ind_right",  0, false, Other, false)->SetValue(flash_right);
      break;
    }

    // ─────────────────────────────────────────────
    // 0x355: Dashboard seatbelt / passenger sensor
    // ─────────────────────────────────────────────
    case 0x355:
    {
      // Take last 4 bytes as big-endian mask (adjust if needed once we see real frames):
      uint32_t mask = (uint32_t(d[4]) << 24) |
                      (uint32_t(d[5]) << 16) |
                      (uint32_t(d[6]) << 8)  |
                       uint32_t(d[7]);

      ESP_LOGD(TAG,
        "0x355 seatbelt mask=0x%08x", mask);

      // Later you can decode:
      //   passenger_belt  = mask & 0x00008000;
      //   driver_belt     = mask & 0x00000040;
      //   rear3_belt      = mask & 0x00000020;
      //   rear1_belt      = mask & 0x00000002;
      //   rear2_belt      = mask & 0x00000008;
      //   passenger_sensor= mask & 0x00000008 (different byte?)
      break;
    }

    // ─────────────────────────────────────────────
    // 0x362: Odometer (broadcast)
    // ─────────────────────────────────────────────
    case 0x362:
    {
      // 24-bit big-endian from bytes 4,5,6:
      uint32_t raw = (uint32_t(d[4]) << 16) |
                     (uint32_t(d[5]) << 8)  |
                      uint32_t(d[6]);

      float odom_km = raw / 1000.0f;   // assume metres → km
      StdMetrics.ms_v_pos_odometer->SetValue(odom_km);

      static uint32_t last_raw = 0;
      if (raw != last_raw) {
        ESP_LOGI(TAG, "0x362 odometer: raw=0x%06x (%.1f km)", raw, odom_km);
        last_raw = raw;
      }
      break;
    }

    // ─────────────────────────────────────────────
    // 0x373: Trip counter? (unknown scaling)
    // ─────────────────────────────────────────────
    case 0x373:
    {
      uint64_t raw =
        (uint64_t(d[0]) << 56) |
        (uint64_t(d[1]) << 48) |
        (uint64_t(d[2]) << 40) |
        (uint64_t(d[3]) << 32) |
        (uint64_t(d[4]) << 24) |
        (uint64_t(d[5]) << 16) |
        (uint64_t(d[6]) << 8)  |
         uint64_t(d[7]);

      ESP_LOGD(TAG, "0x373 trip raw=0x%016llx",
               (unsigned long long) raw);

      // Once you know which bytes move with trip A/B, we can split & scale.
      break;
    }

    // ─────────────────────────────────────────────
    // 0x375: Dashboard lights + door status
    // ─────────────────────────────────────────────
    case 0x375:
    {
      uint8_t light_raw = d[0];

      // Simple mapping: any of bits 0x02 (dipped/high) means "headlights on":
      bool headlights = (light_raw & 0x02) != 0;
      StdMetrics.ms_v_env_headlights->SetValue(headlights);

      // Decode a simple numeric light mode metric:
      //   0 = off, 1 = park, 2 = dipped, 3 = high, 4 = fog+dipped
      int light_mode = 0;
      switch (light_raw)
      {
        case 0x00: light_mode = 0; break; // off
        case 0x01: light_mode = 1; break; // park
        case 0x03: light_mode = 2; break; // dipped
        case 0x07: light_mode = 3; break; // high
        case 0x13: light_mode = 4; break; // fog+dipped
        default:   light_mode = -1; break; // unknown combo
      }

      // Door bits – assume 16-bit big-endian word from bytes 4+5:
      uint16_t dw = (uint16_t(d[4]) << 8) | uint16_t(d[5]);

      bool door_rl = (dw & 0x0001) != 0; // LR door
      bool door_rr = (dw & 0x8000) != 0; // RR door
      bool door_fr = (dw & 0x4000) != 0; // passenger front
      bool door_fl = (dw & 0x2000) != 0; // driver front

      // Map to standard door metrics:
      StdMetrics.ms_v_door_fl->SetValue(door_fl);
      StdMetrics.ms_v_door_fr->SetValue(door_fr);
      StdMetrics.ms_v_door_rl->SetValue(door_rl);
      StdMetrics.ms_v_door_rr->SetValue(door_rr);
      // trunk/bonnet not mapped here

      ESP_LOGD(TAG,
        "0x375 lights: raw=0x%02x mode=%d, doors: raw=0x%04x fl=%d fr=%d rl=%d rr=%d",
        light_raw, light_mode, dw,
        door_fl, door_fr, door_rl, door_rr);

      break;
    }

    default:
      break;
  }
}
