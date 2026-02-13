/** @file
  PIRQ Routing Table Dumper (UEFI Shell App)
  - Scan legacy memory 0xF0000~0xFFFFF for "$PIR"
  - Validate Version(1.0), TableSize(>header & multiple of 16), Checksum==0
  - Menu:
      1) Get PIRQ Routing Table Header
      2) Dump Slot IRQ Routing (Left/Right paging), ESC quit
**/

#include "PirqDump.h"

STATIC
BOOLEAN
IsEsc (
  IN EFI_INPUT_KEY *Key
  )
{
  if (Key == NULL) {
    return FALSE;
  }
  return (BOOLEAN)(Key->ScanCode == SCAN_ESC);
}

STATIC
BOOLEAN
IsLeft (
  IN EFI_INPUT_KEY *Key
  )
{
  if (Key == NULL) {
    return FALSE;
  }
  return (BOOLEAN)(Key->ScanCode == SCAN_LEFT);
}

STATIC
BOOLEAN
IsRight (
  IN EFI_INPUT_KEY *Key
  )
{
  if (Key == NULL) {
    return FALSE;
  }
  return (BOOLEAN)(Key->ScanCode == SCAN_RIGHT);
}

VOID
WaitAnyKey (
  VOID
  )
{
  EFI_INPUT_KEY Key;
  UINTN         Index;

  Print(L"\nPress any key to continuous..");
  gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
  gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
  Print(L"\n");
}

STATIC
UINT8
Checksum8 (
  IN CONST UINT8 *Data,
  IN UINTN        Size
  )
{
  UINTN i;
  UINT8 Sum;

  Sum = 0;
  for (i = 0; i < Size; i++) {
    Sum = (UINT8)(Sum + Data[i]);
  }
  return Sum;
}

