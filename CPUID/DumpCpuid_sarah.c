/** @file
  UEFI Shell App: CPUID (23 hotkey menu) + MSR read/write
  - CPUID menu: 23 items (0-9, A-D, E-M)
  - Enter a hotkey -> show that CPUID function page (decoded)
  - MSR Read/Write with "YES" confirmation
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

#define APP_NAME            L"CpuidMsrTool"
#define INPUT_BUF_CHARS     128

typedef struct {
  CHAR16  HotKey;
  UINT32  Leaf;
  CHAR16 *MenuLine;
} CPUID_MENU_ITEM;

STATIC CPUID_MENU_ITEM mCpuidMenu[23] = {
  {L'0', 0x00000000, L"0. CPUID Function 00"},
  {L'1', 0x00000001, L"1. CPUID Function 01"},
  {L'2', 0x00000002, L"2. CPUID Function 02"},
  {L'3', 0x00000003, L"3. CPUID Function 03"},
  {L'4', 0x00000004, L"4. CPUID Function 04"},
  {L'5', 0x00000005, L"5. CPUID Function 05"},
  {L'6', 0x00000006, L"6. CPUID Function 06"},
  {L'7', 0x00000007, L"7. CPUID Function 07"},
  {L'8', 0x00000008, L"8. CPUID Function 08"},
  {L'9', 0x00000009, L"9. CPUID Function 09"},
  {L'A', 0x0000000A, L"A. CPUID Function 0A"},
  {L'B', 0x0000000B, L"B. CPUID Function 0B"},
  {L'C', 0x0000000C, L"C. CPUID Function 0C"},
  {L'D', 0x0000000D, L"D. CPUID Function 0D"},
  {L'E', 0x80000000, L"E. CPUID Function 80000000"},
  {L'F', 0x80000001, L"F. CPUID Function 80000001"},
  {L'G', 0x80000002, L"G. CPUID Function 80000002"},
  {L'H', 0x80000003, L"H. CPUID Function 80000003"},
  {L'I', 0x80000004, L"I. CPUID Function 80000004"},
  {L'J', 0x80000005, L"J. CPUID Function 80000005"},
  {L'K', 0x80000006, L"K. CPUID Function 80000006"},
  {L'L', 0x80000007, L"L. CPUID Function 80000007"},
  {L'M', 0x80000008, L"M. CPUID Function 80000008"}
};

#define CPUID_MENU_COUNT  (sizeof(mCpuidMenu) / sizeof(mCpuidMenu[0]))

/* ------------------------- Prototypes ------------------------- */
STATIC VOID ClearScreen(VOID);
STATIC VOID WaitAnyKey(VOID);
STATIC VOID CpuidEx(IN UINT32 Leaf, IN UINT32 SubLeaf, OUT UINT32 *Eax, OUT UINT32 *Ebx, OUT UINT32 *Ecx, OUT UINT32 *Edx);

STATIC VOID GetVendorMax(OUT CHAR8 Vendor[13], OUT UINT32 *MaxStd, OUT UINT32 *MaxExt);
STATIC INTN FindMenuIndexByHotkey(IN CHAR16 Key);
STATIC VOID ShowCpuidMenu(VOID);
STATIC VOID RunCpuidMenu(VOID);

STATIC EFI_STATUS ReadLine(OUT CHAR16 *Buffer, IN UINTN BufferChars);
STATIC BOOLEAN ParseHex32Simple(IN CONST CHAR16 *Str, OUT UINT32 *Value);
STATIC BOOLEAN ParseHex64Simple(IN CONST CHAR16 *Str, OUT UINT64 *Value);

STATIC BOOLEAN IsMsrSupported(VOID);
STATIC VOID DoMsrRead(VOID);
STATIC VOID DoMsrWrite(VOID);

STATIC VOID ShowCpuidFunctionPage(IN UINT32 Leaf);

/* CPUID decode pages */
STATIC VOID DecodeLeaf00(VOID);
STATIC VOID DecodeLeaf01(VOID);
STATIC VOID DecodeLeaf02(VOID);
STATIC VOID DecodeLeaf03(VOID);
STATIC VOID DecodeLeaf04(VOID);
STATIC VOID DecodeLeaf05(VOID);
STATIC VOID DecodeLeaf06(VOID);
STATIC VOID DecodeLeaf07(VOID);
STATIC VOID DecodeLeaf08(VOID);
STATIC VOID DecodeLeaf09(VOID);
STATIC VOID DecodeLeaf0A(VOID);
STATIC VOID DecodeLeaf0B(VOID);
STATIC VOID DecodeLeaf0C(VOID);
STATIC VOID DecodeLeaf0D(VOID);

STATIC VOID DecodeLeaf80000000(VOID);
STATIC VOID DecodeLeaf80000001(VOID);
STATIC VOID DecodeLeaf80000002(VOID);
STATIC VOID DecodeLeaf80000003(VOID);
STATIC VOID DecodeLeaf80000004(VOID);
STATIC VOID DecodeLeaf80000005(VOID);
STATIC VOID DecodeLeaf80000006(VOID);
STATIC VOID DecodeLeaf80000007(VOID);
STATIC VOID DecodeLeaf80000008(VOID);

STATIC VOID DumpCpuidRawOne(IN UINT32 Leaf, IN UINT32 Sub);

/* brand string helper (print 16 bytes only, like your screenshots) */
STATIC VOID PrintBrandStringLeaf16(IN UINT32 Leaf);

