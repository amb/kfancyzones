SCRIPT_NAME := kfancyzones
PKGFILE := $(SCRIPT_NAME).kwinscript
SRC_DIR := src
NATIVE_BUILD_DIR := build/native
SESSION_WIDTH := 1920
SESSION_HEIGHT := 1080
SESSION_OUTPUT_COUNT := 1
SESSION_VERBOSE := 0
SESSION_APPLICATIONS := # dolphin konsole kate

.NOTPARALLEL: all

.PHONY: all test build native check install uninstall clean enable disable start-session

all: build

test: check

build: $(PKGFILE)

native:
	@cmake -S native -B $(NATIVE_BUILD_DIR)
	@cmake --build $(NATIVE_BUILD_DIR) --parallel
	@mkdir -p $(SRC_DIR)/contents/ui/liveinput
	@cp $(NATIVE_BUILD_DIR)/qml/qmldir $(NATIVE_BUILD_DIR)/qml/libkfancyzonesliveinputplugin.so $(SRC_DIR)/contents/ui/liveinput/
	@cp $(SRC_DIR)/contents/ui/main.qml $(SRC_DIR)/contents/ui/LiveSettingsOverlay.qml
	@cp $(NATIVE_BUILD_DIR)/config/libkfancyzonesconfig.so $(SRC_DIR)/contents/config/
	@cp LICENSE.md $(SRC_DIR)/LICENSE.md

check:
	@python3 bin/generate-settings.py --check
	@node --test --test-isolation=none tests/selection.test.cjs tests/drop.test.cjs tests/layouts.test.cjs

$(PKGFILE): native $(shell find $(SRC_DIR) -type f)
	@echo "Packaging $(SRC_DIR) into $(PKGFILE)..."
	@rm -f $(PKGFILE)
	@python3 bin/package.py

install: build
	@echo "Installing $(PKGFILE)..."
	@kpackagetool6 --type=KWin/Script -i $(PKGFILE) || \
	kpackagetool6 --type=KWin/Script -u $(PKGFILE)

uninstall:
	@echo "Uninstalling $(SCRIPT_NAME)..."
	@kpackagetool6 --type=KWin/Script -r $(SCRIPT_NAME)

clean:
	@echo "Cleaning up $(PKGFILE)..."
	@rm -f $(PKGFILE)

enable:
	@echo "Enabling $(SCRIPT_NAME)..."
	@kwriteconfig6 --file kwinrc --group Plugins --key $(SCRIPT_NAME)Enabled true
	@qdbus6 org.kde.KWin /KWin reconfigure

disable:
	@echo "Disabling $(SCRIPT_NAME)..."
	@kwriteconfig6 --file kwinrc --group Plugins --key $(SCRIPT_NAME)Enabled false
	@qdbus6 org.kde.KWin /KWin reconfigure

restart-kwin:
	if [ "$$XDG_SESSION_TYPE" = "x11" ]; then \
		kwin_x11 --replace & \
	elif [ "$$XDG_SESSION_TYPE" = "wayland" ]; then \
		kwin_wayland --replace & \
	else \
		echo "Unknown session type"; \
	fi

logs:
	@if [ "${XDG_SESSION_TYPE}" = "x11" ]; then \
	    journalctl -f -t kwin_x11; \
	else \
	    journalctl --user -u plasma-kwin_wayland -f QT_CATEGORY=js QT_CATEGORY=qml QT_CATEGORY=kwin_scripting; \
	fi


start-session:
	@echo "Starting nested Wayland session..."
	@sh -c '\
		unset LD_PRELOAD; \
		NESTED_DIR="$$XDG_RUNTIME_DIR/nested_plasma"; \
		mkdir -p "$$NESTED_DIR"; \
		WRAPPER="$$NESTED_DIR/kwin_wayland_wrapper"; \
		printf "#!/bin/sh\n/usr/bin/kwin_wayland_wrapper --width $(SESSION_WIDTH) --height $(SESSION_HEIGHT) --no-lockscreen --output-count $(SESSION_OUTPUT_COUNT) $(SESSION_APPLICATIONS) \\\$$@\n" > "$$WRAPPER"; \
		chmod a+x "$$WRAPPER"; \
		export PATH="$$NESTED_DIR:$$PATH"; \
		if [ "$(SESSION_VERBOSE)" = "1" ]; then \
			dbus-run-session startplasma-wayland; \
		else \
			dbus-run-session startplasma-wayland 2>&1 | grep -E "qml"; \
		fi; \
		rm -f "$$WRAPPER"'

load:
	bin/load.sh "$(SRC_DIR)" "$(SCRIPT_NAME)-test"

unload:
	bin/unload.sh "$(SCRIPT_NAME)-test"

reload: unload load
