/** @file
  VariableHobB.c - Consumer PEIM that uses VARIABLE_HOB_PPI
**/

#include <Uefi.h>
#include <PiPei.h>
#include <Library/PeiServicesLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include "../VariableHobPpi.h"

/* Test variable name & GUID */
STATIC CONST CHAR16 mTestVarName[] = L"TestVariable";
STATIC CONST EFI_GUID mTestVarGuid = { 0x60652107, 0xda54, 0x4ee2, { 0xb6,0x23,0x1e,0x16,0x10,0xd4,0x91,0x4a } };

EFI_STATUS
EFIAPI
VariableHobBEntry (
  IN EFI_PEI_FILE_HANDLE       FileHandle,
  IN CONST EFI_PEI_SERVICES   **PeiServices
  )
{
  EFI_STATUS               Status;
  VARIABLE_HOB_PPI        *VarPpi;
  EFI_PEI_PPI_DESCRIPTOR  *PpiDescriptor;
  UINT8                    Buffer[256];
  UINTN                    DataSize;

  DEBUG((DEBUG_INFO, "VariableHobB: Entry - locate VariableHob PPI\n"));

  Status = PeiServicesLocatePpi (&gVariableHobPpiGuid, 0, &PpiDescriptor, (VOID **)&VarPpi);
  if (EFI_ERROR(Status) || VarPpi == NULL) {
    DEBUG ((DEBUG_ERROR, "VariableHobB: Locate PPI failed %r\n", Status));
    return Status;
  }

  /* Query variable */
  DataSize = sizeof(Buffer);
  Status = VarPpi->GetVariable ((CHAR16 *)mTestVarName, (EFI_GUID *)&mTestVarGuid, &DataSize, Buffer);
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_INFO, "VariableHobB: GetVariable returned %r\n", Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "VariableHobB: Got variable size=%u\n", (UINT32)DataSize));

  /* Ask provider to create HOB containing the variable */
  Status = VarPpi->CreateHob (Buffer, DataSize);
  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_ERROR, "VariableHobB: CreateHob failed %r\n", Status));
    return Status;
  }

  DEBUG ((DEBUG_INFO, "VariableHobB: CreateHob OK\n"));
  return EFI_SUCCESS;
}
