#define PRJCONF_NET_EN 1
#define PRJCONF_CONSOLE_EN 0

#define GPIO_PORT GPIO_PORT_A
#define GPIO_MODE GPIOx_Pn_F6_EINT
#define MQTT_BUFFER 512
#define UNTUYA_VERSION "0.2"

#include <string.h>
#include "common/framework/platform_init.h"
#include "net/wlan/wlan.h"
#include "common/framework/net_ctrl.h"
#include "pm/pm.h"
#include "driver/chip/hal_wakeup.h"
#include "kernel/os/os.h"
#include "driver/chip/hal_gpio.h"
#include "net/mqtt/MQTTClient-C/MQTTClient.h"

static Client client;
static Network network;
static MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

char *WIFI_SSID = "your_wifi_ssid\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";    /* set your AP's ssid */
char *WIFI_PASS = "your_wifi_password\0\0\0\0\0\0\0\0\0\0\0\0\0\0"; /* set your AP's password */
char *MQTT_SERVER = "your_mqtt_broker_host\0\0\0\0\0\0\0\0\0\0\0";   /* set your mqtt brocker ip */
char *MQTT_PORT = "your_mqtt_broker_tcp_port\0\0\0\0\0\0\0";			/* set your mqtt brocker tcp port (default is 1883)*/
char *MQTT_IDENTIFIER = "unique_device_name\0\0\0\0\0\0\0\0\0\0\0\0\0\0";    /* set a unique device name */
char *MQTT_USER = "your_mqtt_user\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";    /* set your mqtt user (set it empty to use no user/password) */
char *MQTT_PASS = "your_mqtt_pass\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"; /* set your mqtt password */
char *GPIO_WAKEUP_IO = "gpio_wakeup_io\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"; /* wakeup io (determinates also the input pin) */
char *GPIO_PIN_PULL = "gpio_pin_pull\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"; /* gpio pull resistor setting */
char *TIME_WAKEUP = "time_wakeup_sec\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"; /* wakeup timer in seconds */
uint8_t GPIO_PIN = 19; 
uint32_t TIME_WAKEUP_AS_INT = 60; 
uint8_t GPIO_WAKEUP_IO_AS_INT = 6; 

static GPIO_PinState gpio_value()
{
	return HAL_GPIO_ReadPin(GPIO_PORT, GPIO_PIN);
}

static void shutdown_cb()
{
	if(client.isconnected)
	{
		MQTTDisconnect(&client);
		OS_MSleep(50); //Give some time to send/receive tcp acknowledges
	}
	
	network.disconnect(&network);
	OS_MSleep(50); //Give some time to send/receive tcp acknowledges
	wlan_sta_disable();
	HAL_Wakeup_SetTimer_Sec(TIME_WAKEUP_AS_INT); //wakeup on an interval
	uint32_t edge_direction = gpio_value() == GPIO_PIN_LOW;
	
	HAL_Wakeup_SetIO(GPIO_WAKEUP_IO_AS_INT, edge_direction, 1); //wakeup on input

	pm_enter_mode(PM_MODE_POWEROFF); //Shutdown soc (wake up and restart by rtc or input)
}

static void publishHaConfig()
{
	MQTTMessage message;

	char *expire_conf = (char *) malloc(6 * sizeof(char));
	itoa(TIME_WAKEUP_AS_INT + 10, expire_conf, 10); //Tell homeassistent that this device state expires 10s after the regular wakeup interval

	char *haConfigStr = (char *) malloc(400 * sizeof(char));
	strcpy(haConfigStr, "{\"name\":\"");
	strcat(haConfigStr, MQTT_IDENTIFIER);
	strcat(haConfigStr, "\",\"unique_id\":\"");
	strcat(haConfigStr, MQTT_IDENTIFIER);
	strcat(haConfigStr, "\",\"device\":{\"identifiers\":\"");
	strcat(haConfigStr, MQTT_IDENTIFIER);
	strcat(haConfigStr, "\",\"manufacturer\":\"untuya\",\"model\":\"WIFI Door/Window Sensor\",\"name\":\"");
	strcat(haConfigStr, MQTT_IDENTIFIER);
	strcat(haConfigStr, "\",\"sw_version\":\"" UNTUYA_VERSION "\"},\"device_class\":\"window\",\"expire_after\":");
	strcat(haConfigStr, expire_conf);
	strcat(haConfigStr, ",\"state_topic\":\"tele/");
	strcat(haConfigStr, MQTT_IDENTIFIER);
	strcat(haConfigStr, "/state\"}");
	
	message.qos = 0;
	message.retained = 1;
	message.payload = haConfigStr;
	message.payloadlen = strlen(message.payload);

	char *haConfigTopic = (char *) malloc(70 * sizeof(char));
	strcpy(haConfigTopic, "homeassistant/binary_sensor/");
	strcat(haConfigTopic, MQTT_IDENTIFIER);
	strcat(haConfigTopic, "/config");

	MQTTPublish(&client, haConfigTopic, &message);
}

