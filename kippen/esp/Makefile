#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := kippen

IDF_PATH = /home/danny/src/github/esp32/esp-idf-v3.3.1
# IDF_PATH = /home/danny/src/github/esp32/esp-idf-v3.3.2

include $(IDF_PATH)/make/project.mk

ota:
	scp build/keypad.bin pi3:/var/www/html/esp/keypad.bin
	mosquitto_pub -h pi3 -t /alarm/node/hall -m ota://192.168.0.141/esp/keypad.bin

pushkeys: 
	adb push build/main/my-ca.crt /storage/emulated/0/Android/data/info.backx.danny.alarm.alarm.remotecontroller/files/ca.pem
	-rm client.store
	# keytool -keystore client.store -delete
	keytool -keystore client.store -storepass ${KEYTOOL_PASS} -importcert -noprompt -trustcacerts -file build/main/client2.crt
	adb push client.store /storage/emulated/0/Android/data/info.backx.danny.alarm.alarm.remotecontroller/files

list:
	adb shell ls -l /storage/emulated/0/Android/data/info.backx.danny.alarm.alarm.remotecontroller/files
