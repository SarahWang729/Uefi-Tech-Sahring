/** @file
  Timer Event Marker Protocol definition.

  This protocol is installed by the TimerEvent driver to indicate that
  the driver is currently loaded. Other instances can check for this
  protocol to prevent loading twice.
**/

#ifndef _TIMER_EVENT_MARKER_PROTOCOL_H_
#define _TIMER_EVENT_MARKER_PROTOCOL_H_

#define TIMER_EVENT_MARKER_PROTOCOL_GUID \
  {0xc90210e5, 0x8f61, 0x4a47, {0xa1, 0xa8, 0xdc, 0xa7, 0x91, 0x8f, 0x3e, 0x2d}}

#define TIMER_STRING_WIDTH   19   // "YYYY-MM-DD HH:MM:SS" = 19 characters

extern EFI_GUID gTimerEventMarkerProtocolGuid;

#endif
