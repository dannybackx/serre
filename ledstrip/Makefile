#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := ledstrip

# IDF_PATH = /home/danny/src/github/esp32/git/3.2/esp-idf
# IDF_PATH = /home/danny/src/github/esp32/git/esp-idf-3.2.2/esp-idf
# IDF_PATH = /home/danny/src/github/esp32/esp-idf-v3.3.1
IDF_PATH = /home/danny/src/github/esp32/esp-idf-v3.3.2

include $(IDF_PATH)/make/project.mk

ota:
	scp build/ledstrip.bin pi4:/var/www/html/esp/ledstrip.bin
	mosquitto_pub -h pi4 -t /ledstrip -m ota

