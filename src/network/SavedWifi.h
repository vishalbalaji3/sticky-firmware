#pragma once

// Joins the last-used saved network without any UI, for background work that
// must not show the network picker. Call hasLastSavedWifi() first: it loads the
// saved credentials from the card.
bool hasLastSavedWifi();
// Waits up to timeoutMs for the join. Returns true when connected.
bool joinLastSavedWifi(unsigned long timeoutMs);
// Turns the radio fully off, so the voice recorder (which needs WiFi off) works again.
void stopWifi();
