# ========== VAULTHALLA MAKEFILE ==========
# ⚒️  Forge build & deployment targets
# =========================================

.PHONY: all build install dev clean uninstall

# Default target: build
all: build

## 🔨 Build via Conan
build:
	@echo "🔧 Building Vaulthalla..."
	conan install . -r vaulthalla --build=missing
	conan build .

## 🛠️ Install system-wide
install:
	./bin/setup/install_guard.sh
	@echo "🛡️  Running install script..."
	./bin/install.sh

## 🛠️ Install in development mode
dev:
	./bin/uninstall.sh -d
	./bin/setup/install_guard.sh
	@echo "🛡️  Running install script..."
	./bin/install.sh -d

deb:
	@echo "🔧 Building Debian package..."
	./bin/install_deb.sh

release:
	@echo "🔧 Building release package..."
	./bin/install_release.sh

## 🧼 Uninstall everything
clean uninstall:
	@echo "💣 Running uninstall script..."
	./bin/uninstall.sh
