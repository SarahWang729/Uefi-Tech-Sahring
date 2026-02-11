#ifndef _VARIABLE_HOB_PPI_H_
#define _VARIABLE_HOB_PPI_H_

#include <Uefi.h>

/*
  VARIABLE_HOB_PPI
  - Provider PPI that allows a consumer PEIM to ask the provider to:
    1) Read a variable (from ReadOnlyVariable2) into a buffer
    2) Create a GUID HOB containing supplied buffer
*/

#define VARIABLE_HOB_PPI_GUID \
  { 0xd43a6bc4, 0x87b2, 0x4b51, { 0xa2, 0x13, 0x11, 0x3a, 0xe4, 0xbb, 0x71, 0x8c } }

/* Prototype: read a variable (name + guid).*/
typedef
EFI_STATUS
(EFIAPI *PEIM_GET_VARIABLE)(
  IN  CONST CHAR16   *VariableName,
  IN  CONST EFI_GUID *VendorGuid,
  IN OUT UINTN       *DataSize,
  OUT VOID           *Data
  );

/* Prototype: create a GUID HOB whose payload is Buffer. */
typedef
EFI_STATUS
(EFIAPI *PEIM_CREATE_HOB)(
  IN CONST VOID *Buffer,
  IN UINTN       BufferSize
  );

typedef struct {
  PEIM_GET_VARIABLE  GetVariable;
  PEIM_CREATE_HOB    CreateHob;
} VARIABLE_HOB_PPI;

extern EFI_GUID gVariableHobPpiGuid;

#endif 
