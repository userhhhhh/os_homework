##  
# MyAddAcpiPkg DSC file for creating a UEFI application that installs a custom ACPI table
##

[Defines]
  PLATFORM_NAME                  = MyAddAcpiPkg
  PLATFORM_GUID                  = d6cf375a-7885-438f-8cca-0bde7304e434
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION             = 0x00010005
  OUTPUT_DIRECTORY              = Build/MyAddAcpiPkg
  SUPPORTED_ARCHITECTURES        = IA32|X64|ARM|AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT

!include MdePkg/MdeLibs.dsc.inc

[LibraryClasses]
  # 基础库
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  
  # UEFI 应用程序支持库
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf

  # 其他必要的库
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  PciLib|MdePkg/Library/BasePciLibCf8/BasePciLibCf8.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  ReportStatusCodeLib|MdeModulePkg/Library/DxeReportStatusCodeLib/DxeReportStatusCodeLib.inf
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf

[LibraryClasses.common.UEFI_APPLICATION]
  # 调试配置，DEBUG 版本使用标准调试库
  !if $(TARGET) == DEBUG
    DebugLib|MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
  !endif

[Components]
  # 包含我们的 ACPI 表安装应用程序
  MyAddAcpiPkg/MyAddAcpi.inf

[BuildOptions]
  # 通用编译器标志
  GCC:*_*_*_CC_FLAGS = -DMDEPKG_NDEBUG
  MSFT:*_*_*_CC_FLAGS = /D MDEPKG_NDEBUG
  
  # 对于 DEBUG 构建，启用调试信息
  !if $(TARGET) == DEBUG
    GCC:*_*_*_CC_FLAGS = -O0 -g
    MSFT:*_*_*_CC_FLAGS = /Od /Zi
  !endif
