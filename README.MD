# cheat-driver

Simple WDM kernel mode driver for handling read/write memory requests into
arbitrary processes.

### Background

Kernel based anti-cheat drivers (EAC, BattleEye) block or monitor requests for
interfacing with the memory from the game process. The simplest way to bypass anti-cheat protections from the kernel is to use your own kernel mode driver.

### Building

1. Install [Visual Studio](https://www.visualstudio.com/).
2. Install the [Windows Driver Kit](https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk).
3. Set the solution configuration to `x64` and `Release` and build the solution.

### Usage

For practical use you will need a driver signing certificate. For development
purposes you can enable [test-signing mode](https://docs.microsoft.com/en-us/windows-hardware/drivers/install/the-testsigning-boot-configuration-option).

You will need to either install and run the driver as a service with `CreateService` or use a function like `NtLoadDriver`.
Once the driver is loaded, you can open a handle to the driver:

```cpp
#include "driver_config.h"
#include "driver_codes.h"

HANDLE driver = CreateFileW(
    DRIVER_DEVICE_PATH, 
    GENERIC_READ | GENERIC_WRITE, 
    FILE_SHARE_READ | FILE_SHARE_WRITE, 
    0, 
    OPEN_EXISTING, 
    0, 0);
```

To issue read or write requests, you send the driver a control code:

```cpp
DRIVER_COPY_MEMORY copy = {};
copy.ProcessId = processId;
copy.Source = sourceBufferPtr;
copy.Target = targetAddressPtr;
copy.Size = bytesToRead;
copy.Write = FALSE;

DeviceIoControl(
    driver, 
    IOCTL_DRIVER_COPY_MEMORY,
     &copy, 
     sizeof(copy), 
     &copy, 
     sizeof(copy),
     0, 0)
```

The target and source parameters should be reversed if issuing a write request.