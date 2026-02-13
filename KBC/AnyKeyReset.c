/** @file
  UEFI Shell App: Press any key to Reset System via KBC (8042) IoWrite()
**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>

//
// 8042 / KBC I/O ports
//
#define KBC_DATA_PORT     0x60
#define KBC_CMD_PORT      0x64
#define KBC_STATUS_PORT   0x64

//
// Status bits (from 8042 standard)
//
#define KBC_STATUS_OBF    BIT0  // Output Buffer Full
#define KBC_STATUS_IBF    BIT1  // Input Buffer Full

//
// KBC command: Pulse System Reset
//
#define KBC_CMD_PULSE_RESET  0xFE

STATIC
EFI_STATUS
WaitForKey (
  OUT EFI_INPUT_KEY  *Key
  )
{
  EFI_STATUS  Status;
  UINTN       EventIndex;

  if (Key == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  while (TRUE) {
    Status = gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &EventIndex);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, Key);
    if (!EFI_ERROR (Status)) {
      return EFI_SUCCESS;
    }
  }
}

STATIC
EFI_STATUS
KbcWaitInputBufferEmpty (
  IN UINTN TimeoutUs
  )
{
  //
  // Wait until IBF==0, otherwise writing command may be ignored or hang.
  //
  while (TimeoutUs--) {
    if ((IoRead8 (KBC_STATUS_PORT) & KBC_STATUS_IBF) == 0) {
      return EFI_SUCCESS;
    }
    MicroSecondDelay (1);
  }
  return EFI_TIMEOUT;
}

STATIC
EFI_STATUS
KbcPulseReset (
  VOID
  )
{
  EFI_STATUS Status;

  //
  // Make sure KBC is ready to accept a command.
  //
  Status = KbcWaitInputBufferEmpty (200000); // 200ms
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Send "Pulse System Reset" command to 0x64
  //
  IoWrite8 (KBC_CMD_PORT, KBC_CMD_PULSE_RESET);

  //
  // If it returns, something is wrong. Spin.
  //
  CpuDeadLoop ();
  return EFI_DEVICE_ERROR;
}

EFI_STATUS
EFIAPI
AnyKeyKbcResetEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS     Status;
  EFI_INPUT_KEY  Key;

  Print (L"\n[AnyKeyReset] Press ANY key to reset the system (KBC IoWrite).\n");
  Print (L"[AnyKeyReset] (This uses 8042 command 0xFE to port 0x64)\n\n");

  Status = WaitForKey (&Key);
  if (EFI_ERROR (Status)) {
    Print (L"WaitForKey failed: %r\n", Status);
    return Status;
  }

  Print (L"Key pressed (Scan=%x, Char=%c). Resetting now...\n",
         Key.ScanCode,
         (Key.UnicodeChar == 0) ? L' ' : Key.UnicodeChar);

  Status = KbcPulseReset ();
  Print (L"KbcPulseReset failed: %r\n", Status);
  return Status;
}
