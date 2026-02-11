#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/PrintLib.h>
#include <Guid/Acpi.h>
#include <IndustryStandard/Acpi50.h>

#define FADT_SIGNATURE  SIGNATURE_32('F','A','C','P')

//
// Interactive menu
//
VOID
ShowMenu()
{
    Print(L"\n====== ACPI FADT Dump Tool ======\n");
    Print(L"1. Dump FADT\n");
    Print(L"0. Exit\n");
    Print(L"Select: ");
}

//
// Locate ACPI table in XSDT or RSDT (UEFI method)
//
EFI_STATUS
LocateAcpiTableBySignature(
    IN  UINT32                      Signature,
    OUT EFI_ACPI_DESCRIPTION_HEADER **Table
)
{
    EFI_CONFIGURATION_TABLE *Config = gST->ConfigurationTable;
    EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp = NULL;

    EFI_GUID Acpi20Guid = EFI_ACPI_20_TABLE_GUID;
    EFI_GUID Acpi10Guid = EFI_ACPI_TABLE_GUID;

    for (UINTN i = 0; i < gST->NumberOfTableEntries; i++) {

        if (CompareGuid(&Config[i].VendorGuid, &Acpi20Guid) ||
            CompareGuid(&Config[i].VendorGuid, &Acpi10Guid)) {

            Rsdp = (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *)
                     Config[i].VendorTable;
            break;
        }
    }

    if (Rsdp == NULL)
        return EFI_NOT_FOUND;

    //
    // Try XSDT first (ACPI 2.0+)
    //
    EFI_ACPI_DESCRIPTION_HEADER *Xsdt =
        (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Rsdp->XsdtAddress;

    if (Xsdt &&
        Xsdt->Signature ==
            EFI_ACPI_2_0_EXTENDED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) {

        UINTN EntryCount =
            (Xsdt->Length - sizeof(EFI_ACPI_DESCRIPTION_HEADER)) / sizeof(UINT64);
        UINT64 *EntryPtr = (UINT64 *)(Xsdt + 1);

        for (UINTN j = 0; j < EntryCount; j++) {
            EFI_ACPI_DESCRIPTION_HEADER *Hdr =
                (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)EntryPtr[j];

            if (Hdr->Signature == Signature) {
                *Table = Hdr;
                return EFI_SUCCESS;
            }
        }
    }

    //
    // Fallback → RSDT
    //
    EFI_ACPI_DESCRIPTION_HEADER *Rsdt =
        (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)Rsdp->RsdtAddress;

    if (Rsdt) {

        UINTN EntryCount =
            (Rsdt->Length - sizeof(EFI_ACPI_DESCRIPTION_HEADER)) / sizeof(UINT32);

        UINT32 *EntryPtr = (UINT32 *)(Rsdt + 1);

        for (UINTN j = 0; j < EntryCount; j++) {

            EFI_ACPI_DESCRIPTION_HEADER *Hdr =
                (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)EntryPtr[j];

            if (Hdr->Signature == Signature) {
                *Table = Hdr;
                return EFI_SUCCESS;
            }
        }
    }

    return EFI_NOT_FOUND;
}

//
// Helper: Print GAS register
//
VOID
DumpGas(IN EFI_ACPI_5_0_GENERIC_ADDRESS_STRUCTURE *Gas, IN CHAR16 *Name)
{
    Print(L"%s: Addr=0x%lx, Width=%d, Offset=%d, AccessSize=%d\n",
          Name,
          Gas->Address,
          Gas->RegisterBitWidth,
          Gas->RegisterBitOffset,
          Gas->AccessSize);
}

//
// Dump all important FADT ACPI 5.0 fields
//
VOID
DumpFadtInfo(EFI_ACPI_5_0_FIXED_ACPI_DESCRIPTION_TABLE *Fadt)
{
    Print(L"\n========== FADT 5.0 DUMP ==========\n");

    Print(L"FADT @ 0x%lx\n", Fadt);
    Print(L"Length:           %d\n", Fadt->Header.Length);
    Print(L"Revision:         %d\n", Fadt->Header.Revision);

    Print(L"\n===== Basic Addresses =====\n");
    Print(L"FirmwareCtrl:     0x%x\n", Fadt->FirmwareCtrl);
    Print(L"Dsdt:             0x%x\n", Fadt->Dsdt);
    Print(L"XFirmwareCtrl:    0x%lx\n", Fadt->XFirmwareCtrl);
    Print(L"XDsdt:            0x%lx\n", Fadt->XDsdt);

    Print(L"\n===== Power Management Registers =====\n");
    Print(L"PM1aEvtBlk:       0x%x\n", Fadt->Pm1aEvtBlk);
    Print(L"PM1bEvtBlk:       0x%x\n", Fadt->Pm1bEvtBlk);
    Print(L"PM1aCntBlk:       0x%x\n", Fadt->Pm1aCntBlk);
    Print(L"PM1bCntBlk:       0x%x\n", Fadt->Pm1bCntBlk);
    Print(L"PM2CntBlk:        0x%x\n", Fadt->Pm2CntBlk);
    Print(L"PmTmrBlk:         0x%x\n", Fadt->PmTmrBlk);
    Print(L"Gpe0Blk:          0x%x\n", Fadt->Gpe0Blk);
    Print(L"Gpe1Blk:          0x%x\n", Fadt->Gpe1Blk);

    Print(L"\n===== GAS (64-bit Extended Registers) =====\n");
    DumpGas(&Fadt->XPm1aEvtBlk, L"XPm1aEvtBlk");
    DumpGas(&Fadt->XPm1bEvtBlk, L"XPm1bEvtBlk");
    DumpGas(&Fadt->XPm1aCntBlk, L"XPm1aCntBlk");
    DumpGas(&Fadt->XPm1bCntBlk, L"XPm1bCntBlk");
    DumpGas(&Fadt->XPm2CntBlk,  L"XPm2CntBlk");
    DumpGas(&Fadt->XPmTmrBlk,   L"XPmTmrBlk");
    DumpGas(&Fadt->XGpe0Blk,    L"XGpe0Blk");
    DumpGas(&Fadt->XGpe1Blk,    L"XGpe1Blk");

    Print(L"\n===== ACPI Enable/Disable =====\n");
    Print(L"SmiCmd:           0x%x\n", Fadt->SmiCmd);
    Print(L"ACPI Enable:      0x%x\n", Fadt->AcpiEnable);
    Print(L"ACPI Disable:     0x%x\n", Fadt->AcpiDisable);
    Print(L"S4BiosReq:        0x%x\n", Fadt->S4BiosReq);
    Print(L"SCI Interrupt:    %d\n", Fadt->SciInt);

    Print(L"\n===== Sleep Registers =====\n");
    DumpGas(&Fadt->SleepControlReg, L"SleepControlReg");
    DumpGas(&Fadt->SleepStatusReg,  L"SleepStatusReg");

    Print(L"\n===== Flags =====\n");
    Print(L"Flags:            0x%x\n", Fadt->Flags);
}

//
// UEFI Application Entry
//
EFI_STATUS
EFIAPI
UefiMain(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
)
{
    UINTN Choice;
    EFI_STATUS Status;

    while (TRUE) {
        ShowMenu();
        Input(L"%d", &Choice);

        if (Choice == 0)
            break;

        if (Choice == 1) {
            EFI_ACPI_DESCRIPTION_HEADER *Table = NULL;

            Status = LocateAcpiTableBySignature(FADT_SIGNATURE, &Table);

            if (EFI_ERROR(Status)) {
                Print(L"[Error] Unable to find FADT.\n");
                continue;
            }

            DumpFadtInfo((EFI_ACPI_5_0_FIXED_ACPI_DESCRIPTION_TABLE *)Table);
        }
    }

    return EFI_SUCCESS;
}