/*
 *
 */

const char *personal_timezone = "CET+1:00:00CETDST+2:00:00,M3.5.0,M10.5.0";

const char *mqtt_server = "192.168.1.185";
const int mqtt_port = 1883;

const char *mqtt_topic_serre =		"Serre";
const char *mqtt_topic_bmp180 =		"Serre/BMP180";
const char *mqtt_topic_valve =		"Serre/Valve";
const char *mqtt_topic_valve_start =	"Serre/Valve/1";
const char *mqtt_topic_valve_stop =	"Serre/Valve/0";
const char *mqtt_topic_report =		"Serre/Report";
const char *mqtt_topic_report_freq =	"Serre/Report/Frequency";
const char *mqtt_topic_restart =	"Serre/System/Reboot";
const char *mqtt_topic_reconnects =	"Serre/System/Reconnects";
const char *mqtt_topic_boot_time =	"Serre/System/Boot";
const char *mqtt_topic_schedule_set =	"Serre/Schedule/Set";
const char *mqtt_topic_schedule =	"Serre/Schedule";
const char *mqtt_topic_verbose =	"Serre/System/Verbose";

// Pin definitions
const int	valve_pin = 0;

const char *startup_text = "\n\nMQTT Greenhouse Watering and Sensor Module\n"
	"(c) 2016 by Danny Backx\n\n";

int verbose = 0;

/*
 * Schedule for watering
 *	format is CSV
 *	hour,onoff
 */
// char *watering_schedule_string = "21:00,1,21:15,0";
char *watering_schedule_string = "16:27,1,16:28,0";