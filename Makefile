# PlatformIO workflow adapted from Friedjof/AutomaticChickenFeeder.
# The web assets are gzipped into a C header before firmware compilation.

PLATFORMIO ?= pio
BOARD ?= esp32c3
PYTHON ?= python3
OTA_HOST ?= tv-remote.local

ACTION_TARGETS := flash monitor run
ifneq ($(filter $(ACTION_TARGETS),$(firstword $(MAKECMDGOALS))),)
  ARG := $(word 2,$(MAKECMDGOALS))
  ifneq ($(ARG),)
    NR := $(ARG)
    $(eval $(ARG):;@:)
  endif
endif

ifdef NR
  UPLOAD_FLAG := --upload-port /dev/ttyACM$(NR)
  MONITOR_FLAG := --port /dev/ttyACM$(NR)
else ifdef PORT
  UPLOAD_FLAG := --upload-port $(PORT)
  MONITOR_FLAG := --port $(PORT)
else
  UPLOAD_FLAG :=
  MONITOR_FLAG :=
endif

.PHONY: all build flash ota monitor run test clean list web-headers profile-headers web-dev

all: build

build: web-headers profile-headers candidate-header
	$(PLATFORMIO) run --environment $(BOARD)

web-headers:
	$(PYTHON) scripts/web-to-header.py data -o include/generated/web_files.h

profile-headers:
	$(PYTHON) scripts/profiles-to-header.py data/profiles -o include/generated/default_profiles.h

candidate-header:
	$(PYTHON) scripts/candidates-to-header.py

flash: build
	$(PLATFORMIO) run --target upload --environment $(BOARD) $(UPLOAD_FLAG)

ota: web-headers profile-headers candidate-header
	@test -n "$$OTA_PASSWORD" || { echo "Set OTA_PASSWORD in the environment before running make ota"; exit 1; }
	@PLATFORMIO_UPLOAD_FLAGS="--auth=$$OTA_PASSWORD" $(PLATFORMIO) run --target upload --environment esp32c3_ota --upload-port "$(OTA_HOST)"

monitor:
	$(PLATFORMIO) device monitor --environment $(BOARD) $(MONITOR_FLAG)

run: flash monitor

test: web-headers profile-headers candidate-header
	$(PYTHON) -m unittest discover -s test -v
	node --check data/app.js

clean:
	$(PLATFORMIO) run --target clean --environment $(BOARD)

web-dev:
	$(PYTHON) -m http.server 8000 --directory data

list:
	$(PLATFORMIO) device list
