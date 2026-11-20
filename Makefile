API ?= 35

# The NDK directory layout is platform specific. Auto-detect the host so a
# plain `make` works on Linux, macOS and Windows (via Git Bash / MSYS2).
NDK_ROOT ?= $(or $(ANDROID_NDK_HOME),$(ANDROID_NDK_ROOT))

ifeq ($(OS),Windows_NT)
  HOST_TAG := windows-x86_64
else
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Darwin)
    HOST_TAG := darwin-x86_64
  else
    HOST_TAG := linux-x86_64
  endif
endif

BIN := $(NDK_ROOT)/toolchains/llvm/prebuilt/$(HOST_TAG)/bin
NDK_CC := $(BIN)/aarch64-linux-android$(API)-clang
# Windows ships clang only as a .cmd wrapper and drops quoted -D values, so
# offset.h defaults TARGET_CONFIG_H to "target.h" rather than relying on it.
ifeq ($(OS),Windows_NT)
  NDK_CC := $(NDK_CC).cmd
endif

SRCS := \
  src/core/main.c \
  src/core/util.c \
  src/core/slide.c \
  src/core/fops.c \
  src/core/pipe_physrw.c \
  src/core/root.c \
  src/core/miniadb.c \
  src/core/umh_root.c

# -DTARGET_CONFIG_H is deliberately omitted: offset.h defaults it. Passing it
# through the Windows clang wrapper corrupts the quotes.
CFLAGS := -O2 -Wall -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function \
  -Isrc/core -Isrc/devices
LDFLAGS := -fPIE -pie -pthread

.PHONY: all clean check

all: ghostlock

ghostlock: $(SRCS) $(wildcard src/core/*.h src/devices/*.h src/devices/*/*.h)
	$(NDK_CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@

# Fails the build if any translation unit still resolves a per-device symbol
# offset to a compile-time constant instead of active_offsets->*.
check:
	@echo "checking runtime offset binding…"
	@for f in src/core/fops.c src/core/util.c src/core/slide.c src/core/root.c; do \
	  if $(NDK_CC) -E $(CFLAGS) $$f 2>/dev/null | grep -qE 'off_ashmem_open'; then \
	    echo "  ok   $$f"; \
	  else \
	    echo "  FAIL $$f does not use active_offsets"; exit 1; \
	  fi; \
	done

clean:
	rm -f ghostlock
