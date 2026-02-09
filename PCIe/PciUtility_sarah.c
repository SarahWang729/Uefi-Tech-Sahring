/** @file
Extend the Pci_sarah application (which prints PCI devices) 
into an interactive EFI shell utility.
**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Pci22.h>

#define CONFIG_SPACE_SIZE   256

//
// Global variables
//
EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *PciRootBridgeIo = NULL;
EFI_SIMPLE_TEXT_INPUT_PROTOCOL *gConIn = NULL;
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *gConOut = NULL;

//
// Function prototypes
//
EFI_STATUS InitializeProtocols(VOID);
VOID ClearScreen(VOID);
VOID SetCursorPosition(UINTN Column, UINTN Row);
VOID SetTextAttribute(UINTN Attribute);
VOID EnableCursor(BOOLEAN Visible);
EFI_STATUS WaitForKeyPress(EFI_INPUT_KEY *Key);
VOID PrintPciDevices(VOID);
VOID DumpPciDevice(UINT8 Bus, UINT8 Device, UINT8 Function);
EFI_STATUS GetUserInput(UINT8 *Bus, UINT8 *Device, UINT8 *Function);
VOID DisplayMenu(VOID);

EFI_STATUS
EFIAPI
PciUtilityMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  UINT8 Bus = 0, Device = 0, Function = 0;
  BOOLEAN Exit = FALSE;

  //
  // Initialize protocols
  //
  Status = InitializeProtocols();
  if (EFI_ERROR(Status)) {
    Print(L"Failed to initialize protocols: %r\n", Status);
    return Status;
  }

  ClearScreen();
  SetTextAttribute(EFI_WHITE | EFI_BACKGROUND_BLUE);
  Print(L"=== PCI Utility Application ===\n\n");
  SetTextAttribute(EFI_LIGHTGRAY);

  while (!Exit) {
    DisplayMenu();
    //
    // Wait for key press
    //
    Status = WaitForKeyPress(&Key);
    if (EFI_ERROR(Status)) {
      continue;
    }

    switch (Key.UnicodeChar) {
      case '1':
        ClearScreen();
        Print(L"Scanning PCI devices...\n\n");
        PrintPciDevices();
        Print(L"\nPress any key to continue...");
        WaitForKeyPress(&Key);
        ClearScreen();
        break;

      case '2':
        ClearScreen();
        Print(L"Enter PCI Device to Dump:\n");
        Print(L"Format: Bus Device Function (e.g., 00 02 00)\n");
        
        Status = GetUserInput(&Bus, &Device, &Function);
        if (!EFI_ERROR(Status)) {
          ClearScreen();
          Print(L"Dumping PCI Device %02X:%02X.%02X\n\n", Bus, Device, Function);
          DumpPciDevice(Bus, Device, Function);
        } else {
          Print(L"Invalid input format!\n");
        }
        
        Print(L"\nPress any key to continue...");
        WaitForKeyPress(&Key);
        ClearScreen();
        break;

      case '3':
        Exit = TRUE;
        ClearScreen();
        Print(L"Exiting PCI Utility...\n");
        break;

      default:
        // Invalid key - just redisplay menu
        ClearScreen();
        Print(L"Invalid option! Please select 1, 2, or 3.\n\n");
        break;
    }
  }

  SetTextAttribute(EFI_LIGHTGRAY);
  return EFI_SUCCESS;
}

EFI_STATUS InitializeProtocols(VOID)
{
  EFI_STATUS Status;
  //
  // Get standard console protocols
  //
  gConIn = gST->ConIn;
  gConOut = gST->ConOut;
  //
  // Locate PCI Root Bridge I/O Protocol
  //
  Status = gBS->LocateProtocol(
                  &gEfiPciRootBridgeIoProtocolGuid,
                  NULL,
                  (VOID**)&PciRootBridgeIo
                  );
  if (EFI_ERROR(Status)) {
    Print(L"Failed to locate PCI Root Bridge I/O Protocol: %r\n", Status);
  }

  return Status;
}

VOID ClearScreen(VOID)
{
  if (gConOut != NULL) {
    gConOut->ClearScreen(gConOut);
  }
}

VOID SetCursorPosition(UINTN Column, UINTN Row)
{
  if (gConOut != NULL) {
    gConOut->SetCursorPosition(gConOut, Column, Row);
  }
}

VOID SetTextAttribute(UINTN Attribute)
{
  if (gConOut != NULL) {
    gConOut->SetAttribute(gConOut, Attribute);
  }
}

VOID EnableCursor(BOOLEAN Visible)
{
  if (gConOut != NULL) {
    gConOut->EnableCursor(gConOut, Visible);
  }
}

EFI_STATUS WaitForKeyPress(EFI_INPUT_KEY *Key)
{
  EFI_STATUS Status;
  
  if (gConIn == NULL) {
    return EFI_NOT_READY;
  }
  //
  // Poll for key press
  //
  while (1) {
    Status = gConIn->ReadKeyStroke(gConIn, Key);
    if (Status != EFI_NOT_READY) {
      break;
    }
    //
    // Small delay to prevent busy waiting
    //
    gBS->Stall(100000); // 100ms delay
  }
  
  return Status;
}

VOID PrintPciDevices(VOID)
{
  EFI_STATUS Status;
  UINT16 VendorId, DeviceId;
  UINT64 Address;
  UINT8 Bus, Device, Function;

  //
  // Locate PCI Root Bridge IO Protocol
  //
  Status = gBS->LocateProtocol (
                  &gEfiPciRootBridgeIoProtocolGuid,
                  NULL,
                  (VOID **)&PciRootBridgeIo
                  );

  if (EFI_ERROR (Status)) {
    Print (L"Failed to locate PCI Root Bridge IO Protocol: %r\n", Status);
    return;
  }

  for (Bus = 0; Bus < PCI_MAX_BUS; Bus++) {
    for (Device = 0; Device < PCI_MAX_DEVICE; Device++) {
      for (Function = 0; Function < PCI_MAX_FUNC; Function++) {
        Address = EFI_PCI_ADDRESS(Bus, Device, Function, 0x00);
        VendorId = 0xFFFF;
        // Read Vendor ID
        Status = PciRootBridgeIo->Pci.Read(
                    PciRootBridgeIo,
                    EfiPciWidthUint16,
                    Address,
                    1,
                    &VendorId
                    );
        //            
        // Check if device exists (Vendor ID != 0xFFFF)
        //
        if (!EFI_ERROR(Status) && VendorId != 0xFFFF) {
            Address = EFI_PCI_ADDRESS (Bus, Device, Function, 0x02);
            DeviceId = 0xFFFF;
            Status = PciRootBridgeIo->Pci.Read(
                        PciRootBridgeIo,
                        EfiPciWidthUint16,
                        Address,
                        1,
                        &DeviceId
                        );
            
            if (!EFI_ERROR (Status)) {
                Print(L"[PCI]  %02x  %02x  %02x   %04x     %04x\n", 
                           Bus, Device, Function, VendorId, DeviceId);
            }
        }
      }
    }
  }
}

VOID DumpPciDevice(UINT8 Bus, UINT8 Device, UINT8 Function)
{
  EFI_STATUS Status;
  UINT64 Address;
  UINT8 ConfigData[CONFIG_SPACE_SIZE];
  UINT8 *Data8;
  UINT16 *Data16;
  UINT32 *Data32;
  UINT16 i, j;

  // Read entire configuration space
  Address = EFI_PCI_ADDRESS(Bus, Device, Function, 0x00);
  Status = PciRootBridgeIo->Pci.Read(
              PciRootBridgeIo,
              EfiPciWidthUint8,
              Address,
              CONFIG_SPACE_SIZE,
              ConfigData
              );

  if (EFI_ERROR(Status)) {
    Print(L"Failed to read PCI configuration space: %r\n", Status);
    return;
  }

  //
  // Display header information
  //
  Data16 = (UINT16*)&ConfigData[0x00];
  Print(L"Vendor ID: 0x%04X\n", Data16[0]);
  Print(L"Device ID: 0x%04X\n", Data16[1]);
  
  Data16 = (UINT16*)&ConfigData[0x04];
  Print(L"Command: 0x%04X\n", Data16[0]);
  Print(L"Status: 0x%04X\n", Data16[1]);
  
  Data8 = (UINT8*)&ConfigData[0x08];
  Print(L"Revision ID: 0x%02X\n", Data8[0]);
  
  Data8 = (UINT8*)&ConfigData[0x09];
  Print(L"Prog IF: 0x%02X\n", Data8[0]);
  
  Data8 = (UINT8*)&ConfigData[0x0A];
  Print(L"Subclass: 0x%02X\n", Data8[0]);
  
  Data8 = (UINT8*)&ConfigData[0x0B];
  Print(L"Class Code: 0x%02X\n", Data8[0]);
  
  Data8 = (UINT8*)&ConfigData[0x0C];
  Print(L"Cache Line Size: 0x%02X\n", Data8[0]);
  
  Data8 = (UINT8*)&ConfigData[0x0D];
  Print(L"Latency Timer: 0x%02X\n", Data8[0]);
  
  Data8 = (UINT8*)&ConfigData[0x0E];
  Print(L"Header Type: 0x%02X\n", Data8[0]);
  
  Data8 = (UINT8*)&ConfigData[0x0F];
  Print(L"BIST: 0x%02X\n", Data8[0]);

  // Display Base Address Registers
  Data32 = (UINT32*)&ConfigData[0x10];
  for (i = 0; i < 6; i++) {
    Print(L"BAR%d: 0x%08X\n", i, Data32[i]);
  }

  //
  // Display full configuration space in hex dump format
  //
  Print(L"\nFull Configuration Space Dump:\n");
  Print(L"Offset  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
  Print(L"------  -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\n");
  
  for (i = 0; i < CONFIG_SPACE_SIZE; i += 16) {
    Print(L"0x%04X: ", i);
    for (j = 0; j < 16; j++) {
      if ((i + j) < CONFIG_SPACE_SIZE) {
        Print(L"%02X ", ConfigData[i + j]);
      } else {
        Print(L"   ");
      }
    }
    Print(L"\n");
  }
}

EFI_STATUS GetUserInput(UINT8 *Bus, UINT8 *Device, UINT8 *Function)
{
  EFI_STATUS Status;
  EFI_INPUT_KEY Key;
  CHAR16 Input[20] = {0};
  UINTN Index = 0;
  UINTN Values[3];
  UINTN ValueCount = 0;
  CHAR16 CurrentValue[3] = {0};
  UINTN ValueIndex = 0;
  UINTN i;

  Print(L"Enter BDF (e.g., 00 02 00): ");
  
  //
  // Read input character by character
  //
  while (Index < sizeof(Input) - 1) {
    Status = WaitForKeyPress(&Key);
    if (EFI_ERROR(Status)) {
      continue;
    }

    //
    // Enter key pressed
    //
    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      Input[Index] = 0;
      break;
    }

    //
    // Backspace handling
    //
    if (Key.UnicodeChar == CHAR_BACKSPACE) {
      if (Index > 0) {
        Index--;
        Print(L"\b \b");
      }
      continue;
    }

    //
    // Only accept hex digits and spaces
    //
    if ((Key.UnicodeChar >= L'0' && Key.UnicodeChar <= L'9') ||
        (Key.UnicodeChar >= L'A' && Key.UnicodeChar <= L'F') ||
        (Key.UnicodeChar >= L'a' && Key.UnicodeChar <= L'f') ||
        Key.UnicodeChar == L' ') {
      Input[Index++] = Key.UnicodeChar;
      Print(L"%c", Key.UnicodeChar);
    }
  }

  Print(L"\n");

  //
  // Parse the input string
  //
  for (i = 0; i < Index && ValueCount < 3; i++) {
    if (Input[i] != L' ') {
      if (ValueIndex < 2) {
        CurrentValue[ValueIndex++] = Input[i];
      }
    } else {
      if (ValueIndex > 0) {
        CurrentValue[ValueIndex] = 0;
        Values[ValueCount++] = StrHexToUintn(CurrentValue);
        ValueIndex = 0;
      }
    }
  }

  //
  // Get the last value
  //
  if (ValueIndex > 0 && ValueCount < 3) {
    CurrentValue[ValueIndex] = 0;
    Values[ValueCount++] = StrHexToUintn(CurrentValue);
  }

  if (ValueCount == 3) {
    *Bus = (UINT8)Values[0];
    *Device = (UINT8)Values[1];
    *Function = (UINT8)Values[2];
    return EFI_SUCCESS;
  }

  return EFI_INVALID_PARAMETER;
}

VOID DisplayMenu(VOID)
{
  SetTextAttribute(EFI_WHITE | EFI_BACKGROUND_BLUE);
  Print(L"=== PCI Utility Main Menu ===\n\n");
  SetTextAttribute(EFI_LIGHTGRAY);
  
  Print(L"1. Scan and List PCI Devices\n");
  Print(L"2. Dump PCI Device Information\n");
  Print(L"3. Exit\n\n");
  Print(L"Please select an option (1-3): ");
}