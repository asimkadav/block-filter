Block Filter Driver
===================

A simple block I/O filter driver for modern Linux kernels (5.0+). This driver creates a virtual block device that intercepts and forwards I/O requests to an underlying block device, allowing monitoring, logging, and potential modification of block I/O operations.

**Updated for Linux Kernel 5.0+** - This version has been modernized to use the blk-mq (multi-queue block layer) API instead of the deprecated request_fn interface.

## Features

- **Modern Kernel Support**: Compatible with Linux kernels 5.0 and above
- **Bio-based Filtering**: Intercepts block I/O at the bio submission level
- **Configurable Target Device**: Use module parameters to specify which device to filter
- **Statistics Tracking**: Counts processed I/O operations
- **Proper Resource Management**: Safe device reference counting and error handling
- **Control Interface**: Misc device for querying statistics via ioctl

## Requirements

- Linux kernel 5.0 or newer
- Kernel headers for your running kernel
- GCC and make
- Root privileges for loading/unloading the module

## How It Works

The driver operates by:

1. **Creating a Virtual Block Device**: Allocates a new block device (`/dev/blockfilter`) using the modern `blk_alloc_disk()` API
2. **Opening the Target Device**: Safely opens the target block device (e.g., `/dev/sda1`) with proper reference counting using `blkdev_get_by_path()`
3. **Intercepting I/O**: Implements a `submit_bio` callback that intercepts all bio submissions to the filter device
4. **Forwarding Requests**: Clones each bio and forwards it to the underlying target device using `submit_bio_noacct()`
5. **Logging & Statistics**: Logs I/O operations and maintains counters for monitoring

This approach allows transparent interception of block I/O without modifying the target device or filesystem.

## Building

```bash
cd misc
make
```

This will compile the `misc.ko` kernel module.

## Installation

### Loading the Module

Basic usage with default settings (targets `/dev/sda1`):
```bash
sudo insmod misc.ko
```

Specify a custom target device:
```bash
sudo insmod misc.ko target_device=/dev/sdb1
```

**WARNING**: Be very careful when specifying target devices. Incorrect usage could interfere with disk I/O.

### Checking Module Status

View kernel messages:
```bash
dmesg | tail -20
```

List loaded modules:
```bash
lsmod | grep misc
```

Check module information:
```bash
modinfo misc.ko
```

### Unloading the Module

```bash
sudo rmmod misc
```

### System Installation (Optional)

To install the module system-wide:
```bash
sudo make install
```

To uninstall:
```bash
sudo make uninstall
```

## Usage

### Module Parameters

- **target_device** (string): Path to the block device to filter
  - Default: `/dev/sda1`
  - Example: `target_device=/dev/nvme0n1p1`

### Control Interface

The driver creates a misc device at `/dev/blockfilter` that supports ioctl commands:

**Get I/O Statistics**:
```c
#include <sys/ioctl.h>
#include <fcntl.h>

#define MISC_GET _IOR('B', 0x01, unsigned long)

int fd = open("/dev/blockfilter", O_RDONLY);
unsigned long bio_count;
ioctl(fd, MISC_GET, &bio_count);
printf("Total BIOs processed: %lu\n", bio_count);
close(fd);
```

### Viewing Debug Output

Enable debug messages:
```bash
echo 8 > /proc/sys/kernel/printk
```

View real-time kernel logs:
```bash
sudo dmesg -w
```

## Code Structure

```
misc/
├── misc.c       - Main driver implementation
├── misc.h       - Header with ioctl definitions
└── Makefile     - Build configuration
```

### Key Functions

- `filter_submit_bio()`: Bio submission callback that intercepts I/O
- `register_block_device()`: Opens target device and creates filter device
- `unregister_block_device()`: Cleanup and resource release
- `mischelp_ioctl()`: Control interface for statistics and management

## Important Changes from Original Version

This modernized version includes:

1. **Modern Block Layer API**: Uses `blk_alloc_disk()` and `submit_bio` instead of deprecated `request_fn`
2. **Proper Reference Counting**: Uses `blkdev_get_by_path()`/`blkdev_put()` for safe device access
3. **Comprehensive Error Handling**: All error paths properly cleanup resources
4. **NULL Pointer Protection**: Validates all pointers before use
5. **Module Parameters**: Configurable target device without recompilation
6. **Better Logging**: Uses `pr_*` macros with appropriate log levels
7. **Thread Safety**: Uses mutex for synchronization
8. **Atomic Statistics**: Lock-free statistics using atomic64_t
9. **Proper IOCTL Definitions**: Uses standard `_IOR`/`_IOW` macros
10. **Clean Code**: Removed unused headers and improved documentation

## Limitations & Caveats

- **Educational Purpose**: This is a demonstration driver, not production-ready
- **Performance Impact**: Bio cloning adds overhead to I/O path
- **No Hot-Plug Support**: Target device must exist when module loads
- **Single Target**: Only supports one target device per module instance
- **Root Required**: Must run as root to load kernel modules

## Safety Warnings

⚠️ **IMPORTANT SAFETY NOTES**:

- This module operates at the kernel level and can cause system instability if misused
- Always test on non-production systems first
- Specifying the wrong target device could cause data loss
- The default target (`/dev/sda1`) may be your system drive - change it if needed
- Kernel panics are possible if the target device is removed while the module is loaded

## Troubleshooting

**Module fails to load**:
- Check kernel version: `uname -r` (must be 5.0+)
- Verify kernel headers are installed: `ls /lib/modules/$(uname -r)/build`
- Check dmesg for specific error messages

**Target device errors**:
- Ensure the device path exists: `ls -l /dev/sda1`
- Verify you have permission to access the device
- Check that the device is not exclusively locked by another process

**Compilation errors**:
- Ensure kernel headers match your running kernel
- Update to latest kernel headers: `sudo apt update && sudo apt install linux-headers-$(uname -r)`

## Development & Debugging

Enable additional debugging by modifying the code:
- Change `pr_debug()` to `pr_info()` in `misc.c:107` to see all I/O operations
- Add custom logic in `filter_submit_bio()` to inspect/modify I/O requests

## Backstory

The original driver was written in 2013 for Linux 3.x kernels. This updated version (2025) modernizes the code for current Linux kernels (5.0+), which underwent major block layer rewrites. The kernel's transition from request_fn to blk-mq required a complete rewrite of the I/O interception mechanism.

## License

MIT License - See source files for full license text.

## Keywords

Linux kernel, block filter driver, blk-mq, bio interception, block I/O monitoring, device mapper, kernel module

## Author

Original: Asim Kadav (asimkadav@gmail.com)
Modernized: 2025 update for Linux 5.0+ kernels

## Disclaimer

**NO WARRANTIES. Use at your own risk.** This software is provided "as is" without warranty of any kind. Test thoroughly in safe environments before any production use.
