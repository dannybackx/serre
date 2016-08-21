/*
 *
 */

extern char *personal_timezone;
extern const char *mqtt_server;
extern const int mqtt_port;

extern const char *mqtt_topic_serre;
extern const char *mqtt_topic_bmp180;
extern const char *mqtt_topic_valve;
extern const char *mqtt_topic_valve_start;
extern const char *mqtt_topic_valve_stop;
extern const char *mqtt_topic_report;
extern const char *mqtt_topic_report_freq;
extern const char *mqtt_topic_reconnects;
extern const char *mqtt_topic_restart;
extern const char *mqtt_topic_boot_time;
extern const char *mqtt_topic_schedule;

// Pin definitions
extern const int	valve_pin;

extern const char *startup_text;

extern int verbose;
extern char *watering_schedule_string;

#define	VERBOSE_OTA		0x01
#define	VERBOSE_VALVE		0x02
#define	VERBOSE_BMP		0x04
#define	VERBOSE_CALLBACK	0x08
#define	VERBOSE_SYSTEM		0x10
#define	VERBOSE_6		0x20
#define	VERBOSE_7		0x40
#define	VERBOSE_8		0x80

