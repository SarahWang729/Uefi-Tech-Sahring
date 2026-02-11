/** @file
  VariableHobA.c - Provider PEIM implementing VARIABLE_HOB_PPI
**/

#include <Uefi.h>
#include <PiPei.h>
#include <Ppi/ReadOnlyVariable2.h>
#include <Library/PeiServicesLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include "../VariableHobPpi.h"

/* Test variable name & GUID used by sample consumers */
STATIC CONST CHAR16 mTestVarName[] = L"TestVariable";
STATIC CONST EFI_GUID mTestVarGuid = { 0x60652107, 0xda54, 0x4ee2, { 0xb6,0x23,0x1e,0x16,0x10,0xd4,0x91,0x4a } };

/* Simple static cache; real PEIMs can allocate as needed */
#define MAX_CACHE_SIZE 512
STATIC UINT8  mCache[MAX_CACHE_SIZE];
STATIC UINTN  mCacheSize = 0;

STATIC
EFI_STATUS
EFIAPI
PeimGetVariable (
  IN  CONST CHAR16   *VariableName,
  IN  CONST EFI_GUID *VendorGuid,
  IN OUT UINTN       *DataSize,
  OUT VOID           *Data
  );

STATIC
EFI_STATUS
EFIAPI
PeimCreateHob (
  IN CONST VOID *Buffer,
  IN UINTN       BufferSize
  );

/* PPI instance */
STATIC VARIABLE_HOB_PPI mVariableHobPpi = {
  PeimGetVariable,
  PeimCreateHob
};

/* PPI descriptor */
STATIC EFI_PEI_PPI_DESCRIPTOR mVariableHobPpiDesc = {
  (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
  &gVariableHobPpiGuid,
  &mVariableHobPpi
};

/* Implementation: GetVariable using ReadOnlyVariable2 PPI */
STATIC
EFI_STATUS
EFIAPI
PeimGetVariable (
  IN  CONST CHAR16   *VariableName,
  IN  CONST EFI_GUID *VendorGuid,
  IN OUT UINTN       *DataSize,
  OUT VOID           *Data
  )
{
  EFI_STATUS                            Status;
  EFI_PEI_READ_ONLY_VARIABLE2_PPI     *ReadOnlyVarPpi;
  UINTN                                 BufferSize;
  UINT32                                Attributes;

  if (VariableName == NULL || VendorGuid == NULL || DataSize == NULL || Data == NULL) {
    DEBUG ((DEBUG_ERROR, "VariableHobA: Invalid parameter to GetVariable\n"));
    return EFI_INVALID_PARAMETER;
  }

  /* Locate ReadOnlyVariable2 PPI */
  Status = PeiServicesLocatePpi (&gEfiPeiReadOnlyVariable2PpiGuid, 0, NULL, (VOID **)&ReadOnlyVarPpi);
  if (EFI_ERROR(Status) || ReadOnlyVarPpi == NULL) {
    DEBUG ((DEBUG_ERROR, "VariableHobA: ReadOnlyVariable2 PPI not found (%r)\n", Status));
    return EFI_UNSUPPORTED;
  }

  BufferSize = *DataSize;
  Attributes = 0;
  Status = ReadOnlyVarPpi->GetVariable (ReadOnlyVarPpi, (CHAR16 *)VariableName, (EFI_GUID *)VendorGuid, &Attributes, &BufferSize, Data);

  if (EFI_ERROR(Status)) {
    DEBUG ((DEBUG_INFO, "VariableHobA: ReadOnlyVariable2 GetVariable returned %r\n", Status));
    return Status;
  }

  /* Cache a small copy (if it fits) for later use */
  if (BufferSize <= MAX_CACHE_SIZE) {
    CopyMem (mCache, Data, BufferSize);
    mCacheSize = BufferSize;
  } else {
    mCacheSize = 0;
  }

  *DataSize = BufferSize;
  DEBUG ((DEBUG_INFO, "VariableHobA: GetVariable OK size=%u\n", (UINT32)BufferSize));
  return EFI_SUCCESS;
}

/* Implementation: Create a GUID HOB with the vendor GUID defined in header */
STATIC
EFI_STATUS
EFIAPI
PeimCreateHob (
  IN CONST VOID *Buffer,
  IN UINTN       BufferSize
  )
{
  EFI_STATUS         Status;
  VOID              *NewHob;
  EFI_HOB_GUID_TYPE *GuidHob;
  UINT16             HobLen;

  if (Buffer == NULL || BufferSize == 0) {
    DEBUG((DEBUG_ERROR, "VariableHobA: CreateHob invalid parameter\n"));
    return EFI_INVALID_PARAMETER;
  }

  /* Guard BufferSize so Hob length doesn't overflow 16-bit field */
  if (BufferSize > 0xFF00) {
    DEBUG ((DEBUG_ERROR, "VariableHobA: Buffer too large for HOB (%u)\n", (UINT32)BufferSize));
    return EFI_BAD_BUFFER_SIZE;
  }

  HobLen = (UINT16)(sizeof(EFI_HOB_GUID_TYPE) + (UINT16)BufferSize);

  /* Create GUID HOB via PeiServicesCreateHob */
  Status = PeiServicesCreateHob (EFI_HOB_TYPE_GUID_EXTENSION, HobLen, &NewHob);
  if (EFI_ERROR(Status) || NewHob == NULL) {
    DEBUG ((DEBUG_ERROR, "VariableHobA: PeiServicesCreateHob failed %r\n", Status));
    return Status;
  }

  GuidHob = (EFI_HOB_GUID_TYPE *)NewHob;
  CopyGuid (&GuidHob->Name, &gVariableHobPpiGuid);
  CopyMem ((VOID *)(GuidHob + 1), Buffer, BufferSize);

  DEBUG((DEBUG_INFO, "VariableHobA: Created GUID HOB payload=%u\n", (UINT32)BufferSize));
  return EFI_SUCCESS;
}

/* PEIM Entry point - install PPI */
EFI_STATUS
EFIAPI
VariableHobAEntry (
  IN EFI_PEI_FILE_HANDLE       FileHandle,
  IN CONST EFI_PEI_SERVICES   **PeiServices
  )
{
  EFI_STATUS Status;

  DEBUG ((DEBUG_INFO, "VariableHobA: Entry - installing VariableHob PPI\n"));
  Status = PeiServicesInstallPpi (&mVariableHobPpiDesc);
  DEBUG ((DEBUG_INFO, "VariableHobA: InstallPpi returned %r\n", Status));
  return Status;
}
