/** @file
Timer Event DXE Driver
- Creates a periodic timer event (1 second)
- Notify function prints current date/time
- Prevents double loading via marker protocol
- Supports Unload to cancel timer and cleanup
**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Protocol/LoadedImage.h>
#include "TimerEventMarkerProtocol.h"

STATIC EFI_EVENT  mTimerEvent   = NULL;
STATIC EFI_HANDLE mDriverHandle = NULL;

/**
  Timer event callback function prototype.

  @param[in] Event     The timer event that is being notified.
  @param[in] Context   Pointer to notification function context.
**/
STATIC
VOID
EFIAPI
TimerNotifyFunction (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_TIME   Time;
  EFI_STATUS Status;
  UINTN      Columns;
  UINTN      Rows;
  UINTN      OldCol;
  UINTN      OldRow;
  UINTN      X;
  UINTN      Y;

  
  //
  // Get current system time.
  //
  Status = gRT->GetTime(&Time, NULL);
  if (EFI_ERROR(Status)) {
    return;
  }

  //
  // Get console size
  //
  Status = gST->ConOut->QueryMode(
                           gST->ConOut,
                           gST->ConOut->Mode->Mode,
                           &Columns,
                           &Rows
                           );

  if (EFI_ERROR(Status) || Columns == 0) {
    return;
  }

  //
  // Save original cursor position
  //
  OldCol = gST->ConOut->Mode->CursorColumn;
  OldRow = gST->ConOut->Mode->CursorRow;

  //
  // Compute upper-right corner position
  //
  if (Columns > TIMER_STRING_WIDTH) {
    X = Columns - TIMER_STRING_WIDTH;
  } else {
    X = 0;
  }
  Y = 0;   // top row

  //
  // Move cursor to upper-right and print timestamp (no newline)
  //
  gST->ConOut->SetCursorPosition(gST->ConOut, X, Y);

  Print(L"%04d-%02d-%02d %02d:%02d:%02d",
        (UINTN)Time.Year,
        (UINTN)Time.Month,
        (UINTN)Time.Day,
        (UINTN)Time.Hour,
        (UINTN)Time.Minute,
        (UINTN)Time.Second);

  //
  // Restore original cursor position
  //
  gST->ConOut->SetCursorPosition(gST->ConOut, OldCol, OldRow);
}

/**
  Unload entry point: stops timer, closes event, uninstalls protocol

  @param[in] ImageHandle   The image handle for this driver.
  @retval EFI_SUCCESS      The driver was unloaded and resources were freed.
**/
EFI_STATUS
EFIAPI
TimerDriverUnload (
  IN EFI_HANDLE ImageHandle
  )
{
  EFI_STATUS Status;

  //
  // Cancel and close the timer event if it is active.
  //
  if (mTimerEvent != NULL) {
    gBS->SetTimer(mTimerEvent, TimerCancel, 0);
    gBS->CloseEvent(mTimerEvent);
    mTimerEvent = NULL;
  }
  
  //
  // Uninstall the marker protocol so another instance can be loaded later.
  //
  if (mDriverHandle != NULL) {
    Status = gBS->UninstallProtocolInterface(
                    mDriverHandle,
                    &gTimerEventMarkerProtocolGuid,
                    NULL
                    );

    if (EFI_ERROR(Status)) {
      Print(L"[TimerEvent] UninstallProtocolInterface failed: %r\n", Status);
    }
    mDriverHandle = NULL;
  }

  Print(L"TimerEvent: unloaded successfully.\n");
  return EFI_SUCCESS;
}

/**
  Main entry point prototype for the Timer Event DXE driver.

  @param[in] ImageHandle   The firmware allocated handle for the UEFI image.
  @param[in] SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The driver initialized successfully.
  @retval EFI_ALREADY_STARTED   Another instance of this driver is already loaded.
  @retval Others                An error occurred during initialization.
**/
EFI_STATUS
EFIAPI
TimerDriverEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  VOID                      *Existing;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;

  //
  // Check if a previous instance of this driver is already loaded by
  // locating the marker protocol.
  //
  Status = gBS->LocateProtocol(&gTimerEventMarkerProtocolGuid, NULL, &Existing);
  if (!EFI_ERROR(Status)) {
    Print(L"TimerEvent: another instance is already loaded.\n");
    return EFI_ALREADY_STARTED;
  }

  //
  // Install marker protocol to indicate that this driver is now loaded.
  //
  mDriverHandle = ImageHandle;
  Status = gBS->InstallProtocolInterface(
                  &mDriverHandle,
                  &gTimerEventMarkerProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  NULL
                  );

  if (EFI_ERROR(Status)) {
    Print(L"InstallProtocolInterface failed: %r\n", Status);
    return Status;
  }

  //
  // Setup Unload entry point
  //
  Status = gBS->HandleProtocol(
                  ImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );

  if (!EFI_ERROR(Status)) {
    LoadedImage->Unload = TimerDriverUnload;
  }

  //
  // Create periodic timer event
  //
  Status = gBS->CreateEvent(
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  TimerNotifyFunction,
                  NULL,
                  &mTimerEvent
                  );
                  
  if (EFI_ERROR(Status)) {
    Print(L"CreateEvent failed: %r\n", Status);
    gBS->UninstallProtocolInterface(
           mDriverHandle,
           &gTimerEventMarkerProtocolGuid,
           NULL
           );
    return Status;
  }

  //
  // Start 1-second periodic timer (10,000,000 * 100ns = 1s)
  //
  Status = gBS->SetTimer(mTimerEvent, TimerPeriodic, 10000000ULL);
  if (EFI_ERROR(Status)) {
    Print(L"SetTimer failed: %r\n", Status);
    gBS->CloseEvent(mTimerEvent);
    gBS->UninstallProtocolInterface(
           mDriverHandle,
           &gTimerEventMarkerProtocolGuid,
           NULL
           );
    return Status;
  }

  Print(L"TimerEvent DXE driver loaded. Timer started (1s period).\n");
  return EFI_SUCCESS;
}

/**
  DXE driver entry point required by INF (ENTRY_POINT).

  @param[in] ImageHandle   The firmware allocated handle for the UEFI image.
  @param[in] SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS           The driver initialized successfully.
  @retval EFI_ALREADY_STARTED   Another instance of this driver is already loaded.
  @retval Others                An error occurred during initialization.
**/
EFI_STATUS
EFIAPI
TimerEventInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return TimerDriverEntry(ImageHandle, SystemTable);
}
