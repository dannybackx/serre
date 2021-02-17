# MAKEESPARDUINO_CONFIGS_ROOT =	/home/danny/src/github/makeEspArduino/tools
__TOOLS_DIR = ${HOME}/src/github/makeEspArduino/tools

UPLOAD_SPEED = 921600
BUILD_ROOT = buildroot
BOARD = d1_mini
# BOARD = nodemcu
OTA_ADDR = OTA-measure.local

ESP_ARDUINO = /home/danny/src/github/esp8266/esp8266-2.7.4/libraries
LIBS =	../libraries \
	${ESP_ARDUINO}/ESP8266WebServer \
	${HOME}/src/github/ina3221

SRC_FILE = measure.cpp
