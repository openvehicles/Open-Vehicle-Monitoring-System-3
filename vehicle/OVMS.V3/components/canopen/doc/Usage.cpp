// ================================================================================
// CANopen application usage
// ================================================================================

  #include "canopen.h"

  // find CAN interface:
  canbus* bus = (canbus*) MyPcpApp.FindDeviceByName("can1");
  // …or simply use m_can1 if you're a vehicle subclass ;)

  // create CANopen client:
  CANopenClient client(bus);

  // read value from node #1 SDO 0x1008.00:
  uint32_t value;
  if (client.ReadSDO(1, 0x1008, 0x00, (uint8_t)&value, sizeof(value)) == COR_OK)
    …

  // start node #2, wait for presence:
  if (client.SendNMT(2, CONC_Start, true) == COR_OK)
    …

  // write value into node #3 SDO 0x2345.18:
  if (client.WriteSDO(3, 0x2345, 0x18, (uint8_t)&value, 0) == COR_OK)
    …

// ================================================================================
// See shell commands in canopen.cpp for more examples.
// ================================================================================
