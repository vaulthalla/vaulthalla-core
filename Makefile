# ========== VAULTHALLA MAKEFILE ==========
# ⚒️  Forge build & deployment targets
# =========================================

.PHONY: all build install clean uninstall

# Default target: build
all: build

## 🔨 Build via Conan
build:
	@echo "🔧 Building Vaulthalla..."
	conan install . -r vaulthalla --build=missing
	conan build .

## 🛠️ Install system-wide
install:
	@echo "🛡️  Running install script..."
	./bin/install.sh

## 🧼 Uninstall everything
clean uninstall:
	@echo "💣 Running uninstall script..."
	./bin/uninstall.sh
