#pragma once

// Gets the Sticky past a Wi-Fi sign-in page (airports, hotels). The Sticky has
// no browser, so it shares its station link to a phone over a short-lived
// hotspot with NAT: the phone's browser shows the portal, and the portal sees
// the Sticky's MAC, so the Sticky is the device that gets signed in.
namespace captivesignin {

// True when the joined network answers the connectivity check with anything
// but 204 (a redirect or a portal page). False on 204 and on network errors,
// so a flaky link never prompts for a sign-in it doesn't need. Blocks up to ~4 s.
bool behindPortal();

// Starts the sharing hotspot next to the current station link. `ssid` and
// `password` stay valid until stop().
bool start(const char*& ssid, const char*& password);

// Tears the hotspot down and leaves the station link up. Safe to call twice.
void stop();

}  // namespace captivesignin
