#include <Uefi.h>
#include <PiPei.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/HobLib.h>
#include "../VariableHobPpi.h"

EFI_GUID gVariableHobPpiGuid = VARIABLE_HOB_PPI_GUID;

/* Extern accessor for HOB list — replace this if your platform uses a different symbol */
extern VOID *GetHobList(VOID);

//
// Test variable name & GUID
//
STATIC CHAR16 gTestVarName[] = L"TestVariable";
STATIC EFI_GUID gTestVarGuid = { 0x60652107, 0xda54, 0x4ee2, { 0xb6, 0x23, 0x1e, 0x16, 0x10, 0xd4, 0x91, 0x4a } };

//
// Read line helper
//
EFI_STATUS
ReadLine (
  OUT CHAR16 *Buffer,
  IN  UINTN   BufferLen
  )
{
  UINTN Index = 0;
  EFI_INPUT_KEY Key;
  UINTN EventIndex;

  ZeroMem (Buffer, BufferLen * sizeof(CHAR16));

  while (TRUE) {
    gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &EventIndex);
    gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);

    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      Print(L"\r\n");
      Buffer[Index] = L'\0';
      return EFI_SUCCESS;
    }

    if (Key.UnicodeChar == CHAR_BACKSPACE) {
      if (Index > 0) {
        Index--;
        Print (L"\b \b");
      }
      continue;
    }

    if (Key.UnicodeChar >= 0x20 && Key.UnicodeChar <= 0x7E) {
      if (Index + 1 < BufferLen) {
        Buffer[Index++] = Key.UnicodeChar;
        Print (L"%c", Key.UnicodeChar);
      }
    }
  }
}

//
// Dump all variables and indicate presence of TestVariable
//
VOID
DumpVariablesAndCheckTestVar(OUT BOOLEAN *TestVarPresent)
{
  EFI_STATUS Status;
  UINTN BufferLen = 128;
  UINTN NameSize;
  CHAR16 *Name;
  EFI_GUID Guid;
  CHAR16 *NewBuf;
  BOOLEAN Found;
  INTN i;

  if (TestVarPresent == NULL) {
    return;
  }

  Name = AllocateZeroPool (BufferLen * sizeof(CHAR16));
  if (Name == NULL) {
    Print (L"(Out of resources)\n");
    *TestVarPresent = FALSE;
    return;
  }

  Found = FALSE;
  Print (L"\n=== Variables in NVRAM ===\n");

  while (TRUE) {
    NameSize = BufferLen;
    Status = gRT->GetNextVariableName (&NameSize, Name, &Guid);

    if (Status == EFI_BUFFER_TOO_SMALL) {
      NewBuf = ReallocatePool (BufferLen * sizeof(CHAR16), NameSize * sizeof(CHAR16), Name);
      if (NewBuf == NULL) {
        Print (L"(Out of memory while growing variable name buffer)\n");
        break;
      }
      Name = NewBuf;
      BufferLen = NameSize;
      continue;
    }

    if (Status == EFI_NOT_FOUND) {
      // end of variable list
      break;
    }

    if (EFI_ERROR(Status)) {
      Print (L"GetNextVariableName returned error: %r\n", Status);
      break;
    }

    //
    // Print variable name and GUID
    //
    Print (L"  - %s (GUID=", Name);
    Print (L"%08x-%04x-%04x-", Guid.Data1, Guid.Data2, Guid.Data3);
    for (i = 0; i < 2; i++) {
      Print (L"%02x", Guid.Data4[i]);
    }
    Print (L"-");
    for (i = 2; i < 8; i++) {
      Print (L"%02x", Guid.Data4[i]);
    }
    Print (L")\n");

    if (StrCmp(Name, gTestVarName) == 0 && CompareGuid(&Guid, &gTestVarGuid)) {
      Found = TRUE;
    }
  }

  if (!Found) {
    Print (L"(Test variable not found in NVRAM)\n");
  } else {
    Print (L"(Test variable present in NVRAM)\n");
  }

  FreePool (Name);
  *TestVarPresent = Found;
}

