.PHONY: build flash monitor
build:
	idf.py build
flash:
	idf.py -p /dev/ttyUSB0 flash
monitor:
	idf.py -p /dev/ttyUSB0 monitor
all: build flash monitor
