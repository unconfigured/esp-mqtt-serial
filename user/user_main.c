#include "ets_sys.h"
#include <osapi.h>
#include "os_type.h"
#include "mqtt.h"
#include "user_config.h"
#include <user_interface.h>
#include <at_custom.h>
#include <driver/uart.h>
#include <driver/uart_register.h>

const char ssid[32] = WLAN_SSID;
const char password[64] = WLAN_PASSWORD;
char send_msg_buff[256];

os_timer_t start_timer;			// Timer for main loop
struct station_config stationConf;	// Wifi configuration
MQTT_Client mqttClient;			// MQTT Client
uint8_t conn_status = STATION_IDLE;	// last connection status

//UartDev is defined and initialized in rom code.
extern UartDevice UartDev;

enum {
  DEVICE_STATUS_UNK = 0,
  DEVICE_STATUS_OFF,
  DEVICE_STATUS_ON
};

int beamer_status = DEVICE_STATUS_UNK;

  
// MQTT connected
static void ICACHE_FLASH_ATTR mqtt_connected_cb(uint32_t *args) {
  MQTT_Client* client = (MQTT_Client*)args;

  INFO("MQTT Connected, subscribing to channels");
  MQTT_Subscribe(client, MQTT_TOPIC_CMD, 0);
}

// MQTT disconnected
static void ICACHE_FLASH_ATTR mqtt_disconnected_cb(uint32_t *args)
{
  MQTT_Client* client = (MQTT_Client*)args;

  INFO("MQTT: Disconnected\n");
}

// Received data from broker
static void ICACHE_FLASH_ATTR mqtt_data_cb(
   uint32_t *args, const char* topic, uint32_t topic_len,
   const char *data, uint32_t data_len) {

  if (strncmp(MQTT_TOPIC_CMD, topic, topic_len) == 0
      && data_len < sizeof(send_msg_buff)) {
    if (strncmp("ON", data, data_len) == 0) {
	  os_sprintf(send_msg_buff, "%s", "* 0 IR 001");
    } else if (strncmp("OFF", data, data_len) == 0) {
	  os_sprintf(send_msg_buff, "%s", "* 0 IR 002");
    } else
	  // send as is ;)
	  os_sprintf(send_msg_buff, "%s", data);
  }
}

// serial receive
//void __attribute__((section(".iram0.text"))) uart_rx(void *para) {
void uart_rx(void *para) {
  char RcvStr[256];
  char buf[4];
  int i;
  int len;
  uint32 intr_status;

  intr_status = READ_PERI_REG(UART_INT_ST(UART0));
  //DECIDE THE INTERRUPT SOURCE
  if ( (intr_status & UART_RXFIFO_FULL_INT_ST)
           == UART_RXFIFO_FULL_INT_ST  ||
	   (intr_status & UART_RXFIFO_TOUT_INT_ST)
           == UART_RXFIFO_TOUT_INT_ST ) {
    
	len  = (READ_PERI_REG(UART_STATUS(UART0)) >> UART_RXFIFO_CNT_S);
	len &= UART_RXFIFO_CNT;

    for (i=0; (i < len && i < sizeof(RcvStr)); i++) {
      RcvStr[i]= READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
	  if (RcvStr[i] == '\0' ||
	      RcvStr[i] == '\r' ||
		  RcvStr[i] == '\n') {
	    break;
	  }
    }
    RcvStr[i]='\0';

    // Publish received message to (debug) raw queue
    INFO("%s\n", RcvStr);
	MQTT_Publish(&mqttClient, MQTT_TOPIC_RAW, RcvStr, i, 0, 0);

	// Pubish status
	if (strncmp(RcvStr, "LAMP ", 5) == 0 ||
	    strncmp(RcvStr, "Lamp ", 5) == 0) {
	  os_sprintf(buf, "%s", (RcvStr[5] == '0') ? "OFF" : "ON");

	  MQTT_Publish(&mqttClient, MQTT_TOPIC_STATUS,
	    buf, strlen(buf), 0, 0);
    }

	// Reset command to status command
	os_sprintf(send_msg_buff, "%s", STATUS_CMD);

    //AT LAST CLEAR INTERRUPT STATUS
    WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);
    WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_TOUT_INT_CLR);
  }
}


