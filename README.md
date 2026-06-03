# AM275x USB Device

This workspace contains a CCS project for AM275x USB device development on the AM275x EVM R5F core.  The current firmware exposes the board eMMC as a USB Mass Storage Class device and initializes the media with a full-disk FAT32 layout when needed.

## Project

- CCS project: `am275xusb`
- Target: AM275x EVM, `MAIN_Cortex_R5_0_0`
- SDK tested with: `FreeRTOS SDK for AM275x 12.0.0.22`
- SysConfig tested with: `1.27.1`

## Source Layout

- `am275xusb/main.c` - FreeRTOS task entry and driver open/close sequence.
- `am275xusb/am275xusb.c` - USB bring-up, eMMC FAT32 check, and format flow.
- `am275xusb/example.syscfg` - TI driver configuration.
- `am275xusb/components/usb_device/` - Common USB device, EP0, and AM275x DWC3 hardware code.
- `am275xusb/apps/usb_msc/` - USB Mass Storage Class implementation for exposing eMMC.
- `am275xusb/apps/uac2/` - UAC2 code carried forward for the future audio-device branch.
- `am275xusb/docs/` - Local implementation notes for the AM275x USB controller.

## Notes

- The firmware uses eMMC as a block device for USB MSC.  It intentionally avoids mounting FreeRTOS+FAT during normal USB operation so the host owns the filesystem.
- On first boot, if the eMMC does not contain the expected full-disk FAT32 layout, the firmware writes a quick FAT32 layout and erases the existing partition table/filesystem metadata.
- Large local reference files, packet captures, serial logs, and CCS build outputs are ignored by git.

## Build

Import or open the CCS project directory and build the `Debug` configuration.  SysConfig-generated files and CCS build outputs are regenerated under the ignored build output directory.
