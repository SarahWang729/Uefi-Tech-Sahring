/** @file
  PIRQ Routing Table Dumper - Header
**/

#ifndef __PIRQ_DUMP_H__
#define __PIRQ_DUMP_H__

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/PciRootBridgeIo.h>

#define PIR_SIGNATURE  SIGNATURE_32('$','P','I','R')

#pragma pack(push, 1)

typedef struct {
  UINT32  Signature;          /* "$PIR" */
  UINT16  Version;            /* 1.0 => 0x0100 */
  UINT16  TableSize;          /* bytes, >32, multiple of 16 */
  UINT8   RouterBus;          /* PCI Interrupt Router's Bus */
  UINT8   RouterDevFunc;      /* DevFunc: [7:3]=Dev, [2:0]=Func */
  UINT16  PciExclusiveIrqs;   /* IRQ bitmap devoted exclusively to PCI usage */
  UINT32  CompatibleRouter;   /* VendorID (low16) | DeviceID (high16) */
  UINT32  MiniportData;
  UINT8   Reserved[11];
  UINT8   Checksum;           /* checksum of entire table == 0 */
  /* followed by SlotEntry[16*N] */
} PIR_TABLE_HEADER;

typedef struct {
  UINT8   PciBusNumber;
  UINT8   PciDeviceNumber;    /* [7:3]=Device, [2:0]=0 */
  UINT8   LinkValueIntA;
  UINT16  IrqBitmapIntA;
  UINT8   LinkValueIntB;
  UINT16  IrqBitmapIntB;
  UINT8   LinkValueIntC;
  UINT16  IrqBitmapIntC;
  UINT8   LinkValueIntD;
  UINT16  IrqBitmapIntD;
  UINT8   SlotNumber;
  UINT8   Reserved;
} PIR_SLOT_ENTRY;

#pragma pack(pop)

/* Core functions */
EFI_STATUS
FindPirqTable (
  OUT EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  **OutRbIo,
  OUT EFI_PHYSICAL_ADDRESS             *OutTableAddr,
  OUT PIR_TABLE_HEADER                 *OutHeaderCopy
  );

VOID
ShowMainMenu (
  VOID
  );

VOID
DumpHeaderScreen (
  IN EFI_PHYSICAL_ADDRESS  TableAddr,
  IN PIR_TABLE_HEADER     *Hdr
  );

EFI_STATUS
DumpSlotPage (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *RbIo,
  IN EFI_PHYSICAL_ADDRESS             TableAddr,
  IN PIR_TABLE_HEADER                *Hdr,
  IN UINTN                            SlotIndex
  );

VOID
WaitAnyKey (
  VOID
  );

#endif /* __PIRQ_DUMP_H__ */
