[Defines]
    PLATFORM_NAME = MyAcpiViewPkg
    PLATFORM_GUID = 76ef5ba8-8dcf-48cb-8b97-6373e1d30c60
    PLATFORM_VERSION = 0.1
    DSC_SPECIFICATION = 0x00010005
    SUPPORTED_ARCHITECTURES = X64
    BUILD_TARGETS = DEBUG|RELEASE
    OUTPUT_DIRECTORY = Build/MyAcpiViewPkg

[LibraryClasses]
    UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
    UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
    PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
    PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
    MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
    DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
    BaseMemoryLib|MdePkg/Library/BaseMemoryLibMmx/BaseMemoryLibMmx.inf
    BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
    UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
    DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
    UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
    UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
    UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf
    RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
    StackCheckLib|MdePkg/Library/StackCheckLib/StackCheckLib.inf
    StackCheckFailureHookLib|MdePkg/Library/StackCheckFailureHookLibNull/StackCheckFailureHookLibNull.inf
    GuidLib|MdePkg/Library/BaseExtractGuidedSectionLib/BaseExtractGuidedSectionLib.inf


[Components]
    MyAcpiViewPkg/MyAcpiView.inf