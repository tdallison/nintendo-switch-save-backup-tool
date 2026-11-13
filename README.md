# Switch Payload Launcher

A small GTK application that sends the included `hekate.bin` with the included
`fusee-nano` launcher. Both files are embedded into the compiled program, so
`SwitchPayloadLauncher` is the only executable a user needs to start it.

## Build and run

```bash
make
./SwitchPayloadLauncher
```

## Desktop icon

Run `make desktop-launcher` to create **Switch Payload Launcher.desktop** in
this folder. Copy that file to your desktop (or `~/.local/share/applications`)
to launch the app from an icon with the Switch artwork.

Click **Send Hekate Payload** after connecting a Switch in RCM mode. The app
uses PolicyKit (`pkexec`) for the administrator permission required for USB
access; it does not run a terminal or keep a password in the program.

## Create an AppImage

Install `appimagetool`, then run:

```bash
make appimage
```

That produces `SwitchPayloadLauncher-x86_64.AppImage`. For distribution to
systems that may not have GTK 3 installed, use `linuxdeploy` to add GTK runtime
libraries to `build/AppDir` before running `appimagetool`.

## Notes

The embedded `fusee-nano` is x86-64 Linux, so this build targets 64-bit Intel/
AMD Linux systems. Replace it with an ARM build and adjust the `objcopy` target
in the Makefile when building for ARM Linux.
