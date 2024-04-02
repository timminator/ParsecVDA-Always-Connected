<p align="center">
  <img src="https://i.imgur.com/dDUa6GH.png" width="64" height="64" />
  <h1 align="center">ParsecVDA - Always Connected</h1>
  <p align="center">
    Connects a Virtual Display to your PC
    <br />
    and allows for a headless operation -
    <br />
    no need for Dummy Plugs anymore!
  </p>
</p>

<br>

## ℹ About

This project is based on the "parsec-vdd" project from nomi-san. It adds a Virtual Display to your system by using the [Parsec VDD Driver](https://support.parsec.app/hc/en-us/articles/4422939339789-Overview-Prerequisites-and-Installation), without relying on the [Parsec app](https://parsec.app/). The Virtual Display will stay connected until you shutdown or restart your computer even when you disconnect through remote apps like Parsec, which is an important feature of this project. 

With this program it is possible to run a PC completely headless without relying on Dummy Plugs.

Furthermore this program can be used in the Hyper-V environment in combination with GPU-Paravirtualization (see GPU-PV) where you cannot disconnect the Hyper-V-Monitor which leads to Parsec not automatically falling back to its Virtual Display. This program adds a Virtual Display nonetheless.



> The Virtual Display Driver (VDD) is required to enable virtual displays on a Windows host. Virtual displays is a feature available for **Teams** and **Warp** customers that lets you add up to 3 additional virtual displays to the host while connecting to a machine you own through Parsec.

> **Parsec VDD** is a perfect software driver developed by Parsec. It utilizes the [IddCx API](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/indirect-display-driver-model-overview) (Indirect Display Driver) to create virtual displays on Windows 10+. This virtual display is particularly useful in situations where a physical monitor may not be available or when additional screens are desired.

> One of the notable features of Parsec VDD is its support for a wide range of [resolutions and refresh rates](#preset-display-modes), including up to 4K and 240 Hz. This makes it well-suited for gaming, as it can provide a high-quality visual experience. It enables users to simulate the presence of additional screens or work without a physical monitor, enhancing flexibility and customization in display management.


## Steps to get it running:

1. Make sure you have installed the Parsec VDD Driver:
- [parsec-vdd-v0.41](https://builds.parsec.app/vdd/parsec-vdd-0.41.0.0.exe)
- [parsec-vdd-v0.45](https://builds.parsec.app/vdd/parsec-vdd-0.45.0.0.exe) (recommended)

2. Download the release and extract the folder to your preferred location.

3. Double click the batch file "Setup.bat"

  
That's all you have to do! On your next restart a Virtual Display should be added to your system!
     
     
## Info:

- The batch file creates a .vbs file to start the application hidden without a console window open.
Furthermore it creates a registry entry to start the .vbs file at startup. 

## Notes:

- Make sure that in your preferred location it is possible to create files without admin rights (suitable locations are for example your Documents or Downloads folder).
- Because the application is not running as a service, it can take a while until it starts at startup. Nevertheless it starts before you login, so logging in is perfectly fine with this program, it just takes a few seconds.
- You will hear a connect/disconnect sound at startup/shutdown because the Virtual Display is added/removed at every start/shutdown.
- If you're using this program together with Parsec, you have to make sure that the Virtual Display setting in the Parsec settings is set to off.
- The program also creates a basic logfile in the same directory.
  



## 😥 Known Limitations

> This list shows the known limitations of Parsec VDD.

### 1. HDR support

Parsec VDD does not support HDR on its displays (see the EDID below). Theoretically, you can unlock support by editing the EDID, then adding HDR metadata and setting 10-bit+ color depth. Unfortunately, you cannot flash its firmware like a physical device, or modify the registry value.

All IDDs have their own fixed EDID block inside the driver binary to initialize the monitor specs. So the solution is to modify this block in the driver DLL (mm.dll), then update it with `devcon` CLI.

```
admin$ > devcon update driver\mm.inf Root\Parsec\VDA
```

### 2. Custom resolutions

Before connecting, the virtual display looks in the `HKEY_LOCAL_MACHINE\SOFTWARE\Parsec\vdd` registry for additional preset resolutions. Currently this supports a maximum of 5 values.

```
SOFTWARE\Parsec\vdd
  key: 0 -> 5 | (width, height, hz)
```

To unlock this limit, you need to patch the driver DLL the same way as above, but **5 is enough** for personal use.

## 😑 Known Bugs

> This is a list of known issues when working with standalone Parsec VDD.

### 1. Incompatible with Parsec Privacy Mode

![Alt text](https://i.imgur.com/C74IRgC.png)

If you have enabled "Privacy Mode" in Parsec Host settings, please disable it and clear the connected display configruations in the following Registry path.

```
HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\GraphicsDrivers\Connectivity
```

This option causes your main display to turn off when virtual displays are added, making it difficult to turn the display on and disrupting the remote desktop session.

## 🤔 Comparison with other IDDs

The table below shows a comparison with other popular Indirect Display Driver projects. 

| Project                         | Iddcx version | Signed | Gaming | HDR  | H-Cursor | Tweakable | Controller 
| :--                             | :--:          | :--:   | :--:   | :--: | :--:     | :-:       | :-:
| [usbmmidd_v2]                   |               | ✅     | ❌    |  ❌  |  ❌     |           |
| [IddSampleDriver]               | 1.2           | ❌     | ❌    |  ❌  |  ❌     |           |
| [RustDeskIddDriver]             | 1.2           | ❌     | ❌    |  ❌  |  ❌     |           |
| [Virtual-Display-Driver (HDR)]  | 1.10          | ❌     |       |  ✅  |  ❌     |            |
| [virtual-display-rs]            | 1.5           | ❌     |       |  ❌   | ❌     |    ✅     |  ✅
| parsec-vdd                      | 1.4           | ✅     | ✅    |  ❌  |  ✅     |   🆗     |  ✅

✅ - full support, 🆗 - limited support

[usbmmidd_v2]: https://www.amyuni.com/forum/viewtopic.php?t=3030
[IddSampleDriver]: https://github.com/roshkins/IddSampleDriver
[RustDeskIddDriver]: https://github.com/fufesou/RustDeskIddDriver
[virtual-display-rs]: https://github.com/MolotovCherry/virtual-display-rs
[Virtual-Display-Driver (HDR)]: https://github.com/itsmikethetech/Virtual-Display-Driver

**Signed** means that the driver files have a valid digital signature.
**H-Cursor** means hardware cursor support, without it you will get double cursor on some remote desktop apps.
**Tweakable** is the ability to customize display modes. Visit [MSDN IddCx versions](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/iddcx-versions) to check the minimum supported Windows version.

## 📘 Parsec VDD Specs

### Preset display modes

All of the following display modes are set by driver default.

| Resolution   | Common name      | Aspect ratio         | Refresh rates (Hz)
| -            | :-:              | :-:                  | :-:
| 4096 x 2160  | DCI 4K           | 1.90:1 (256:135)     | 24/30/60/144/240
| 3840 x 2160  | 4K UHD           | 16:9                 | 24/30/60/144/240
| 3840 x 1600  | UltraWide        | 24:10                | 24/30/60/144/240
| 3840 x 1080  | UltraWide        | 32:9 (2x 16:9 FHD)   | 24/30/60/144/240
| 3440 x 1440  |                  | 21.5:9 (43:18)       | 24/30/60/144/240
| 3240 x 2160  |                  | 3:2                  | 60
| 3200 x 1800  | 3K               | 16:9                 | 24/30/60/144/240
| 3000 x 2000  |                  | 3:2                  | 60
| 2880 x 1800  | 2.8K             | 16:10                | 60
| 2880 x 1620  | 2.8K             | 16:9                 | 24/30/60/144/240
| 2736 x 1824  |                  |                      | 60
| 2560 x 1600  | 2K               | 16:10                | 24/30/60/144/240
| 2560 x 1440  | 2K               | 16:9                 | 24/30/60/144/240
| 2560 x 1080  | UltraWide        | 21:9                 | 24/30/60/144/240
| 2496 x 1664  |                  |                      | 60
| 2256 x 1504  |                  |                      | 60
| 2048 x 1152  |                  |                      | 60/144/240
| 1920 x 1200  | FHD              | 16:10                | 60/144/240
|**1920 x 1080**| **FHD**         | **16:9**             | 24/30/**60**/144/240
| 1800 x 1200  | FHD              | 3:2                  | 60
| 1680 x 1050  | HD+              | 16:10                | 60/144/240
| 1600 x 1200  | HD+              | 4:3                  | 24/30/60/144/240
|  1600 x 900  | HD+              | 16:9                 | 60/144/240
|  1440 x 900  | HD               | 16:10                | 60/144/240
|  1366 x 768  |                  |                      | 60/144/240
|  1280 x 800  | HD               | 16:10                | 60/144/240
|  1280 x 720  | HD               | 16:9                 | 60/144/240

Notes:
- Low GPUs, e.g GTX 1650 will not support the highest DCI 4K.
- All resolutions are compatible with 60 Hz refresh rates.


### Adapter info

- Name: `Parsec Virtual Display Adapter`
- Hardware ID: `Root\Parsec\VDA`
- Adapter GUID: `{00b41627-04c4-429e-a26e-0265cf50c8fa}`
- Class GUID: `{4d36e968-e325-11ce-bfc1-08002be10318}`

### Monitor info

- ID: `PSCCDD0`
- Name: `ParsecVDA`
- EDID:

```
00 FF FF FF FF FF FF 00  42 63 D0 CD ED 5F 84 00
11 1E 01 04 A5 35 1E 78  3B 57 E0 A5 54 4F 9D 26
12 50 54 27 CF 00 71 4F  81 80 81 40 81 C0 81 00
95 00 B3 00 01 01 86 6F  80 A0 70 38 40 40 30 20
35 00 E0 0E 11 00 00 1A  00 00 00 FD 00 30 A5 C1
C1 29 01 0A 20 20 20 20  20 20 00 00 00 FC 00 50
61 72 73 65 63 56 44 41  0A 20 20 20 00 00 00 10
00 00 00 00 00 00 00 00  00 00 00 00 00 00 01 C6
02 03 10 00 4B 90 05 04  03 02 01 11 12 13 14 1F
8A 4D 80 A0 70 38 2C 40  30 20 35 00 E0 0E 11 00
00 1A FE 5B 80 A0 70 38  35 40 30 20 35 00 E0 0E
11 00 00 1A FC 7E 80 88  70 38 12 40 18 20 35 00
E0 0E 11 00 00 1E A4 9C  80 A0 70 38 59 40 30 20
35 00 E0 0E 11 00 00 1A  02 3A 80 18 71 38 2D 40
58 2C 45 00 E0 0E 11 00  00 1E 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 A6
```

Visit http://www.edidreader.com/ to view it online or use an advanced tool [AW EDID Editor](https://www.analogway.com/apac/products/software-tools/aw-edid-editor/)
