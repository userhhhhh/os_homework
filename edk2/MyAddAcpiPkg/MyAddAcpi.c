#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <IndustryStandard/Acpi.h>
#include <Protocol/AcpiTable.h>  // 添加ACPI表协议头文件

// 定义自定义 ACPI 表的签名
#define MY_CUSTOM_ACPI_TABLE_SIGNATURE  SIGNATURE_32('M','Y','T','B')

// 自定义 ACPI 表结构
#pragma pack(1)
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER Header;  // 标准 ACPI 表头
  UINT32                      Data1;   // 自定义数据字段
  UINT64                      Data2;   // 自定义数据字段
  CHAR8                       Message[32]; // 自定义消息字段
} MY_CUSTOM_ACPI_TABLE;
#pragma pack()

/**
  计算 ACPI 表校验和
  
  @param[in] Buffer   指向需要计算校验和的表的指针
  @param[in] Size     表的大小
  
  @retval    用于填充表头的校验和值
**/
UINT8
CalculateChecksum (
  IN UINT8  *Buffer,
  IN UINTN  Size
  )
{
  UINT8 Sum;
  UINTN Index;

  Sum = 0;
  for (Index = 0; Index < Size; Index++) {
    Sum = (UINT8) (Sum + Buffer[Index]);
  }

  return (UINT8) (0x100 - Sum);
}

/**
  创建并初始化自定义 ACPI 表
  
  @param[out] Table   指向创建的表的指针的指针
  
  @retval EFI_SUCCESS            表创建成功
  @retval EFI_OUT_OF_RESOURCES   内存分配失败
**/
EFI_STATUS
CreateCustomAcpiTable (
  OUT MY_CUSTOM_ACPI_TABLE **Table
  )
{
  MY_CUSTOM_ACPI_TABLE *MyTable;
  // 删除未使用的Status变量
  
  // 为自定义表分配内存
  MyTable = AllocateZeroPool (sizeof (MY_CUSTOM_ACPI_TABLE));
  if (MyTable == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  
  // 填充表头
  MyTable->Header.Signature = MY_CUSTOM_ACPI_TABLE_SIGNATURE;
  MyTable->Header.Length = sizeof (MY_CUSTOM_ACPI_TABLE);
  MyTable->Header.Revision = 1;
  MyTable->Header.OemRevision = 1;
  CopyMem (MyTable->Header.OemId, "MYOEMD", 6);
  // 修正OemTableId的类型使用
  AsciiStrCpyS ((CHAR8 *)(UINTN)&MyTable->Header.OemTableId, 9, "MYCUSTOM"); // 修改为字符串拷贝
  MyTable->Header.CreatorId = 0x12345678;
  MyTable->Header.CreatorRevision = 0x00000001;
  
  // 填充自定义数据
  MyTable->Data1 = 0xDEADBEEF;
  MyTable->Data2 = 0x0123456789ABCDEF;
  CopyMem (MyTable->Message, "Hello from custom ACPI table!", 28);
  
  // 计算校验和
  MyTable->Header.Checksum = 0;
  MyTable->Header.Checksum = CalculateChecksum (
                              (UINT8 *) MyTable,
                              MyTable->Header.Length
                              );
  
  *Table = MyTable;
  return EFI_SUCCESS;
}

/**
  安装自定义 ACPI 表到 UEFI 系统表中
  
  @param[in] Table   指向要安装的表的指针
  @param[out] TableKey 返回的表键值
  
  @retval EFI_SUCCESS     表安装成功
  @retval 其他           表安装失败
**/
EFI_STATUS
InstallCustomAcpiTable (
  IN MY_CUSTOM_ACPI_TABLE *Table,
  OUT UINTN *TableKey
  )
{
  EFI_STATUS Status;
  EFI_ACPI_TABLE_PROTOCOL *AcpiTable = NULL;  // 修改为局部变量
  
  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **) &AcpiTable
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to locate ACPI table protocol: %r\n", Status));
    return Status;
  }
  
  // 安装表
  Status = AcpiTable->InstallAcpiTable (
                        AcpiTable,
                        Table,
                        Table->Header.Length,
                        TableKey
                        );
                        
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to install ACPI table: %r\n", Status));
    return Status;
  }
  
  DEBUG ((DEBUG_INFO, "Custom ACPI table installed successfully with key: %d\n", *TableKey));
  return EFI_SUCCESS;
}

/**
  Application entry point.

  @param[in] ImageHandle  The image handle of the UEFI Application.
  @param[in] SystemTable  A pointer to the UEFI System Table.

  @retval EFI_SUCCESS     The application executed successfully.
  @retval other           An error occurred.
**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;
  MY_CUSTOM_ACPI_TABLE *MyCustomTable;
  UINTN TableKey = 0;
  
  Print (L"Starting MyAddAcpi application...\n");
  
  // 创建自定义表
  Status = CreateCustomAcpiTable (&MyCustomTable);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to create custom ACPI table: %r\n", Status);
    return Status;
  }
  
  Print (L"Custom ACPI table created successfully\n");
  Print (L"Signature: 0x%08x\n", MyCustomTable->Header.Signature);
  Print (L"Length: %u bytes\n", MyCustomTable->Header.Length);
  
  // 安装表，使用修正后的函数接口
  Status = InstallCustomAcpiTable (MyCustomTable, &TableKey);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to install ACPI table: %r\n", Status);
    FreePool (MyCustomTable);
    return Status;
  }
  
  Print (L"Custom ACPI table installed successfully with key: %u\n", TableKey);
  Print (L"You can now retrieve this table from UEFI using the MYTB signature\n");
  
  // 释放表内存
  FreePool (MyCustomTable);
  
  return EFI_SUCCESS;
}
