/*
 * user_config.h
 *
 */

#ifndef USER_CONFIG_H_
#define USER_CONFIG_H_

#define WLAN_SSID		"myssid"
#define WLAN_PASSWORD	"wlan_password"

#define MQTT_HOST     "mqtt_fqdn_or_ip" //or "mqtt.yourdomain.com"
#define MQTT_PORT     1883

#define MQTT_CLIENT_ID	"mydevice"
#define MQTT_USER		"mqtt_user"
#define MQTT_PASS		"mqtt_pass"

#define STATUS_CMD		"* 0 Lamp ?"						// serial command to poll for status
#define MQTT_TOPIC_CMD	"topic/device/cmd"			// subscribes for commands
#define MQTT_TOPIC_STATUS "/topic/device/status"	// publishes current device state (on/off)
#define MQTT_TOPIC_RAW	"/topic/device/raw"			// publishes all received data

// See MQTT Submodule for documentation
#define DEFAULT_SECURITY  0
#define MQTT_CLEAN_SESSION 1
#define PROTOCOL_NAMEv311
#define MQTT_RECONNECT_TIMEOUT 5 /*second*/
#define MQTT_KEEPALIVE    120  /*second*/
#define MQTT_BUF_SIZE 1024
#define QUEUE_BUFFER_SIZE       2048

// define DEBUG_ON to see MQTT and this modules debug messages
#ifndef INFO
#if defined(DEBUG_ON)
#define INFO( format, ... ) os_printf( format, ## __VA_ARGS__ )
#else
#define INFO( format, ... )
#endif
#endif

#endif /* USER_CONFIG_H_ */
