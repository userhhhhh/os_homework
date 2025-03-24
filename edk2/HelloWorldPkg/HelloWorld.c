#include <Uefi.h>
// #include "/home/hqs123/os_homework/edk2/MdePkg/Include/Uefi.h"
#include <Library/UefiLib.h>

EFI_STATUS 
EFIAPI
UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable){
    Print(L"Hello Fuck\n");
    return 0;
}