//
// Dump HOB list and check for GUID HOB created by PEIM
//
VOID
DumpHobAndCheck(OUT BOOLEAN *HobPresent)
{
  EFI_HOB_GUID_TYPE *GuidHob;
  UINT8 *Data;
  UINTN DataSize;
  UINTN i;

  if (HobPresent == NULL) {
      return;
  }
  *HobPresent = FALSE;

  Print (L"\n=== Searching for VARIABLE HOB ===\n\n");

  // Get first GUID HOB matching gVariableHobPpiGuid
  GuidHob = GetFirstGuidHob(&gVariableHobPpiGuid);
  if (GuidHob == NULL) {
      Print (L"(No matching GUID HOB found)\n");
      return;
  }

  // Payload immediately after GUID HOB header
  Data = (UINT8 *)(GuidHob + 1);
  DataSize = GuidHob->Header.HobLength - sizeof(EFI_HOB_GUID_TYPE);

  Print (L"Found VARIABLE HOB\n");

  // Dump first 64 bytes
  Print (L"Hex: ");
  for (i = 0; i < DataSize && i < 64; i++) {
      Print (L"%02x ", Data[i]);
  }
  Print (L"\n");

  *HobPresent = TRUE;

  return;

}

//
// Set test variable
//
EFI_STATUS
SetTestVariable(VOID)
{
  CHAR16 Data[] = L"HelloFromTestPEIM";
  EFI_STATUS Status;

  Status = gRT->SetVariable(
             gTestVarName,
             &gTestVarGuid,
             EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE,
             sizeof(Data),
             Data
             );

  if (EFI_ERROR(Status)) {
    Print (L"SetVariable failed: %r\n", Status);
  } else {
    Print (L"SetVariable succeeded (name=%s)\n", gTestVarName);
  }
  return Status;
}

//
// Delete test variable
//
EFI_STATUS
DeleteTestVariable(VOID)
{
  EFI_STATUS Status;

  Status = gRT->SetVariable(
             gTestVarName,
             &gTestVarGuid,
             0,
             0,
             NULL
             );

  if (EFI_ERROR(Status)) {
    Print (L"Delete variable failed: %r\n", Status);
  } else {
    Print (L"Delete variable succeeded (name=%s)\n", gTestVarName);
  }
  return Status;
}

VOID WaitAnyKey(VOID)
{
  EFI_INPUT_KEY Key;
  UINTN Index;
  Print (L"\nPress any key to continue...");
  gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
  gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
  Print (L"\n");
}

//
// Show verification instructions
//
VOID
ShowVerificationInstructions(VOID)
{
  Print (L"\n\nVarHobViewer - manual 4-step verification tool\n");
  Print (L"(1) After boot into EFI shell, choose 1 and 2 to confirm empty.\n");
  Print (L"(2) Choose 3 then 1 and 2 to confirm variable exists but HOB not present.\n");
  Print (L"(3) Cold reboot then run again and choose 1 and 2 to check both present.\n");
  Print (L"(4) Choose 4 to delete the variable and then 1 and 2 to check variable deleted but HOB remains.\n\n");
}

//
// Menu + main
//
VOID ShowMenu(VOID)
{
  Print (L"\n=== PEIM Verification Menu ===\n");
  Print (L"1) Dump Variables\n");
  Print (L"2) Dump HOB Data\n");
  Print (L"3) Set test variable\n");
  Print (L"4) Delete test variable\n");
  Print (L"5) Show verification instructions\n");
  Print (L"6) Exit\n");
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_INPUT_KEY Key;
  UINTN EventIndex;
  BOOLEAN TestVarPresent;
  BOOLEAN HobPresent;

  while (TRUE) {
    ShowMenu();
    Print (L"Choose option: ");
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &EventIndex);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    switch (Key.UnicodeChar) {
      case L'1':
        DumpVariablesAndCheckTestVar(&TestVarPresent);
        Print(L"\nTestVariable present? %a\n", TestVarPresent ? "Yes" : "No");
        WaitAnyKey();
        break;

      case L'2':
        DumpHobAndCheck(&HobPresent);
        Print(L"\nHOB present? %a\n", HobPresent ? "Yes" : "No");
        WaitAnyKey();
        break;

      case L'3':
        SetTestVariable();
        WaitAnyKey();
        break;

      case L'4':
        DeleteTestVariable();
        WaitAnyKey();
        break;

      case L'5':
        ShowVerificationInstructions();
        WaitAnyKey();
        break;

      case L'6':
        return EFI_SUCCESS;

      default:
        Print (L"Invalid selection.\n");
        WaitAnyKey();
        break;
    }
  }
}
