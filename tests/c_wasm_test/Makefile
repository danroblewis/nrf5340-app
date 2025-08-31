# Makefile for C WASM compilation using Emscripten
# 
# Prerequisites:
# - Emscripten SDK (emsdk) installed and activated
# - emcc command available in PATH

# Compiler and flags
CC = emcc
CFLAGS = -O2 -s WASM=1 -s EXPORTED_FUNCTIONS='["_getNumber","_add","_multiply","_subtract","_divide","_max","_min","_isEven","_isOdd","_abs"]' -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' -s ALLOW_MEMORY_GROWTH=1

# Source files
SOURCES = simple.c
TARGET = simple.wasm

# Default target
all: $(TARGET)

# Build WASM file
$(TARGET): $(SOURCES)
	@echo "🔨 Building C WASM: $@"
	$(CC) $(CFLAGS) $(SOURCES) -o $@
	@echo "✅ Built: $@"
	@echo "📏 File size: $(shell wc -c < $@) bytes"

# Rebuild (clean + build)
rebuild: clean all

# Clean build artifacts
clean:
	@echo "🧹 Cleaning build artifacts..."
	rm -f $(TARGET)
	@echo "✅ Cleaned"

# Check file size and info
check-size:
	@echo "📏 WASM file information:"
	@if [ -f $(TARGET) ]; then \
		echo "  File: $(TARGET)"; \
		echo "  Size: $(shell wc -c < $(TARGET)) bytes"; \
		echo "  Created: $(shell ls -la $(TARGET) | awk '{print $$6, $$7, $$8}')"; \
	else \
		echo "  ❌ $(TARGET) not found. Run 'make' to build it."; \
	fi

# Show WASM info using wasm-objdump if available
info:
	@echo "🔍 WASM file information:"
	@if [ -f $(TARGET) ]; then \
		echo "  File: $(TARGET)"; \
		echo "  Size: $(shell wc -c < $(TARGET)) bytes"; \
		echo ""; \
		if command -v wasm-objdump >/dev/null 2>&1; then \
			echo "📋 Exported functions:"; \
			wasm-objdump -x $(TARGET) | grep -E "(Function|Export)" | head -20; \
		else \
			echo "⚠️  wasm-objdump not available. Install WABT for detailed info."; \
		fi; \
	else \
		echo "  ❌ $(TARGET) not found. Run 'make' to build it."; \
	fi

# Install dependencies (Emscripten)
install-deps:
	@echo "📦 Installing Emscripten dependencies..."
	@if ! command -v emcc >/dev/null 2>&1; then \
		echo "❌ Emscripten not found. Installing..."; \
		cd ../emsdk && ./emsdk install latest && ./emsdk activate latest; \
		echo "✅ Emscripten installed. Please run: source ../emsdk/emsdk_env.sh"; \
	else \
		echo "✅ Emscripten already installed: $(shell emcc --version | head -1)"; \
	fi

# Show help
help:
	@echo "C WASM Build System"
	@echo "=================="
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build the WASM file (default)"
	@echo "  rebuild      - Clean and rebuild"
	@echo "  clean        - Remove build artifacts"
	@echo "  check-size   - Show file size and creation time"
	@echo "  info         - Show detailed WASM information"
	@echo "  install-deps - Install Emscripten dependencies"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make                    # Build simple.wasm"
	@echo "  make rebuild            # Clean and rebuild"
	@echo "  make clean             # Clean build artifacts"
	@echo "  make check-size        # Check file size"
	@echo "  make info              # Show WASM details"
	@echo "  make install-deps      # Install Emscripten"
	@echo ""
	@echo "Prerequisites:"
	@echo "  - Emscripten SDK (emsdk)"
	@echo "  - Run: source ../emsdk/emsdk_env.sh"
	@echo "  - Optional: WABT (wasm-objdump) for detailed info"

# Phony targets
.PHONY: all rebuild clean check-size info install-deps help