// loop function
void ICACHE_FLASH_ATTR timer_cb(void *arg) {
  struct ip_info ipconfig;
  int status;
  int c;
  char recv_buf[30];

  // Stop timer
  os_timer_disarm(&start_timer);

  // get information on current connection
  wifi_get_ip_info(STATION_IF, &ipconfig);
  status = wifi_station_get_connect_status();

  // ensure that wifi is (still) ready
  if (status != STATION_GOT_IP || ipconfig.ip.addr == 0) {
    INFO("Still trying to connect... (%d)\n", status);
    // We might have lost our connect, tell mqtt it's disconnected
    if (status != conn_status) {
      INFO("Wifi status changed, disconning mqtt (%d -> %d)\n",
        conn_status, status);
      MQTT_Disconnect(&mqttClient);
      conn_status = status;
    }
    goto end;
  }

  // connect to MQTT, if we're just become online
  if (status != conn_status) {
    INFO("Status changed to online, connecting mqtt\n");
    MQTT_Connect(&mqttClient);
    conn_status = status;
	MQTT_Publish(&mqttClient, MQTT_TOPIC_RAW,
      ">> now online", 13, 0, 0);
  }

  // send whatever is in buffer
  os_printf("%s\n", send_msg_buff);
  // Reset to status command
  os_sprintf(send_msg_buff, "%s", STATUS_CMD);

  end:
    // reenable timer
    os_timer_setfn(&start_timer, timer_cb, NULL);
    os_timer_arm(&start_timer, 2000, 1);
}

// initialization (after system is up and wifi established)
void ICACHE_FLASH_ATTR sdk_init_done_cb(void) {
  struct station_info *statinfo;

  // Fill struct with Connection parameters
  MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, 
    DEFAULT_SECURITY);
  // Initialize mqtt
  MQTT_InitClient(&mqttClient, MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, 
    MQTT_KEEPALIVE, MQTT_CLEAN_SESSION);
  // register callbacks
  MQTT_OnConnected(&mqttClient, mqtt_connected_cb);
  MQTT_OnDisconnected(&mqttClient, mqtt_disconnected_cb);
  MQTT_OnData(&mqttClient, mqtt_data_cb);

  // set main loop
  os_timer_disarm(&start_timer);
  os_timer_setfn(&start_timer, timer_cb, NULL);
  os_timer_arm(&start_timer, 2000, 1);
}

void ICACHE_FLASH_ATTR user_init() {

  // initialize status string
  os_sprintf(send_msg_buff, "%s", STATUS_CMD);

  // disable serial interrupts
  ETS_UART_INTR_DISABLE();
  // set baud rate
  uart_div_modify(UART0, UART_CLK_FREQ / 9600);
  //clear rx and tx fifo,not ready
  SET_PERI_REG_MASK(UART_CONF0(UART0), UART_RXFIFO_RST | UART_TXFIFO_RST);
  CLEAR_PERI_REG_MASK(UART_CONF0(UART0), UART_RXFIFO_RST | UART_TXFIFO_RST);
  // set parameters on interrupt
  WRITE_PERI_REG(UART_CONF1(UART0),
	// UART RX FULL THRESHOLD
    ((100 & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S) |
	// UART RX TIMEOUT THRESHOLD
	((0x02 & UART_RX_TOUT_THRHD) << UART_RX_TOUT_THRHD_S) |
	UART_RX_TOUT_EN
  );
  // clear all interrupts
  WRITE_PERI_REG(UART_INT_CLR(UART0), 0xffff);
  // set enabled interrupts
  SET_PERI_REG_MASK(
    UART_INT_ENA(UART0),
	UART_RXFIFO_FULL_INT_ENA | UART_RXFIFO_TOUT_INT_ENA);
  // register interrupt handler
  ETS_UART_INTR_ATTACH(uart_rx,  NULL);
  // Enable interrupts
  ETS_UART_INTR_ENABLE();

  // setup wlan
  wifi_set_opmode(STATION_MODE);
  os_memset(&stationConf, 0, sizeof(struct station_config));
  os_memcpy(&stationConf.ssid, ssid, sizeof(ssid));
  os_memcpy(&stationConf.password, password, sizeof(ssid));
  wifi_station_set_config(&stationConf);

  system_init_done_cb(sdk_init_done_cb);
}
