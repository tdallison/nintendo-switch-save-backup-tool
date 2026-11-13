APP := SwitchPayloadLauncher
BUILD := build
CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -O2 $(shell pkg-config --cflags gtk+-3.0)
LDLIBS := $(shell pkg-config --libs gtk+-3.0)
APPIMAGETOOL ?= appimagetool

.PHONY: all clean appimage desktop-launcher

all: $(APP)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/fusee_nano.o: fusee-nano | $(BUILD)
	objcopy -I binary -O elf64-x86-64 -B i386:x86-64 --rename-section .data=.rodata,alloc,load,readonly,data,contents $< $@

$(BUILD)/hekate_bin.o: hekate.bin | $(BUILD)
	objcopy -I binary -O elf64-x86-64 -B i386:x86-64 --rename-section .data=.rodata,alloc,load,readonly,data,contents $< $@

$(BUILD)/switch_launcher_svg.o: icons/switch-launcher.svg | $(BUILD)
	objcopy -I binary -O elf64-x86-64 -B i386:x86-64 --rename-section .data=.rodata,alloc,load,readonly,data,contents $< $@

$(APP): switch-launcher.c $(BUILD)/fusee_nano.o $(BUILD)/hekate_bin.o $(BUILD)/switch_launcher_svg.o
	$(CC) $(CFLAGS) -o $@ switch-launcher.c $(BUILD)/fusee_nano.o $(BUILD)/hekate_bin.o $(BUILD)/switch_launcher_svg.o $(LDLIBS)

# Requires appimagetool. This makes one distributable file; install GTK runtime
# dependencies into the AppDir with linuxdeploy first if targeting systems without GTK 3.
appimage: $(APP)
	rm -rf $(BUILD)/AppDir
	mkdir -p $(BUILD)/AppDir/usr/bin $(BUILD)/AppDir/usr/share/icons/hicolor/scalable/apps
	cp $(APP) $(BUILD)/AppDir/usr/bin/
	cp switch-payload-launcher.desktop $(BUILD)/AppDir/
	cp icons/switch-launcher.svg $(BUILD)/AppDir/switch-payload-launcher.svg
	cp icons/switch-launcher.svg $(BUILD)/AppDir/usr/share/icons/hicolor/scalable/apps/switch-payload-launcher.svg
	$(APPIMAGETOOL) $(BUILD)/AppDir $(APP)-x86_64.AppImage

# Creates a double-clickable launcher in the project folder.  Copy it to your
# desktop or ~/.local/share/applications to show the Switch icon in your menu.
desktop-launcher: $(APP)
	sed -e 's|^Exec=.*|Exec=$(CURDIR)/$(APP)|' -e 's|^Icon=.*|Icon=$(CURDIR)/icons/switch-launcher.svg|' switch-payload-launcher.desktop > "Switch Payload Launcher.desktop"
	chmod +x "Switch Payload Launcher.desktop"

clean:
	rm -rf $(BUILD) $(APP) $(APP)-x86_64.AppImage "Switch Payload Launcher.desktop"