static void publishGpioState(GPIO_PinState state)
{
	MQTTMessage message;

	message.qos = 0;
	message.retained = 1;
	if(state == GPIO_PIN_LOW){
		message.payload = "OFF";
	}else{
		message.payload = "ON";
	}
	message.payloadlen = strlen(message.payload);

	char *stateTopic = (char *) malloc(50 * sizeof(char));
	strcpy(stateTopic, "tele/");
	strcat(stateTopic, MQTT_IDENTIFIER);
	strcat(stateTopic, "/state");

	MQTTPublish(&client, stateTopic, &message);
}

int main(void)
{	
	//setup the os
	platform_init();

	/* set ssid and password to wlan */
	wlan_sta_set((uint8_t *)WIFI_SSID, strlen(WIFI_SSID), (uint8_t *)WIFI_PASS);

	/* Parse "config" */
	TIME_WAKEUP_AS_INT = atoi(TIME_WAKEUP);
	GPIO_WAKEUP_IO_AS_INT = atoi(GPIO_WAKEUP_IO);

	/*set pin driver capability*/
	GPIO_InitParam param;
	param.driving = GPIO_DRIVING_LEVEL_1;

    //Mapping taken from WAKEUP_IO enum (hal_gpio.h)
	 
	switch (GPIO_WAKEUP_IO_AS_INT)
	{
	case 0:
		GPIO_PIN = WAKEUP_IO0;
		break;
	case 1:
		GPIO_PIN = WAKEUP_IO1;
		break;
	case 2:
		GPIO_PIN = WAKEUP_IO2;
		break;
	case 3:
		GPIO_PIN = WAKEUP_IO3;
		break;
	case 4:
		GPIO_PIN = WAKEUP_IO4;
		break;
	case 5:
		GPIO_PIN = WAKEUP_IO5;
		break;
	case 6:
		GPIO_PIN = WAKEUP_IO6;
		break;
	case 7:
		GPIO_PIN = WAKEUP_IO7;
		break;
	case 8:
		GPIO_PIN = WAKEUP_IO8;
		break;
	case 9:
		GPIO_PIN = WAKEUP_IO9;
		break;
	}

	switch(atoi(GPIO_PIN_PULL))
	{
	case 0:
		param.pull = GPIO_PULL_NONE;
		break;
	case 1:
		param.pull = GPIO_PULL_DOWN;
		break;
	case 2:
		param.pull = GPIO_PULL_UP;
		break;
	}

	param.mode = GPIO_MODE;
	HAL_GPIO_Init(GPIO_PORT, GPIO_PIN, &param);

	wlan_sta_enable();

	uint8_t *send_buf;
	uint8_t *recv_buf;

	send_buf = malloc(MQTT_BUFFER);
	if (send_buf == NULL) {
		shutdown_cb();
		return 0;
	}
	recv_buf = malloc(MQTT_BUFFER);
	if (recv_buf == NULL) {
		shutdown_cb();
		return 0;
	}

	NewNetwork(&network);
	MQTTClient(&client, &network, 1000, send_buf, MQTT_BUFFER, recv_buf, MQTT_BUFFER);

	char *clientId = (char *) malloc(40 * sizeof(char));
	strcpy(clientId, "client_");
	strcat(clientId, MQTT_IDENTIFIER);

	connectData.clientID.cstring = clientId;
	connectData.keepAliveInterval = 0;
	connectData.cleansession = 1;

	if (strlen(MQTT_USER) > 0)
	{
		connectData.username.lenstring.data = MQTT_USER;
		connectData.username.lenstring.len = strlen(connectData.username.lenstring.data);

		connectData.password.lenstring.data = MQTT_PASS;
		connectData.password.lenstring.len = strlen(connectData.password.lenstring.data);
	}

	uint32_t i = 3;
	while (ConnectNetwork(&network, MQTT_SERVER, atoi(MQTT_PORT)) != 0)
	{
		OS_Sleep(2);
		if (!(i--)){
			shutdown_cb();
			return 0;
		}
	}

	if (MQTTConnect(&client, &connectData) != 0) {
		shutdown_cb();
		return 0;
	}

	publishHaConfig();

	publishGpioState(gpio_value());

	shutdown_cb();

	return 0;
}
