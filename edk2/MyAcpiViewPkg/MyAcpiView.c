#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/Acpi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/DebugLib.h>

VOID _PrintTable_Root(EFI_ACPI_6_3_ROOT_SYSTEM_DESCRIPTION_POINTER  *Root);
VOID _PrintTable_Header(EFI_ACPI_DESCRIPTION_HEADER *Header);
VOID ChangeACPITable(IN UINTN TableIndex, IN EFI_ACPI_DESCRIPTION_HEADER *NewTable);

EFI_STATUS
EFIAPI
UefiMain(
    EFI_HANDLE ImageHandle, 
    EFI_SYSTEM_TABLE *SystemTable
){
    UINTN i, j = 0, EntryCount;
    UINT64 *EntryPtr;
    EFI_CONFIGURATION_TABLE *configTab = NULL;
    EFI_ACPI_6_3_ROOT_SYSTEM_DESCRIPTION_POINTER  *Root;
    EFI_ACPI_DESCRIPTION_HEADER   *XSDT, *Entry, *FACS, *DSDT;
    EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE  *FADT;

    configTab = gST -> ConfigurationTable;
    Print(L"Hello UEFI\n");

    for(i = 0; i < gST -> NumberOfTableEntries; i++){
        if(CompareGuid(&gEfiAcpiTableGuid, &configTab[i].VendorGuid)||
            CompareGuid(&gEfiAcpi20TableGuid, &configTab[i].VendorGuid)){
            Print(L"ACPI Table found\n");
            Root = configTab[i].VendorTable;
            XSDT = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN) Root->XsdtAddress;

            _PrintTable_Root(Root);
            _PrintTable_Header(XSDT);

            EntryCount = (XSDT->Length - sizeof(EFI_ACPI_DESCRIPTION_HEADER)) / sizeof(UINT64);
            EntryPtr = (UINT64 *)(XSDT + 1);
            Entry = (EFI_ACPI_DESCRIPTION_HEADER *)((UINTN)(*EntryPtr));
            FADT = (EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE *)(UINTN) Entry;
            FACS = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)(FADT->FirmwareCtrl);
            DSDT = (EFI_ACPI_DESCRIPTION_HEADER *)(UINTN)(FADT->XDsdt);
            for (j = 0; j < EntryCount; j++, EntryPtr++) {
                Entry = (EFI_ACPI_DESCRIPTION_HEADER *)((UINTN)(*EntryPtr));
                if (Entry == NULL) {
                    continue;
                }
                _PrintTable_Header(Entry);
                if(j == 0){
                    _PrintTable_Header(FACS);
                    _PrintTable_Header(DSDT);
                }
            }
            break;
        }
    }
    return 0;
}

VOID _PrintTable_Root(
    EFI_ACPI_6_3_ROOT_SYSTEM_DESCRIPTION_POINTER  *Root
){
    UINTN j = 0;
    Print(L"------------RSDP Table------------\n");
    Print(L"Address: 0x%x\n", Root);
    Print(L"Length: %d\n", Root->Length);
    Print(L"OEM_ID:");
    for (j = 0; j < 6; j++) {
        Print(L"%c", Root->OemId[j]); 
    }
    Print(L"\n");
    Print(L"CheckSum: 0x%x\n", Root->Checksum);
}

VOID _PrintTable_Header(
    EFI_ACPI_DESCRIPTION_HEADER *Header
){
    UINTN j = 0;
    Print(L"------------");
    UINT8 Temp = 0;
    for(j = 0; j < 4; j++) {
        Temp = (Header->Signature >> (j * 8)) & 0xff;
        Print(L"%c", Temp);
    }
    Print(L" Table------------\n");
    Print(L"Address: 0x%x\n", Header);
    Print(L"Length: %d\n", Header->Length);
    Print(L"OEM_ID:");
    for (j = 0; j < 6; j++) {
        Print(L"%c", Header->OemId[j]); 
    }
    Print(L"\n");
    Print(L"Checksum: 0x%x\n", Header->Checksum);
}

VOID ChangeACPITable(
    IN UINTN TableIndex,
    IN EFI_ACPI_DESCRIPTION_HEADER *NewTable
){

    EFI_ACPI_DESCRIPTION_HEADER *CurrentTable = NULL;
    EFI_CONFIGURATION_TABLE *configTab = gST -> ConfigurationTable;

    for(UINTN i = 0; i < gST -> NumberOfTableEntries; i++){
        if(CompareGuid(&gEfiAcpiTableGuid, &configTab[i].VendorGuid)||
            CompareGuid(&gEfiAcpi20TableGuid, &configTab[i].VendorGuid)){
            if(TableIndex == 0){
                CurrentTable = configTab[i].VendorTable;
                break;
            }
            TableIndex--;
        }
    }

    if(CurrentTable == NULL){
        return;
    }

    if(NewTable != NULL){
        CopyMem(CurrentTable, NewTable, NewTable->Length);
        Print(L"Table %d has been modified!\n", TableIndex);
    }

    Print(L"Modified Table Address: 0x%lx\n", (UINT64)CurrentTable);
    Print(L"Modified Table Length: %d\n", CurrentTable->Length);

    UINT8 Checksum = 0;
    UINT8 *TableBytes = (UINT8 *)CurrentTable;
    for (UINTN i = 0; i < CurrentTable->Length; i++){
        Checksum += TableBytes[i];
    }

    if (Checksum == 0){
        Print(L"Checksum is valid.\n");
    } else {
        Print(L"Checksum is invalid. Value: %d\n", Checksum);
    }
}
