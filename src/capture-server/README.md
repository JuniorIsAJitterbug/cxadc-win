# capture-server

This is a Windows port of [cxadc_vhs_server](https://github.com/namazso/cxadc_vhs_server).  
Effort has been made to preserve cross-platform compatibility but testing has primarily been on Windows.  

## Changes (initial release)
- Added support for `cxadc-win`
- Added support for WASAPI audio capture
- Added PowerShell script for local capture
- Local capture script uses `FLAC` and `SoX` instead of `FFmpeg` for Video/HiFi resampling and compression
- Renamed linear to baseband
- Baseband audio is no longer captured by default
- Replaced ringbuffer with cross-platform implementation from [MISRC](https://github.com/Stefan-Olt/MISRC)
- Replaced `pthreads` with native C11 threads
