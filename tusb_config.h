/* TinyUSB settings for Pico MZ-80K and MZ-80A */
/* Tim Holyoake, October 2024 - February 2025. */

#pragma once

#define CFG_TUSB_RHPORT0_MODE OPT_MODE_HOST

#define CFG_TUH_HUB 1
#define CFG_TUH_HID 4
#define CFG_TUH_VENDOR 0

#define CFG_TUSB_HOST_DEVICE_MAX (CFG_TUH_HUB ? 5 : 1) // normal hub has 4 ports