/* ------------------------- Common helpers ------------------------- */

STATIC
VOID
ClearScreen (
  VOID
  )
{
  gST->ConOut->ClearScreen(gST->ConOut);
}

STATIC
VOID
CpuidEx (
  IN  UINT32 Leaf,
  IN  UINT32 SubLeaf,
  OUT UINT32 *Eax,
  OUT UINT32 *Ebx,
  OUT UINT32 *Ecx,
  OUT UINT32 *Edx
  )
{
  UINT32 A, B, C, D;

  AsmCpuidEx(Leaf, SubLeaf, &A, &B, &C, &D);
  if (Eax) *Eax = A;
  if (Ebx) *Ebx = B;
  if (Ecx) *Ecx = C;
  if (Edx) *Edx = D;
}

STATIC
VOID
WaitAnyKey (
  VOID
  )
{
  EFI_INPUT_KEY Key;

  Print(L"\n(Press any key to continue...)\n");
  while (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_NOT_READY) {
    gBS->Stall(1000);
  }
}

STATIC
VOID
GetVendorMax (
  OUT CHAR8  Vendor[13],
  OUT UINT32 *MaxStd,
  OUT UINT32 *MaxExt
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;

  CpuidEx(0, 0, &Eax, &Ebx, &Ecx, &Edx);
  if (MaxStd) *MaxStd = Eax;

  CopyMem(&Vendor[0], &Ebx, 4);
  CopyMem(&Vendor[4], &Edx, 4);
  CopyMem(&Vendor[8], &Ecx, 4);
  Vendor[12] = '\0';

  CpuidEx(0x80000000, 0, &Eax, &Ebx, &Ecx, &Edx);
  if (MaxExt) *MaxExt = Eax;
}

STATIC
INTN
FindMenuIndexByHotkey (
  IN CHAR16 Key
  )
{
  UINTN  i;
  CHAR16 Up;

  Up = Key;
  if (Up >= L'a' && Up <= L'z') {
    Up = (CHAR16)(Up - L'a' + L'A');
  }

  for (i = 0; i < CPUID_MENU_COUNT; i++) {
    if (mCpuidMenu[i].HotKey == Up) {
      return (INTN)i;
    }
  }
  return -1;
}

STATIC
VOID
ShowCpuidMenu (
  VOID
  )
{
  UINTN i;

  Print(L"\n");
  for (i = 0; i < CPUID_MENU_COUNT; i++) {
    Print(L"%s\n", mCpuidMenu[i].MenuLine);
  }
  Print(L"\n(Press Q to go back)>");
}

/* ------------------------- Hex parser ------------------------- */

STATIC
BOOLEAN
HexCharToNibble (
  IN  CHAR16  Ch,
  OUT UINT8  *Nibble
  )
{
  if (Nibble == NULL) return FALSE;

  if (Ch >= L'0' && Ch <= L'9') { *Nibble = (UINT8)(Ch - L'0'); return TRUE; }
  if (Ch >= L'a' && Ch <= L'f') { *Nibble = (UINT8)(Ch - L'a' + 10); return TRUE; }
  if (Ch >= L'A' && Ch <= L'F') { *Nibble = (UINT8)(Ch - L'A' + 10); return TRUE; }
  return FALSE;
}

STATIC
BOOLEAN
ParseHex64Simple (
  IN  CONST CHAR16 *Str,
  OUT UINT64       *Value
  )
{
  UINT64 V;
  UINT8  N;

  if (Str == NULL || Value == NULL) return FALSE;

  while (*Str == L' ' || *Str == L'\t') Str++;

  if (Str[0] == L'0' && (Str[1] == L'x' || Str[1] == L'X')) {
    Str += 2;
  }

  if (*Str == L'\0') return FALSE;

  V = 0;
  while (*Str != L'\0') {
    if (*Str == L' ' || *Str == L'\t') break;
    if (!HexCharToNibble(*Str, &N)) return FALSE;
    V = (V << 4) | (UINT64)N;
    Str++;
  }

  *Value = V;
  return TRUE;
}

STATIC
BOOLEAN
ParseHex32Simple (
  IN  CONST CHAR16 *Str,
  OUT UINT32       *Value
  )
{
  UINT64 V;

  if (Value == NULL) return FALSE;
  if (!ParseHex64Simple(Str, &V)) return FALSE;
  *Value = (UINT32)V;
  return TRUE;
}

/* ------------------------- CPUID Raw helper ------------------------- */

STATIC
VOID
DumpCpuidRawOne (
  IN UINT32 Leaf,
  IN UINT32 Sub
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;

  CpuidEx(Leaf, Sub, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"Index: %d\n", Sub);
  Print(L"EAX=%08X EBX=%08X ECX=%08X EDX=%08X\n", Eax, Ebx, Ecx, Edx);
  Print(L"\n");
}

/* ------------------------- Brand string leaf(16 bytes) helper ------------------------- */

STATIC
VOID
PrintBrandStringLeaf16 (
  IN UINT32 Leaf
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;
  CHAR8  Str[17];

  ZeroMem(Str, sizeof(Str));

  CpuidEx(Leaf, 0, &Eax, &Ebx, &Ecx, &Edx);
  CopyMem(&Str[0],  &Eax, 4);
  CopyMem(&Str[4],  &Ebx, 4);
  CopyMem(&Str[8],  &Ecx, 4);
  CopyMem(&Str[12], &Edx, 4);
  Str[16] = '\0';

  Print(L"Processor Brand String: %a\n", Str);
}

