# AM275x USB

This repository keeps `main` as a branch index only.  Firmware source code is maintained on feature branches.

## Branches

- `usb-device` - common AM275x USB device controller and class-driver framework.
- `usb-msc` - USB Mass Storage device firmware for exposing eMMC to the host.
- `uac` - UAC2 device firmware branch.

## Usage

Check out the branch that matches the USB device function you want to work on:

```bash
git checkout usb-msc
```

or:

```bash
git checkout uac
```

The CCS project is available on the feature branches, not on `main`.
