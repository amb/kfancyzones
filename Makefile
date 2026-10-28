SCRIPT_NAME := kfancyzones
PKGFILE := $(SCRIPT_NAME).kwinscript
NATIVE_BUILD_DIR := build/native

.PHONY: all test build native check install uninstall clean enable disable logs

all: build

test: check

native:
	@cmake -S native -B $(NATIVE_BUILD_DIR)
	@cmake --build $(NATIVE_BUILD_DIR) --parallel

build: native
	@python3 bin/package.py

check:
	@node --test --test-isolation=none tests/selection.test.cjs tests/drop.test.cjs tests/layouts.test.cjs

install: build
	@kpackagetool6 --type=KWin/Script -i $(PKGFILE) || \
	kpackagetool6 --type=KWin/Script -u $(PKGFILE)

uninstall:
	@kpackagetool6 --type=KWin/Script -r $(SCRIPT_NAME)

clean:
	@rm -f $(PKGFILE)

enable:
	@kwriteconfig6 --file kwinrc --group Plugins --key $(SCRIPT_NAME)Enabled true
	@qdbus6 org.kde.KWin /KWin reconfigure

disable:
	@kwriteconfig6 --file kwinrc --group Plugins --key $(SCRIPT_NAME)Enabled false
	@qdbus6 org.kde.KWin /KWin reconfigure

logs:
	@journalctl --user -u plasma-kwin_wayland -f QT_CATEGORY=js QT_CATEGORY=qml QT_CATEGORY=kwin_scripting