/* ------------------------- CPUID decode dispatcher ------------------------- */

STATIC
VOID
ShowCpuidFunctionPage (
  IN UINT32 Leaf
  )
{
  ClearScreen();

  switch (Leaf) {
    case 0x00000000: DecodeLeaf00(); break;
    case 0x00000001: DecodeLeaf01(); break;
    case 0x00000002: DecodeLeaf02(); break;
    case 0x00000003: DecodeLeaf03(); break;
    case 0x00000004: DecodeLeaf04(); break;
    case 0x00000005: DecodeLeaf05(); break;
    case 0x00000006: DecodeLeaf06(); break;
    case 0x00000007: DecodeLeaf07(); break;
    case 0x00000008: DecodeLeaf08(); break;
    case 0x00000009: DecodeLeaf09(); break;
    case 0x0000000A: DecodeLeaf0A(); break;
    case 0x0000000B: DecodeLeaf0B(); break;
    case 0x0000000C: DecodeLeaf0C(); break;
    case 0x0000000D: DecodeLeaf0D(); break;

    case 0x80000000: DecodeLeaf80000000(); break;
    case 0x80000001: DecodeLeaf80000001(); break;
    case 0x80000002: DecodeLeaf80000002(); break;
    case 0x80000003: DecodeLeaf80000003(); break;
    case 0x80000004: DecodeLeaf80000004(); break;
    case 0x80000005: DecodeLeaf80000005(); break;
    case 0x80000006: DecodeLeaf80000006(); break;
    case 0x80000007: DecodeLeaf80000007(); break;
    case 0x80000008: DecodeLeaf80000008(); break;

    default:
      Print(L"Function %08X - Raw Dump\n\n", Leaf);
      DumpCpuidRawOne(Leaf, 0);
      break;
  }

  WaitAnyKey();
}

/* ------------------------- Decode 0..9 ------------------------- */

