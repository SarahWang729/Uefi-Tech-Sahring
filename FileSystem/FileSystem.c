/** @file
  This application operates on the file system of the device from which it was loaded. It supports the following commands:
    - Create an empty file
    - Copy a file
    - Read a file and display its content
    - Delete a file
    - Merge two files into a third file
    - Show file information
**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/EfiShellParameters.h>
#include <Guid/FileInfo.h>

//
// Get root directory on the *current storage device* where this
// application is loaded from.
//
STATIC
EFI_STATUS
GetRootOnCurrentDevice (
  IN  EFI_HANDLE         ImageHandle,
  OUT EFI_FILE_PROTOCOL  **Root
  )
{
  EFI_STATUS                       Status;
  EFI_LOADED_IMAGE_PROTOCOL        *LoadedImage;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *SimpleFs;

  if (Root == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *Root = NULL;

  // Get LoadedImage for this image
  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  if (EFI_ERROR (Status)) {
    Print(L"Get LoadedImageProtocol failed: %r\n", Status);
    return Status;
  }

  // Get SimpleFileSystem from the device where we were loaded
  Status = gBS->HandleProtocol (
                  LoadedImage->DeviceHandle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  (VOID **)&SimpleFs
                  );
  if (EFI_ERROR (Status)) {
    Print(L"Get SimpleFileSystemProtocol failed: %r\n", Status);
    return Status;
  }

  // Open the volume and get the root directory handle.
  Status = SimpleFs->OpenVolume (SimpleFs, Root);
  if (EFI_ERROR (Status)) {
    Print(L"OpenVolume failed: %r\n", Status);
    return Status;
  }

  return EFI_SUCCESS;
}

//
// Read a whole file into a buffer (allocated).
// Caller must FreePool(Buffer) when done.
//
STATIC
EFI_STATUS
ReadFileToBuffer (
  IN  EFI_FILE_PROTOCOL  *Root,
  IN  CHAR16             *FileName,
  OUT VOID               **Buffer,
  OUT UINTN              *BufferSize
  )
{
  EFI_STATUS        Status;
  EFI_FILE_PROTOCOL *File;
  EFI_FILE_INFO     *FileInfo;
  UINTN             InfoSize;
  VOID              *LocalBuffer;
  UINTN             Size;

  if (Root == NULL || FileName == NULL || Buffer == NULL || BufferSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *Buffer = NULL;
  *BufferSize = 0;

  // Open the file for read
  Status = Root->Open(
                    Root,
                    &File,
                    FileName,
                    EFI_FILE_MODE_READ,
                    0
                    );
  if (EFI_ERROR (Status)) {
    Print(L"Open file '%s' failed: %r\n", FileName, Status);
    return Status;
  }

  // First, get file size using GetInfo with EFI_FILE_INFO_GUID.
  InfoSize = 0;
  Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, NULL);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    Print(L"GetInfo (size) failed: %r\n", Status);
    File->Close(File);
    return Status;
  }

  FileInfo = AllocatePool (InfoSize);
  if (FileInfo == NULL) {
    File->Close(File);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, FileInfo);
  if (EFI_ERROR (Status)) {
    Print(L"GetInfo failed: %r\n", Status);
    FreePool(FileInfo);
    File->Close(File);
    return Status;
  }

  Size = (UINTN) FileInfo->FileSize;
  LocalBuffer = AllocatePool (Size + 1);  // +1 for ASCII NUL if text
  if (LocalBuffer == NULL) {
    FreePool(FileInfo);
    File->Close(File);
    return EFI_OUT_OF_RESOURCES;
  }

  // Read file content
  Status = File->Read(File, &Size, LocalBuffer);
  if (EFI_ERROR (Status)) {
    Print(L"Read file failed: %r\n", Status);
    FreePool(LocalBuffer);
    FreePool(FileInfo);
    File->Close(File);
    return Status;
  }

  // Close file and free info
  File->Close(File);
  FreePool(FileInfo);

  *Buffer = LocalBuffer;
  *BufferSize = Size;
  return EFI_SUCCESS;
}

//
// Create (empty) file or copy from source file.
//
STATIC
EFI_STATUS
DoCreateOrCopy (
  IN EFI_FILE_PROTOCOL *Root,
  IN CHAR16            *SrcFile OPTIONAL,
  IN CHAR16            *DstFile
  )
{
  EFI_STATUS       Status;
  EFI_FILE_PROTOCOL *Dst;
  VOID             *Buffer = NULL;
  UINTN            Size = 0;

  if (Root == NULL || DstFile == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (SrcFile != NULL) {
    // Copy: read source file into memory
    Status = ReadFileToBuffer(Root, SrcFile, &Buffer, &Size);
    if (EFI_ERROR(Status)) {
      return Status;
    }
  }

  // Open/create destination file
  Status = Root->Open(
                    Root,
                    &Dst,
                    DstFile,
                    EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                    EFI_FILE_ARCHIVE
                    );
  if (EFI_ERROR (Status)) {
    Print(L"Create/open dest file '%s' failed: %r\n", DstFile, Status);
    if (Buffer != NULL) {
      FreePool(Buffer);
    }
    return Status;
  }

  if (Buffer != NULL && Size > 0) {
    UINTN WriteSize = Size;
    Status = Dst->Write(Dst, &WriteSize, Buffer);
    if (EFI_ERROR (Status)) {
      Print(L"Write dest file failed: %r\n", Status);
    } else {
      Print(L"Copied %d bytes from '%s' to '%s'\n", WriteSize, SrcFile, DstFile);
    }
  } else {
    // Just create empty file
    Print(L"Created empty file '%s'\n", DstFile);
  }

  Dst->Close(Dst);

  if (Buffer != NULL) {
    FreePool(Buffer);
  }

  return Status;
}

//
// Read file and display (assume ASCII text; print with %a).
//
STATIC
EFI_STATUS
DoReadAndDisplay (
  IN EFI_FILE_PROTOCOL *Root,
  IN CHAR16            *FileName
  )
{
  EFI_STATUS Status;
  VOID       *Buffer;
  UINTN      Size;

  Status = ReadFileToBuffer(Root, FileName, &Buffer, &Size);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  ((CHAR8*)Buffer)[Size] = '\0'; // ensure NUL for %a
  Print(L"===== %s =====\n", FileName);
  Print(L"%a\n", (CHAR8*)Buffer);
  Print(L"===== END =====\n");

  FreePool(Buffer);
  return EFI_SUCCESS;
}

//
// Delete a file.
//
STATIC
EFI_STATUS
DoDeleteFile (
  IN EFI_FILE_PROTOCOL *Root,
  IN CHAR16            *FileName
  )
{
  EFI_STATUS        Status;
  EFI_FILE_PROTOCOL *File;

  if (Root == NULL || FileName == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = Root->Open(
                    Root,
                    &File,
                    FileName,
                    EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                    0
                    );
  if (EFI_ERROR(Status)) {
    Print(L"Open file '%s' for delete failed: %r\n", FileName, Status);
    return Status;
  }

  Status = File->Delete(File);
  if (Status == EFI_SUCCESS || Status == EFI_WARN_DELETE_FAILURE) {
    Print(L"Delete file '%s' status: %r\n", FileName, Status);
  } else {
    Print(L"Delete file '%s' failed: %r\n", FileName, Status);
  }

  // On success, Delete() already closes the handle.
  return Status;
}

//
// Merge two files into a destination file: src1 + src2 -> dst.
//
STATIC
EFI_STATUS
DoMergeFiles (
  IN EFI_FILE_PROTOCOL *Root,
  IN CHAR16            *Src1,
  IN CHAR16            *Src2,
  IN CHAR16            *Dst
  )
{
  EFI_STATUS        Status;
  EFI_FILE_PROTOCOL *DstFile;
  VOID              *Buf1 = NULL;
  VOID              *Buf2 = NULL;
  UINTN             Size1 = 0;
  UINTN             Size2 = 0;

  if (Root == NULL || Src1 == NULL || Src2 == NULL || Dst == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = ReadFileToBuffer(Root, Src1, &Buf1, &Size1);
  if (EFI_ERROR(Status)) {
    goto Done;
  }

  Status = ReadFileToBuffer(Root, Src2, &Buf2, &Size2);
  if (EFI_ERROR(Status)) {
    goto Done;
  }

  Status = Root->Open(
                    Root,
                    &DstFile,
                    Dst,
                    EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                    EFI_FILE_ARCHIVE
                    );
  if (EFI_ERROR(Status)) {
    Print(L"Open/create dst '%s' failed: %r\n", Dst, Status);
    goto Done;
  }

  if (Size1 > 0) {
    UINTN WriteSize = Size1;
    Status = DstFile->Write(DstFile, &WriteSize, Buf1);
    if (EFI_ERROR(Status)) {
      Print(L"Write first file data failed: %r\n", Status);
      DstFile->Close(DstFile);
      goto Done;
    }
  }

  if (Size2 > 0) {
    UINTN WriteSize = Size2;
    Status = DstFile->Write(DstFile, &WriteSize, Buf2);
    if (EFI_ERROR(Status)) {
      Print(L"Write second file data failed: %r\n", Status);
      DstFile->Close(DstFile);
      goto Done;
    }
  }

  Print(L"Merged '%s' (%d bytes) + '%s' (%d bytes) -> '%s'\n",
        Src1, (INT32)Size1, Src2, (INT32)Size2, Dst);

  DstFile->Close(DstFile);

Done:
  if (Buf1 != NULL) {
    FreePool(Buf1);
  }
  if (Buf2 != NULL) {
    FreePool(Buf2);
  }
  return Status;
}

//
// Show file information using EFI_FILE_INFO.
//
STATIC
EFI_STATUS
DoShowFileInfo (
  IN EFI_FILE_PROTOCOL *Root,
  IN CHAR16            *FileName
  )
{
  EFI_STATUS        Status;
  EFI_FILE_PROTOCOL *File;
  UINTN             InfoSize = 0;
  EFI_FILE_INFO     *Info;

  if (Root == NULL || FileName == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = Root->Open(
                    Root,
                    &File,
                    FileName,
                    EFI_FILE_MODE_READ,
                    0
                    );
  if (EFI_ERROR(Status)) {
    Print(L"Open file '%s' failed: %r\n", FileName, Status);
    return Status;
  }

  Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, NULL);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    Print(L"GetInfo(size) failed: %r\n", Status);
    File->Close(File);
    return Status;
  }

  Info = AllocatePool(InfoSize);
  if (Info == NULL) {
    File->Close(File);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, Info);
  if (EFI_ERROR(Status)) {
    Print(L"GetInfo failed: %r\n", Status);
    FreePool(Info);
    File->Close(File);
    return Status;
  }

  
  Print(L"File: %s\n", Info->FileName);
  Print(L"  Size:          %d bytes\n", (INT32)Info->FileSize);
  Print(L"  PhysicalSize:  %d bytes\n", (INT32)Info->PhysicalSize);
  Print(L"  Attribute:     0x%lx\n", Info->Attribute);

  Print(L"  CreateTime:    %04d-%02d-%02d %02d:%02d:%02d\n",
        Info->CreateTime.Year, Info->CreateTime.Month, Info->CreateTime.Day,
        Info->CreateTime.Hour, Info->CreateTime.Minute, Info->CreateTime.Second);
  Print(L"  LastAccessTime:%04d-%02d-%02d %02d:%02d:%02d\n",
        Info->LastAccessTime.Year, Info->LastAccessTime.Month, Info->LastAccessTime.Day,
        Info->LastAccessTime.Hour, Info->LastAccessTime.Minute, Info->LastAccessTime.Second);
  Print(L"  Modification:  %04d-%02d-%02d %02d:%02d:%02d\n",
        Info->ModificationTime.Year, Info->ModificationTime.Month, Info->ModificationTime.Day,
        Info->ModificationTime.Hour, Info->ModificationTime.Minute, Info->ModificationTime.Second);

  FreePool(Info);
  File->Close(File);
  return EFI_SUCCESS;
}

STATIC
VOID
PrintUsage (
  VOID
  )
{
  Print(L"\nFileSystem.efi usage:\n");
  Print(L"  -c dst              Create empty file\n");
  Print(L"  -c src dst          Copy file\n");
  Print(L"  -r file             Read and display file\n");
  Print(L"  -d file             Delete file\n");
  Print(L"  -m src1 src2 dst    Merge two files\n");
  Print(L"  -i file             Show file information\n");
}

EFI_STATUS
EFIAPI
FileSystemMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                      Status;
  EFI_FILE_PROTOCOL               *Root;
  EFI_SHELL_PARAMETERS_PROTOCOL   *ShellParams;
  UINTN                           Argc;
  CHAR16                          **Argv;

  Status = GetRootOnCurrentDevice(ImageHandle, &Root);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  // Get Argc/Argv from ShellParametersProtocol.
  Status = gBS->OpenProtocol(
                  ImageHandle,
                  &gEfiShellParametersProtocolGuid,
                  (VOID **)&ShellParams,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR(Status)) {
    Print(L"Get ShellParametersProtocol failed: %r\n", Status);
    PrintUsage();
    Root->Close(Root);
    return Status;
  }

  Argc = ShellParams->Argc;
  Argv = ShellParams->Argv;

  if (Argc < 2) {
    PrintUsage();
    Root->Close(Root);
    return EFI_SUCCESS;
  }

  if (StrCmp(Argv[1], L"-c") == 0) {
    if (Argc == 3) {
      // create empty
      Status = DoCreateOrCopy(Root, NULL, Argv[2]);
    } else if (Argc == 4) {
      // copy
      Status = DoCreateOrCopy(Root, Argv[2], Argv[3]);
    } else {
      PrintUsage();
      Status = EFI_INVALID_PARAMETER;
    }
  } else if (StrCmp(Argv[1], L"-r") == 0) {
    if (Argc == 3) {
      Status = DoReadAndDisplay(Root, Argv[2]);
    } else {
      PrintUsage();
      Status = EFI_INVALID_PARAMETER;
    }
  } else if (StrCmp(Argv[1], L"-d") == 0) {
    if (Argc == 3) {
      Status = DoDeleteFile(Root, Argv[2]);
    } else {
      PrintUsage();
      Status = EFI_INVALID_PARAMETER;
    }
  } else if (StrCmp(Argv[1], L"-m") == 0) {
    if (Argc == 5) {
      Status = DoMergeFiles(Root, Argv[2], Argv[3], Argv[4]);
    } else {
      PrintUsage();
      Status = EFI_INVALID_PARAMETER;
    }
  } else if (StrCmp(Argv[1], L"-i") == 0) {
    if (Argc == 3) {
      Status = DoShowFileInfo(Root, Argv[2]);
    } else {
      PrintUsage();
      Status = EFI_INVALID_PARAMETER;
    }
  } else {
    PrintUsage();
    Status = EFI_INVALID_PARAMETER;
  }

  Root->Close(Root);
  return Status;
}
