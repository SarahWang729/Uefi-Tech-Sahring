/** @file
  This sample application bases on PCI setting 
  to print pci devices to the UEFI Console.

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PciRootBridgeIo.h>
#include <IndustryStandard/Pci.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>

/**
  The user Entry Point for Application. The user code starts with this function
  as the real entry point for the application.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.  
  @param[in] SystemTable    A pointer to the EFI System Table.
  
  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.
**/

EFI_STATUS
EFIAPI
PciMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                      Status;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *PciRootBridgeIo;
  UINT8                           Bus;
  UINT8                           Device;
  UINT8                           Function;
  UINT64                          PciAddress;
  UINT16                          VendorId;
  UINT8                           HeaderType;
  UINT32                          SubsystemInfo;
  UINT16                          SubsystemVendorId;
  UINT16                          SubsystemId;
  UINT8                           BaseClass;
  UINT8                           SubClass;

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
    return Status;
  }

  //
  // Scan all PCI buses, devices, and functions
  //
  for (Bus = 0; Bus < PCI_MAX_BUS; Bus++) {
    for (Device = 0; Device < PCI_MAX_DEVICE; Device++) {
      for (Function = 0; Function < PCI_MAX_FUNC; Function++) {
        //
        // Get the PCI address
        //
        PciAddress = EFI_PCI_ADDRESS (Bus, Device, Function, 0);
        VendorId = 0xFFFF;
        Status = PciRootBridgeIo->Pci.Read (
                                        PciRootBridgeIo,
                                        EfiPciWidthUint16,
                                        PciAddress,
                                        1,
                                        &VendorId
                                        );

        if (!EFI_ERROR (Status) && VendorId != 0xFFFF) {
          //
          // Read Header Type to determine if this is a valid device for Subsystem ID
          //
          PciAddress = EFI_PCI_ADDRESS (Bus, Device, Function, PCI_HEADER_TYPE_OFFSET);
          HeaderType = 0xFF;
          Status = PciRootBridgeIo->Pci.Read (
                                          PciRootBridgeIo,
                                          EfiPciWidthUint8,
                                          PciAddress,
                                          1,
                                          &HeaderType
                                          );

          if (EFI_ERROR (Status)) {
            continue;
          }

          SubsystemVendorId = 0;
          SubsystemId = 0;
          if ((HeaderType & 0x7F) == 0x00 || (HeaderType & 0x7F) == 0x01) {
            PciAddress = EFI_PCI_ADDRESS (Bus, Device, Function, PCI_SUBSYSTEM_VENDOR_ID_OFFSET);
            //
            // Subsystem Vendor ID and Subsystem ID
            // are only valid for Header Type 0x00 and 0x01 devices
            //
            SubsystemInfo = 0xFFFFFFFF;
            Status = PciRootBridgeIo->Pci.Read (
                                            PciRootBridgeIo,
                                            EfiPciWidthUint32,
                                            PciAddress,
                                            1,
                                            &SubsystemInfo
                                            );

            if (!EFI_ERROR (Status)) {
              SubsystemVendorId = SubsystemInfo & 0xFFFF;
              SubsystemId = (SubsystemInfo >> 16) & 0xFFFF;
            }
          }

          //
          // Read Class Code
          //
          PciAddress = EFI_PCI_ADDRESS (Bus, Device, Function, PCI_CLASSCODE_OFFSET);
          BaseClass = 0;
          SubClass = 0;
          Status = PciRootBridgeIo->Pci.Read (
                                          PciRootBridgeIo,
                                          EfiPciWidthUint8,
                                          PciAddress + 1,  // Read SubClass (offset 0x0A)
                                          1,
                                          &SubClass
                                          );
          if (!EFI_ERROR (Status)) {
            Status = PciRootBridgeIo->Pci.Read (
                                            PciRootBridgeIo,
                                            EfiPciWidthUint8,
                                            PciAddress + 2,  // Read BaseClass (offset 0x0B)
                                            1,
                                            &BaseClass
                                            );
          }

          if (EFI_ERROR (Status)) {
            continue;
          }

          if ((BaseClass == 0x06 && SubClass <= 0x04) || (BaseClass == 0x08 && SubClass <= 0x03)) { 
            continue;
          } else {
            Print (L"[PCI] Bus: %02x, Device: %02x, Function: %02x, Class: %02x%02x, SVID: %04x, SSID: %04x\n", 
                  Bus, Device, Function, BaseClass, SubClass, SubsystemVendorId, SubsystemId);
          }  
        }
      }
    }
  } 
  return EFI_SUCCESS;
}