STATIC
EFI_STATUS
RbMemRead (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *RbIo,
  IN  EFI_PHYSICAL_ADDRESS             Address,
  IN  VOID                            *Buffer,
  IN  UINTN                            Size
  )
{
  if (RbIo == NULL || Buffer == NULL || Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  /* Read byte-wise for simplicity and compatibility in legacy range */
  return RbIo->Mem.Read(
                  RbIo,
                  EfiPciWidthUint8,
                  (UINT64)Address,
                  Size,
                  Buffer
                  );
}

EFI_STATUS
FindPirqTable (
  OUT EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  **OutRbIo,
  OUT EFI_PHYSICAL_ADDRESS             *OutTableAddr,
  OUT PIR_TABLE_HEADER                 *OutHeaderCopy
  )
{
  EFI_STATUS Status;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *RbIo;

  EFI_PHYSICAL_ADDRESS Addr;
  UINT32 Sig;
  PIR_TABLE_HEADER Hdr;

  RbIo = NULL;

  if (OutRbIo == NULL || OutTableAddr == NULL || OutHeaderCopy == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->LocateProtocol(&gEfiPciRootBridgeIoProtocolGuid, NULL, (VOID**)&RbIo);
  if (EFI_ERROR(Status) || RbIo == NULL) {
    return EFI_NOT_FOUND;
  }

  /* Per spec/training: Search 0xF0000~0xFFFFF for "$PIR". Scan 16-byte boundary */
  for (Addr = 0xF0000; Addr <= 0xFFFF0; Addr += 0x10) {

    Status = RbMemRead(RbIo, Addr, &Sig, sizeof(Sig));
    if (EFI_ERROR(Status)) {
      continue;
    }
    if (Sig != PIR_SIGNATURE) {
      continue;
    }

    Status = RbMemRead(RbIo, Addr, &Hdr, sizeof(Hdr));
    if (EFI_ERROR(Status)) {
      continue;
    }

    /* Version 1.0 */
    if (Hdr.Version != 0x0100) {
      continue;
    }

    /* TableSize must be > header and multiple of 16 */
    if (Hdr.TableSize <= (UINT16)sizeof(PIR_TABLE_HEADER)) {
      continue;
    }
    if ((Hdr.TableSize % 16) != 0) {
      continue;
    }

    /* checksum validate (sum of all bytes in table == 0) */
    {
      UINT8 *Tmp;

      Tmp = (UINT8*)AllocatePool((UINTN)Hdr.TableSize);
      if (Tmp == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      Status = RbMemRead(RbIo, Addr, Tmp, (UINTN)Hdr.TableSize);
      if (!EFI_ERROR(Status)) {
        if (Checksum8(Tmp, (UINTN)Hdr.TableSize) == 0) {
          CopyMem(OutHeaderCopy, &Hdr, sizeof(Hdr));
          *OutRbIo      = RbIo;
          *OutTableAddr = Addr;
          FreePool(Tmp);
          return EFI_SUCCESS;
        }
      }

      FreePool(Tmp);
    }
  }

  return EFI_NOT_FOUND;
}

VOID
ShowMainMenu (
  VOID
  )
{
  gST->ConOut->ClearScreen(gST->ConOut);
  Print(L"PIRQ Routing Table\n\n");
  Print(L"1. Get PIRQ Routing Table Header.\n");
  Print(L"2. Dump Slot IRQ Routing.\n\n");
  Print(L"Press [ESC] to Quit.\n");
  Print(L"-> ");
}

VOID
DumpHeaderScreen (
  IN EFI_PHYSICAL_ADDRESS  TableAddr,
  IN PIR_TABLE_HEADER     *Hdr
  )
{
  UINT8 Major;
  UINT8 Minor;

  UINT8 RouterDev;
  UINT8 RouterFunc;

  UINT16 CompVendor;
  UINT16 CompDevId;

  if (Hdr == NULL) {
    return;
  }

  Major = (UINT8)(Hdr->Version >> 8);
  Minor = (UINT8)(Hdr->Version & 0xFF);

  RouterDev  = (UINT8)(Hdr->RouterDevFunc >> 3);
  RouterFunc = (UINT8)(Hdr->RouterDevFunc & 0x07);

  CompVendor = (UINT16)(Hdr->CompatibleRouter & 0xFFFF);
  CompDevId  = (UINT16)((Hdr->CompatibleRouter >> 16) & 0xFFFF);

  gST->ConOut->ClearScreen(gST->ConOut);
  Print(L"PIRQ Routing Table Header\n\n");

  Print(L"Signature                           = %c%c%c%c\n",
        (CHAR16)(Hdr->Signature & 0xFF),
        (CHAR16)((Hdr->Signature >> 8) & 0xFF),
        (CHAR16)((Hdr->Signature >> 16) & 0xFF),
        (CHAR16)((Hdr->Signature >> 24) & 0xFF));

  Print(L"Minor Version                       = 0x%02x\n", Minor);
  Print(L"Major Version                       = 0x%02x\n", Major);
  Print(L"Table Size                          = %u Byte\n", (UINTN)Hdr->TableSize);

  Print(L"PCI Interrupt Router's Bus          = 0x%02x\n", Hdr->RouterBus);
  Print(L"PCI Interrupt Router's Device Number= 0x%02x\n", RouterDev);
  Print(L"PCI Interrupt Router's Function Number= 0x%02x\n", RouterFunc);

  Print(L"PCI Exclusive IRQs                  = 0x%04x\n", Hdr->PciExclusiveIrqs);
  Print(L"Compatible PCI Interrupt Router Vendor ID = 0x%04x\n", CompVendor);
  Print(L"Compatible PCI Interrupt Router Device ID = 0x%04x\n", CompDevId);

  Print(L"Miniport Data                       = 0x%08x\n", Hdr->MiniportData);

  Print(L"Reserved                            =");
  {
    UINTN i;
    for (i = 0; i < sizeof(Hdr->Reserved); i++) {
      Print(L" %02x", Hdr->Reserved[i]);
    }
  }
  Print(L"\n");

  Print(L"Checksum                            = 0x%02x\n", Hdr->Checksum);
  Print(L"\nTable Address                        = 0x%05lx\n", (UINT64)TableAddr);
}

STATIC
VOID
PrintIrqBitmap (
  IN UINT16 Bitmap
  )
{
  BOOLEAN First;
  UINTN   Irq;

  First = TRUE;

  Print(L"0x%04x (", Bitmap);

  for (Irq = 0; Irq < 16; Irq++) {
    if (Bitmap & (1u << Irq)) {
      if (!First) {
        Print(L",");
      }
      Print(L"%u", (UINTN)Irq);
      First = FALSE;
    }
  }

  Print(L")");
}

EFI_STATUS
DumpSlotPage (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *RbIo,
  IN EFI_PHYSICAL_ADDRESS             TableAddr,
  IN PIR_TABLE_HEADER                *Hdr,
  IN UINTN                            SlotIndex
  )
{
  EFI_STATUS Status;
  PIR_SLOT_ENTRY Slot;
  UINTN SlotCount;
  EFI_PHYSICAL_ADDRESS SlotAddr;
  UINT8 Dev;

  if (RbIo == NULL || Hdr == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (Hdr->TableSize < sizeof(PIR_TABLE_HEADER)) {
    return EFI_COMPROMISED_DATA;
  }

  SlotCount = (Hdr->TableSize - sizeof(PIR_TABLE_HEADER)) / sizeof(PIR_SLOT_ENTRY);
  if (SlotCount == 0) {
    return EFI_NOT_FOUND;
  }
  if (SlotIndex >= SlotCount) {
    return EFI_INVALID_PARAMETER;
  }

  SlotAddr = TableAddr + sizeof(PIR_TABLE_HEADER) + (EFI_PHYSICAL_ADDRESS)(SlotIndex * sizeof(PIR_SLOT_ENTRY));
  Status = RbMemRead(RbIo, SlotAddr, &Slot, sizeof(Slot));
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Dev = (UINT8)(Slot.PciDeviceNumber >> 3);

  gST->ConOut->ClearScreen(gST->ConOut);

  Print(L"%02uth Slot IRQ Routing\n\n", (UINTN)(SlotIndex + 1));

  Print(L"PCI Bus Number          = 0x%02x\n", Slot.PciBusNumber);
  Print(L"PCI Device Number       = 0x%02x\n", Dev);

  Print(L"Link Value for INTA#     = 0x%02x\n", Slot.LinkValueIntA);
  Print(L"IRQ Bitmap for INTA#     = "); PrintIrqBitmap(Slot.IrqBitmapIntA); Print(L"\n");

  Print(L"Link Value for INTB#     = 0x%02x\n", Slot.LinkValueIntB);
  Print(L"IRQ Bitmap for INTB#     = "); PrintIrqBitmap(Slot.IrqBitmapIntB); Print(L"\n");

  Print(L"Link Value for INTC#     = 0x%02x\n", Slot.LinkValueIntC);
  Print(L"IRQ Bitmap for INTC#     = "); PrintIrqBitmap(Slot.IrqBitmapIntC); Print(L"\n");

  Print(L"Link Value for INTD#     = 0x%02x\n", Slot.LinkValueIntD);
  Print(L"IRQ Bitmap for INTD#     = "); PrintIrqBitmap(Slot.IrqBitmapIntD); Print(L"\n");

  Print(L"Slot Number              = 0x%02x\n", Slot.SlotNumber);
  Print(L"Reserved                 = 0x%02x\n", Slot.Reserved);

  Print(L"\nPress [Right][Left] to control slot page...\n");
  Print(L"Press [ESC] to quit...\n");

  Print(L"\n(%u / %u)  TableAddr=0x%05lx  SlotAddr=0x%05lx\n",
        (UINTN)(SlotIndex + 1),
        SlotCount,
        (UINT64)TableAddr,
        (UINT64)SlotAddr);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;

  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *RbIo;
  EFI_PHYSICAL_ADDRESS TableAddr;
  PIR_TABLE_HEADER Hdr;

  EFI_INPUT_KEY Key;
  UINTN Index;

  RbIo = NULL;
  TableAddr = 0;

  Status = FindPirqTable(&RbIo, &TableAddr, &Hdr);
  if (EFI_ERROR(Status)) {
    Print(L"ERROR: Cannot find valid $PIR table in 0xF0000~0xFFFFF (Status=%r)\n", Status);
    Print(L"Tip: Some pure-UEFI/APIC platforms may not provide legacy $PIR.\n");
    return Status;
  }

  while (TRUE) {
    ShowMainMenu();

    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

    if (IsEsc(&Key)) {
      gST->ConOut->ClearScreen(gST->ConOut);
      return EFI_SUCCESS;
    }

    if (Key.UnicodeChar == L'1') {
      DumpHeaderScreen(TableAddr, &Hdr);
      WaitAnyKey();
      continue;
    }

    if (Key.UnicodeChar == L'2') {
      UINTN SlotCount;
      UINTN Cur;

      SlotCount = (Hdr.TableSize - sizeof(PIR_TABLE_HEADER)) / sizeof(PIR_SLOT_ENTRY);
      Cur = 0;

      if (SlotCount == 0) {
        Print(L"\nNo slot entry found.\n");
        WaitAnyKey();
        continue;
      }

      DumpSlotPage(RbIo, TableAddr, &Hdr, Cur);

      while (TRUE) {
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &Index);
        gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

        if (IsEsc(&Key)) {
          break;
        }
        if (IsRight(&Key)) {
          if (Cur + 1 < SlotCount) {
            Cur++;
          }
          DumpSlotPage(RbIo, TableAddr, &Hdr, Cur);
          continue;
        }
        if (IsLeft(&Key)) {
          if (Cur > 0) {
            Cur--;
          }
          DumpSlotPage(RbIo, TableAddr, &Hdr, Cur);
          continue;
        }
      }

      continue;
    }

    /* ignore other keys */
  }

  /* never reach */
  /* return EFI_SUCCESS; */
}