STATIC
VOID
DecodeLeaf00 (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;
  CHAR8  Vendor[13];

  Print(L"Function 0 - Vendor-ID and Largest Standard Function\n");
  CpuidEx(0x00000000, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX=%08X EBX=%08X ECX=%08X EDX=%08X\n", Eax, Ebx, Ecx, Edx);

  CopyMem(&Vendor[0], &Ebx, 4);
  CopyMem(&Vendor[4], &Edx, 4);
  CopyMem(&Vendor[8], &Ecx, 4);
  Vendor[12] = '\0';

  Print(L"Largest Standard function: %d\n", Eax);
  Print(L"Vendor ID: %a\n", Vendor);
}

STATIC
VOID
DecodeLeaf01 (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;
  UINT32 Stepping, Model, Family, ProcType, ExtModel, ExtFamily;
  UINT32 DispFamily, DispModel;

  Print(L"Function 1 - Feature Information\n");

  CpuidEx(0x00000001, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX=%08X EBX=%08X ECX=%08X EDX=%08X\n", Eax, Ebx, Ecx, Edx);

  Print(L"\n");
  Print(L"EAX=%08X EBX=%08X ECX=%08X EDX=%08X\n", Eax, Ebx, Ecx, Edx);
  Print(L"\n");

  Stepping  = (Eax >> 0)  & 0xF;
  Model     = (Eax >> 4)  & 0xF;
  Family    = (Eax >> 8)  & 0xF;
  ProcType  = (Eax >> 12) & 0x3;
  ExtModel  = (Eax >> 16) & 0xF;
  ExtFamily = (Eax >> 20) & 0xFF;

  DispFamily = (Family == 0xF) ? (Family + ExtFamily) : Family;
  DispModel  = ((Family == 0x6) || (Family == 0xF)) ? ((ExtModel << 4) | Model) : Model;

  Print(L"Processor Signature Fields:\n");
  Print(L"  Stepping ID         : %d\n", Stepping);
  Print(L"  Model               : %d\n", Model);
  Print(L"  Family ID           : %d\n", Family);
  Print(L"  Processor Type      : %d  (Original OEM Processor)\n", ProcType);
  Print(L"  Extended Model ID   : %d\n", ExtModel);
  Print(L"  Extended Family ID  : %d\n", ExtFamily);

  Print(L"\nDisplay Family/Model:\n");
  Print(L"  Display Family = %d\n", DispFamily);
  Print(L"  Display Model  = %d\n", DispModel);
}

STATIC
VOID
DecodeLeaf02 (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;

  Print(L"Function 2 - Cache and TLB Descriptor Information\n");
  CpuidEx(0x00000002, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX=%08X EBX=%08X ECX=%08X EDX=%08X\n", Eax, Ebx, Ecx, Edx);
}

STATIC
VOID
DecodeLeaf03 (
  VOID
  )
{
  UINT32 Eax1, Ebx1, Ecx1, Edx1;

  Print(L"Function 3 - Processor Serial Number\n");
  CpuidEx(0x00000001, 0, &Eax1, &Ebx1, &Ecx1, &Edx1);

  if (((Edx1 >> 18) & 1U) == 0) {
    Print(L"The processor serial number feature is not supported\n");
    Print(L"Processor serial number (PSN) is available in Pentium III processor only\n");
    return;
  }

  Print(L"(Supported) PSN feature bit set.\n");
}

STATIC
VOID
DecodeLeaf04 (
  VOID
  )
{
  UINT32 Sub;
  UINT32 Eax, Ebx, Ecx, Edx;
  UINT32 CacheType, CacheLevel;
  UINT32 Ways, Partitions, LineSize, Sets;
  UINT64 SizeBytes;
  CHAR16 *TypeStr;
  UINT32 CoresOnDie;
  BOOLEAN PrintedCores;

  Print(L"Function 4 - Deterministic Cache Parameters\n");

  PrintedCores = FALSE;
  CoresOnDie   = 0;

  Sub = 0;
  while (Sub <= 32) {
    CpuidEx(0x00000004, Sub, &Eax, &Ebx, &Ecx, &Edx);

    CacheType  = Eax & 0x1F;
    CacheLevel = (Eax >> 5) & 0x7;

    if (CacheType == 0) {
      break;
    }

    if (!PrintedCores && CacheLevel == 3) {
      CoresOnDie = ((Eax >> 26) & 0x3F) + 1;
      Print(L"Number of processor cores on this die: %d\n\n", CoresOnDie);
      PrintedCores = TRUE;
    }

    Print(L"Index %d\n", Sub);
    Print(L"EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

    TypeStr = L"Unknown";
    if (CacheType == 1) TypeStr = L"Data Cache";
    else if (CacheType == 2) TypeStr = L"Instruction Cache";
    else if (CacheType == 3) TypeStr = L"Unified Cache";

    Ways       = ((Ebx >> 22) & 0x3FF) + 1;
    Partitions = ((Ebx >> 12) & 0x3FF) + 1;
    LineSize   = ((Ebx >> 0)  & 0xFFF) + 1;
    Sets       = Ecx + 1;

    SizeBytes = (UINT64)Ways * (UINT64)Partitions * (UINT64)LineSize * (UINT64)Sets;

    Print(L"Cache Type: %s\n", TypeStr);
    Print(L"Cache Level: %d\n", CacheLevel);
    Print(L"Ways of associativity: %d\n", Ways);
    Print(L"Physical Line Partitions: %d\n", Partitions);
    Print(L"Line Size: %d\n", LineSize);
    Print(L"Number Of Sets: %d\n", Sets);
    Print(L"Cache Size: %d Bytes\n\n", SizeBytes);

    WaitAnyKey();
    ClearScreen();

    Sub++;
  }
}

STATIC
VOID
DecodeLeaf05 (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;
  UINT32 Smallest, Largest;
  BOOLEAN IntBreak;
  BOOLEAN MwaitExt;
  UINT32 C0, C1, C2, C3, C4;

  Print(L"Function 5 - MEDMONITOR/MWAIT Parameters\n");
  CpuidEx(0x00000005, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

  Smallest = Eax & 0xFFFF;
  Largest  = Ebx & 0xFFFF;

  IntBreak = (BOOLEAN)(((Ecx >> 0) & 1U) != 0);
  MwaitExt = (BOOLEAN)(((Ecx >> 1) & 1U) != 0);

  C0 = (Edx >> 0)  & 0xF;
  C1 = (Edx >> 4)  & 0xF;
  C2 = (Edx >> 8)  & 0xF;
  C3 = (Edx >> 12) & 0xF;
  C4 = (Edx >> 16) & 0xF;

  Print(L"Smallest monitor line size in bytes: %d\n", Smallest);
  Print(L"Largest  monitor line size in bytes: %d\n", Largest);
  Print(L"Support for treating interrupts as break-events for MWAIT: %s\n", IntBreak ? L"TRUE" : L"FALSE");
  Print(L"MONITOR / MWAIT Extensions supported: %s\n\n", MwaitExt ? L"TRUE" : L"FALSE");

  Print(L"Number of C4 sub-states supported: %d\n", C4);
  Print(L"Number of C3 sub-states supported: %d\n", C3);
  Print(L"Number of C2 sub-states supported: %d\n", C2);
  Print(L"Number of C1 sub-states supported: %d\n", C1);
  Print(L"Number of C0 sub-states supported: %d\n", C0);
}

STATIC
VOID
DecodeLeaf06 (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;
  BOOLEAN Dts;
  UINT32 Thresholds;
  BOOLEAN HwCoord;

  Print(L"Function 6 - Digital Thermal Sensor and Power Management Parameters\n");
  CpuidEx(0x00000006, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

  Dts        = (BOOLEAN)(((Eax >> 0) & 1U) != 0);
  Thresholds = (Eax >> 8) & 0xFF;
  HwCoord    = (BOOLEAN)(((Ecx >> 0) & 1U) != 0);

  Print(L"Digital Temperature Sensor (DTS) capability: %s\n", Dts ? L"TRUE" : L"FALSE");
  Print(L"Number of Interrupt Thresholds: %d\n", Thresholds);
  Print(L"Hardware Coordination Feedback Capability: %s\n", HwCoord ? L"TRUE" : L"FALSE");
}

STATIC
VOID
DecodeLeaf07 (
  VOID
  )
{
  UINT32 Eax0, Ebx0, Ecx0, Edx0;
  UINT32 Sub;
  UINT32 Eax, Ebx, Ecx, Edx;
  UINT32 MaxSub;

  BOOLEAN Invpcid;
  BOOLEAN Erms;
  BOOLEAN Smep;
  BOOLEAN Fsgsbase;

  Print(L"Function 7 - Structured Extended Feature Flags Enumeration\n");
  CpuidEx(0x00000007, 0, &Eax0, &Ebx0, &Ecx0, &Edx0);
  Print(L"EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n\n", Eax0, Ebx0, Ecx0, Edx0);

  MaxSub = Eax0;
  Print(L"Maximum Supported sub-leaf: %d\n", MaxSub);

  Invpcid  = (BOOLEAN)(((Ebx0 >> 10) & 1U) != 0);
  Erms     = (BOOLEAN)(((Ebx0 >> 9)  & 1U) != 0);
  Smep     = (BOOLEAN)(((Ebx0 >> 7)  & 1U) != 0);
  Fsgsbase = (BOOLEAN)(((Ebx0 >> 0)  & 1U) != 0);

  Print(L"INVPCID instruction support: %s\n", Invpcid ? L"TRUE" : L"FALSE");
  Print(L"RDS MOVSB/STOSB support: %s\n", Erms ? L"TRUE" : L"FALSE");
  Print(L"Supervisor Mode Execution Protection (SMEP) support: %s\n", Smep ? L"TRUE" : L"FALSE");
  Print(L"FSGSBASE support: %s\n\n", Fsgsbase ? L"TRUE" : L"FALSE");

  if (MaxSub > 2) {
    MaxSub = 2;
  }

  for (Sub = 1; Sub <= MaxSub; Sub++) {
    CpuidEx(0x00000007, Sub, &Eax, &Ebx, &Ecx, &Edx);
    Print(L"Index: %d\n", Sub);
    Print(L"EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);
  }
}

STATIC
VOID
DecodeLeaf08 (
  VOID
  )
{
  Print(L"Function 8 - Reserved\n");
  Print(L"This function is reserved\n");
}

STATIC
VOID
DecodeLeaf09 (
  VOID
  )
{
  UINT32 Eax1, Ebx1, Ecx1, Edx1;
  BOOLEAN Dca;

  CpuidEx(0x00000001, 0, &Eax1, &Ebx1, &Ecx1, &Edx1);
  Dca = (BOOLEAN)(((Ecx1 >> 18) & 1U) != 0);

  Print(L"Function 9 - Direct Cache Access (DCA) Parameters\n");
  if (!Dca) {
    Print(L"The Direct Cache Access (DCA) feature is not supported\n");
    return;
  }

  /* If supported, show raw regs (no screenshot provided for supported case) */
  {
    UINT32 Eax, Ebx, Ecx, Edx;
    CpuidEx(0x00000009, 0, &Eax, &Ebx, &Ecx, &Edx);
    Print(L"EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n", Eax, Ebx, Ecx, Edx);
  }
}

/* ------------------------- Decode 0A..0D ------------------------- */

STATIC
VOID
DecodeLeaf0A (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;
  UINT32 VersionId;
  UINT32 NumGp;
  UINT32 BitWidth;
  UINT32 EbxLen;
  UINT32 NumFixed;
  UINT32 FixedWidth;

  BOOLEAN Bm;
  BOOLEAN Bi;
  BOOLEAN LlcMiss;
  BOOLEAN LlcRef;
  BOOLEAN RefCycles;
  BOOLEAN InstrRet;
  BOOLEAN CoreCycles;

  Print(L"Function 0A - Architectural Performance Monitor Features\n");

  CpuidEx(0x0000000A, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

  VersionId = (Eax >> 0)  & 0xFF;
  NumGp     = (Eax >> 8)  & 0xFF;
  BitWidth  = (Eax >> 16) & 0xFF;
  EbxLen    = (Eax >> 24) & 0xFF;

  NumFixed   = (Edx >> 0) & 0x1F;
  FixedWidth = (Edx >> 5) & 0xFF;

  Print(L"Length of EBX bit vector to enumerate architectural performance monitoring event: %d\n", EbxLen);
  Print(L"Bit width of general-purpose performance monitoring counters: %d\n", BitWidth);
  Print(L"Number of general-purpose performance monitoring counters per logical processor: %d\n", NumGp);
  Print(L"Version ID: %d\n\n", VersionId);

  Bm      = (BOOLEAN)(((Ecx >> 0) & 1U) != 0);
  Bi      = (BOOLEAN)(((Ecx >> 1) & 1U) != 0);
  LlcMiss = (BOOLEAN)(((Ecx >> 2) & 1U) != 0);
  LlcRef  = (BOOLEAN)(((Ecx >> 3) & 1U) != 0);

  RefCycles = (BOOLEAN)(((Ecx >> 4) & 1U) != 0);
  InstrRet  = (BOOLEAN)(((Ecx >> 5) & 1U) != 0);
  CoreCycles= (BOOLEAN)(((Ecx >> 6) & 1U) != 0);

  Print(L"Branch Mispredicts Retired: %s\n", Bm ? L"Supported" : L"Not Supported");
  Print(L"Branch Instructions Retired: %s\n", Bi ? L"Supported" : L"Not Supported");
  Print(L"Last Level Cache Misses: %s\n", LlcMiss ? L"Supported" : L"Not Supported");
  Print(L"Last Level Cache References: %s\n", LlcRef ? L"Supported" : L"Not Supported");
  Print(L"Reference Cycles: %s\n", RefCycles ? L"Supported" : L"Not Supported");
  Print(L"Instructions Retired: %s\n", InstrRet ? L"Supported" : L"Not Supported");
  Print(L"Core Cycles: %s\n\n", CoreCycles ? L"Supported" : L"Not Supported");

  Print(L"Number of Fixed Counters: %d\n", NumFixed);
  Print(L"Number of Bits in the Fixed Counters (width): %d\n", FixedWidth);
}

STATIC
VOID
DecodeLeaf0B (
  VOID
  )
{
  UINT32 Sub;
  UINT32 Eax, Ebx, Ecx, Edx;
  UINT32 Shift;
  UINT32 NumLog;
  UINT32 LevelType;
  UINT32 LevelNum;

  Print(L"Function 0B - x2APIC Features / Processor Topology\n");

  Sub = 0;
  while (Sub <= 8) {
    CpuidEx(0x0000000B, Sub, &Eax, &Ebx, &Ecx, &Edx);

    Print(L"Index: %d\n", Sub);
    Print(L"EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

    if ((Ebx & 0xFFFF) == 0) {
      break;
    }

    Shift     = Eax & 0x1F;
    NumLog    = Ebx & 0xFFFF;
    LevelType = (Ecx >> 8) & 0xFF;
    LevelNum  = Ecx & 0xFF;

    Print(L"Number of bits to shift right APIC ID to get next level APIC ID: %d\n", Shift);
    Print(L"Number of factory-configured logical processors at this level: %d\n", NumLog);

    if (LevelType == 1) {
      Print(L"Level Type           : Thread\n");
    } else if (LevelType == 2) {
      Print(L"Level Type           : Core\n");
    } else {
      Print(L"Level Type           : %d\n", LevelType);
    }

    Print(L"Level Number         : %d\n", LevelNum);
    Print(L"Extended APIC ID     : 0x%08X\n\n", Edx);

    Sub++;
  }
}

STATIC
VOID
DecodeLeaf0C (
  VOID
  )
{
  Print(L"Function 0C - Reserved\n");
  Print(L"This function is reserved\n");
}

STATIC
VOID
DecodeLeaf0D (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;
  BOOLEAN XsaveOpt;

  Print(L"Function 0D - XSAVE Features\n");

  Print(L"Processor Extended State Enumeration (CPUID Function 0Dh with ECX=0)\n");
  CpuidEx(0x0000000D, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

  Print(L"Reports the valid bit fields of the lower 32 bits of the XCR0 : 0x%08X\n", Eax);
  Print(L"Maximum size required by enabled features in XCR0 : %d Bytes\n", Ebx);
  Print(L"Maximum size of the XSAVE/XRSTOR save area required by all supported features in\n");
  Print(L"  the processor : %d Bytes\n", Ecx);

  if (Edx == 0) {
    Print(L"Reports the valid bit fields of the upper 32 bits of the XCR0 : Reserve\n\n");
  } else {
    Print(L"Reports the valid bit fields of the upper 32 bits of the XCR0 : 0x%08X\n\n", Edx);
  }

  Print(L"Processor Extended State Enumeration (CPUID Function 0Dh with ECX=1)\n");
  CpuidEx(0x0000000D, 1, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

  XsaveOpt = (BOOLEAN)(((Eax >> 0) & 1U) != 0);
  Print(L"The XSAVEOPT instruction: %s\n\n", XsaveOpt ? L"Supported" : L"Not Supported");

  Print(L"Processor Extended State Enumeration (CPUID Function 0Dh with ECX=2)\n");
  CpuidEx(0x0000000D, 2, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

  Print(L"The size in bytes of the save area for an extended state feature : 0x%08X\n", Eax);
  Print(L"The offset in bytes of the save area from the beginning of the XSAVE/XRSTOR area\n");
  Print(L"  : 0x%08X\n", Ebx);
}

/* ------------------------- Decode 80000000..80000008 (aligned to screenshots) ------------------------- */

STATIC
VOID
DecodeLeaf80000000 (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;

  Print(L"Function 80000000 - Largest Extended Function\n");
  CpuidEx(0x80000000, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX: %08X EBX: %08X ECX: %08X EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);
  Print(L"Largest extended function number supported: %08X\n", Eax);
}

STATIC
VOID
DecodeLeaf80000001 (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;

  BOOLEAN LahfSahf;
  BOOLEAN Lm64;
  BOOLEAN Rdtscp;
  BOOLEAN OneGb;
  BOOLEAN Nx;
  BOOLEAN Syscall;

  Print(L"Function 80000001 - Extended Feature Bits\n");
  CpuidEx(0x80000001, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

  LahfSahf = (BOOLEAN)(((Ecx >> 0)  & 1U) != 0);
  Lm64     = (BOOLEAN)(((Edx >> 29) & 1U) != 0);
  Rdtscp   = (BOOLEAN)(((Edx >> 27) & 1U) != 0);
  OneGb    = (BOOLEAN)(((Edx >> 26) & 1U) != 0);
  Nx       = (BOOLEAN)(((Edx >> 20) & 1U) != 0);
  Syscall  = (BOOLEAN)(((Edx >> 11) & 1U) != 0);

  Print(L"LAHF / SAHF: %s\n", LahfSahf ? L"Support" : L"Not Support");
  Print(L"Intel 64 Instruction Set Architecture: %s\n", Lm64 ? L"Support" : L"Not Support");
  Print(L"RDTSCP and IA32_TSC_AUX: %s\n", Rdtscp ? L"Support" : L"Not Support");
  Print(L"1 GB Pages: %s\n", OneGb ? L"Support" : L"Not Support");
  Print(L"Execution Disable Bit: %s\n", Nx ? L"Support" : L"Not Support");
  Print(L"SYSCALL / SYSRET: %s\n", Syscall ? L"Support" : L"Not Support");
}

STATIC
VOID
DecodeLeaf80000002 (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;

  Print(L"Function 80000002 - Processor Brand String\n");
  CpuidEx(0x80000002, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX: %08X EBX: %08X ECX: %08X EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

  PrintBrandStringLeaf16(0x80000002);
}

STATIC
VOID
DecodeLeaf80000003 (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;

  Print(L"Function 80000003 - Processor Brand String\n");
  CpuidEx(0x80000003, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX: %08X EBX: %08X ECX: %08X EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

  PrintBrandStringLeaf16(0x80000003);
}

STATIC
VOID
DecodeLeaf80000004 (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;

  Print(L"Function 80000004 - Processor Brand String\n");
  CpuidEx(0x80000004, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX: %08X EBX: %08X ECX: %08X EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

  PrintBrandStringLeaf16(0x80000004);
}

STATIC
VOID
DecodeLeaf80000005 (
  VOID
  )
{
  Print(L"Function 80000005 - Reserved\n");
  Print(L"This function is reserved\n");
}

STATIC
VOID
DecodeLeaf80000006 (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;
  UINT32 L2SizeKb;
  UINT32 L2Assoc;
  UINT32 L2Line;

  Print(L"Function 80000006 - Extended L2 Cache Features\n");
  CpuidEx(0x80000006, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

  L2Line   = (Ecx & 0xFF);
  L2SizeKb = (Ecx >> 16) & 0xFFFF;
  L2Assoc  = (Ecx >> 12) & 0xF;

  Print(L"L2 Cache Size: %d KB\n", L2SizeKb);
  Print(L"L2 Cache Associativity: 0x%02X\n", L2Assoc);
  Print(L"L2 Cache Line Size: %d Bytes\n", L2Line);
}

STATIC
VOID
DecodeLeaf80000007 (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;
  BOOLEAN InvariantTsc;

  Print(L"Function 80000007 - Advanced Power Management\n");
  CpuidEx(0x80000007, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX: %08X  EBX: %08X  ECX: %08X  EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

  InvariantTsc = (BOOLEAN)(((Edx >> 8) & 1U) != 0);

  Print(L"TSC will run at a constant rate in all ACPI P-states, C-states, T-states: %s\n",
        InvariantTsc ? L"Available" : L"Not Available");
}

STATIC
VOID
DecodeLeaf80000008 (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;
  UINT32 PhysBits;
  UINT32 VirtBits;

  Print(L"Function 80000008 - Virtual and Physical Address Sizes\n");
  CpuidEx(0x80000008, 0, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"EAX: %08X EBX: %08X ECX: %08X EDX: %08X\n\n", Eax, Ebx, Ecx, Edx);

  PhysBits = (Eax >> 0) & 0xFF;
  VirtBits = (Eax >> 8) & 0xFF;

  Print(L"Number of address bits supported by the processor for a virtual address: %d bits\n\n", VirtBits);
  Print(L"Number of address bits supported by the processor for a physical address: %d bit\n", PhysBits);
  Print(L"s\n"); /* mimic screenshot line wrap: "39 bit" + next line "s" */
}

/* ------------------------- CPUID menu loop ------------------------- */

STATIC
VOID
RunCpuidMenu (
  VOID
  )
{
  EFI_INPUT_KEY Key;
  CHAR8  Vendor[13];
  UINT32 MaxStd, MaxExt;

  CHAR16 Ch;
  INTN   Idx;
  UINT32 Leaf;

  GetVendorMax(Vendor, &MaxStd, &MaxExt);

  while (TRUE) {
    ClearScreen();
    Print(L"Vendor: %a  MaxStd=%08X  MaxExt=%08X\n", Vendor, MaxStd, MaxExt);
    ShowCpuidMenu();

    while (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_NOT_READY) {
      gBS->Stall(1000);
    }

    Ch = Key.UnicodeChar;
    if (Ch == 0) continue;

    if (Ch == 0x1B || Ch == L'Q' || Ch == L'q') {
      return;
    }

    Idx = FindMenuIndexByHotkey(Ch);
    if (Idx < 0) {
      Print(L"\nInvalid selection.\n");
      WaitAnyKey();
      continue;
    }

    Leaf = mCpuidMenu[Idx].Leaf;

    if (Leaf < 0x80000000) {
      if (Leaf > MaxStd) {
        Print(L"\nLeaf %08X not supported on this CPU.\n", Leaf);
        WaitAnyKey();
        continue;
      }
    } else {
      if (Leaf > MaxExt) {
        Print(L"\nLeaf %08X not supported on this CPU.\n", Leaf);
        WaitAnyKey();
        continue;
      }
    }

    ShowCpuidFunctionPage(Leaf);
  }
}

/* ------------------------- MSR read/write ------------------------- */

STATIC
BOOLEAN
IsMsrSupported (
  VOID
  )
{
  UINT32 Eax, Ebx, Ecx, Edx;
  CpuidEx(1, 0, &Eax, &Ebx, &Ecx, &Edx);
  return (BOOLEAN)(((Edx >> 5) & 1U) != 0);
}

STATIC
EFI_STATUS
ReadLine (
  OUT CHAR16  *Buffer,
  IN  UINTN   BufferChars
  )
{
  UINTN         Len;
  EFI_INPUT_KEY Key;
  EFI_STATUS    Status;

  if (Buffer == NULL || BufferChars < 2) return EFI_INVALID_PARAMETER;

  Len = 0;
  ZeroMem(Buffer, BufferChars * sizeof(CHAR16));

  while (TRUE) {
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    if (Status == EFI_NOT_READY) {
      gBS->Stall(1000);
      continue;
    }
    if (EFI_ERROR(Status)) return Status;

    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
      Print(L"\n");
      Buffer[Len] = L'\0';
      return EFI_SUCCESS;
    }

    if (Key.UnicodeChar == CHAR_BACKSPACE) {
      if (Len > 0) {
        Len--;
        Buffer[Len] = L'\0';
        Print(L"\b \b");
      }
      continue;
    }

    if (Key.UnicodeChar >= 0x20 && Key.UnicodeChar <= 0x7E) {
      if (Len + 1 < BufferChars) {
        Buffer[Len] = Key.UnicodeChar;
        Len++;
        Buffer[Len] = L'\0';
        Print(L"%c", Key.UnicodeChar);
      }
    }
  }
}

STATIC
VOID
DoMsrRead (
  VOID
  )
{
  CHAR16 Buf[INPUT_BUF_CHARS];
  UINT32 Msr;
  UINT64 Val;

  ClearScreen();
  Print(L"MSR Read\n\n");

  if (!IsMsrSupported()) {
    Print(L"MSR not supported (CPUID.01h:EDX[5]=0)\n");
    WaitAnyKey();
    return;
  }

  Print(L"WARNING: RDMSR may hang/reset if MSR is invalid on this CPU.\n");
  Print(L"Type YES to continue: ");
  if (EFI_ERROR(ReadLine(Buf, INPUT_BUF_CHARS)) || StrCmp(Buf, L"YES") != 0) {
    Print(L"Cancelled.\n");
    WaitAnyKey();
    return;
  }

  Print(L"\nEnter MSR (hex), e.g. 0x10: ");
  if (EFI_ERROR(ReadLine(Buf, INPUT_BUF_CHARS)) || !ParseHex32Simple(Buf, &Msr)) {
    Print(L"Invalid MSR.\n");
    WaitAnyKey();
    return;
  }

  Val = AsmReadMsr64(Msr);
  Print(L"\nRDMSR[0x%08X] = 0x%016lx\n", Msr, Val);
  WaitAnyKey();
}

STATIC
VOID
DoMsrWrite (
  VOID
  )
{
  CHAR16 Buf[INPUT_BUF_CHARS];
  UINT32 Msr;
  UINT64 Val;

  ClearScreen();
  Print(L"MSR Write\n\n");

  if (!IsMsrSupported()) {
    Print(L"MSR not supported (CPUID.01h:EDX[5]=0)\n");
    WaitAnyKey();
    return;
  }

  Print(L"WARNING: WRMSR may hang/reset if MSR is locked/invalid.\n");
  Print(L"Type YES to continue: ");
  if (EFI_ERROR(ReadLine(Buf, INPUT_BUF_CHARS)) || StrCmp(Buf, L"YES") != 0) {
    Print(L"Cancelled.\n");
    WaitAnyKey();
    return;
  }

  Print(L"\nEnter MSR (hex), e.g. 0x10: ");
  if (EFI_ERROR(ReadLine(Buf, INPUT_BUF_CHARS)) || !ParseHex32Simple(Buf, &Msr)) {
    Print(L"Invalid MSR.\n");
    WaitAnyKey();
    return;
  }

  Print(L"Enter Value (hex 64-bit), e.g. 0x1234: ");
  if (EFI_ERROR(ReadLine(Buf, INPUT_BUF_CHARS)) || !ParseHex64Simple(Buf, &Val)) {
    Print(L"Invalid value.\n");
    WaitAnyKey();
    return;
  }

  AsmWriteMsr64(Msr, Val);
  Print(L"\nWRMSR[0x%08X] <= 0x%016lx (done)\n", Msr, Val);
  WaitAnyKey();
}

/* ------------------------- Main menu ------------------------- */

STATIC
VOID
ShowMainMenu (
  VOID
  )
{
  Print(L"\n==== %s ====\n", APP_NAME);
  Print(L"1) CPUID Functions\n");
  Print(L"2) MSR Read\n");
  Print(L"3) MSR Write\n");
  Print(L"0) Exit\n");
  Print(L"> ");
}

INTN
EFIAPI
ShellAppMain (
  IN UINTN Argc,
  IN CHAR16 **Argv
  )
{
  CHAR16 Buf[INPUT_BUF_CHARS];

  (VOID)Argc;
  (VOID)Argv;

  while (TRUE) {
    ClearScreen();
    ShowMainMenu();

    if (EFI_ERROR(ReadLine(Buf, INPUT_BUF_CHARS))) break;

    if (StrCmp(Buf, L"1") == 0) {
      RunCpuidMenu();
    } else if (StrCmp(Buf, L"2") == 0) {
      DoMsrRead();
    } else if (StrCmp(Buf, L"3") == 0) {
      DoMsrWrite();
    } else if (StrCmp(Buf, L"0") == 0) {
      break;
    } else {
      Print(L"Unknown selection.\n");
      WaitAnyKey();
    }
  }

  return 0;
}