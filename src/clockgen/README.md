# clockgen
This is an updated [clockgen](https://gitlab.com/wolfre/cxadc-clock-generator-audio-adc/) firmware with Windows-specific features and fixes.  
The PowerShell module located [here](CxadcClockGen.psm1) can be used to manage the the device on Windows.  

## Changes
- Add vendor interface for clock selection
- Fix device/interface name
- Fix bug with `maxEPsize`
- Change device vendor/product to `1209:0002`

## Dependencies
- [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
- [pico-sdk](https://github.com/raspberrypi/pico-sdk) (`2.1.1` or later)
- [tinyusb](https://github.com/hathach/tinyusb) (`334ac8072650e3b18278042c2c2b402db73c7359` or later)
