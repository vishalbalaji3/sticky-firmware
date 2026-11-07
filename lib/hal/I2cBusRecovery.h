#pragma once

// Frees an I2C bus that a slave is holding. An ESP reset in the middle of a read never reaches
// battery-powered slaves (fuel gauge, RTC), so one can keep driving SDA low while it waits for
// the rest of its byte. Up to 9 SCL clocks (8 data bits + ACK) let it finish, then a STOP resets
// every slave's bus state. Io provides sdaHigh(), clockScl() and stop().
// Returns the clocks it needed, or -1 when SDA is still low.
template <typename Io>
int clearI2cBus(Io& io) {
  int clocks = 0;
  while (!io.sdaHigh() && clocks < 9) {
    io.clockScl();
    ++clocks;
  }
  if (!io.sdaHigh()) return -1;
  io.stop();
  return clocks;
}
