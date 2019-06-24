
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>

/************ WIFI and MQTT Information (CHANGE THESE FOR YOUR SETUP) ******************/
const char* ssid = ""; //type your WIFI information inside the quotes
const char* password = "";//type your WIFI password
const char* mqtt_server = "";//type your server IP
const char* mqtt_username = "";//type your mqtt sever name
const char* mqtt_password = "";//type your mqtt sever password
const int mqtt_port = ;// type your mqtt sever port

#define PROPORTIONAL_VALUE_ANALOG_INPUT 17
#define PROPORTIONAL_VALUE_ANALOG_OUTPUT 4 //D2
#define STARTUP_PROPORTIONAL_VALUE 14 //D5
#define READ_PRESSOR_STATE 12 //D6

/**************************** FOR OTA **************************************************/
#define SENSORNAME "" //change this to whatever you want to call your device                                                                                              
#define OTApassword "" //the password you will need to enter to upload remotely via the ArduinoIDE
int OTAport = 8266;

/************* MQTT TOPICS (change these topics as you wish)  **************************/
const char* switch_state_topic = "WorkShop/AirCompressor/State";
const char* switch_set_topic = "WorkShop/AirCompressor/OnOff";
const char* brightness_state_topic = "WorkShop/AirCompressor/levelState";
const char* brightness_command_topic = "WorkShop/AirCompressor/level";
const char* on_cmd = "ON";
const char* off_cmd = "OFF";


/****************************************FOR JSON***************************************/
//const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);
//#define MQTT_MAX_PACKET_SIZE 512
float PValueSet = 0;
uint8_t CurrentValueState = 0;
uint8_t TempValueState = 6;
int PValueState = 0;

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_MCP4725 dac;

void setup() 
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  //TempValueState = 66;
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  //OTA SETUP
  ArduinoOTA.setPort(OTAport);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(SENSORNAME);

  // No authentication by default
  ArduinoOTA.setPassword((const char *)OTApassword);

  ArduinoOTA.onStart([]() {
	String type;
	if (ArduinoOTA.getCommand() == U_FLASH)
		type = "sketch";
	else // U_SPIFFS
		type = "filesystem";
    Serial.println("Starting" + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  pinMode(STARTUP_PROPORTIONAL_VALUE, OUTPUT);
  pinMode(READ_PRESSOR_STATE, INPUT);
  digitalWrite(STARTUP_PROPORTIONAL_VALUE, LOW);

  Serial.println("Ready");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  dac.begin(0x60);
  PValueSet = 0.16;
  
  dac.setVoltage(InCalcLSB(PValueSet), false);//0.16MPa Defult
}

void loop()
{
	// put your main code here, to run repeatedly:
	if (!client.connected()) {
		reconnect();
	}

	if (WiFi.status() != WL_CONNECTED) {
		delay(1);
		Serial.print("WIFI Disconnected. Attempting reconnection.");
		setup_wifi();
		return;
	}

	client.loop();

	ArduinoOTA.handle();

  while (Serial.available() > 0)
  {
    float anaout = Serial.parseFloat();
    if (Serial.read() == '\n' && anaout >= 0 && anaout <= 0.333)
    {
      int Temp = InCalcLSB(anaout);
      //dac.setVoltage(Temp, false);
      Serial.println("anaoutOK" + Temp);
    }
  }

  //CurrentValueState = digitalRead(READ_PRESSOR_STATE);
  CheckPress();
  //if (CurrentValueState != TempValueState)
  //{
  //  TempValueState = CurrentValueState;
  //  if (TempValueState == LOW)
  //  {
  //    digitalWrite(STARTUP_PROPORTIONAL_VALUE, HIGH);
  //    client.publish("WorkShop/AirCompressor/OnOff", "ON");
  //  }
  //  else if(TempValueState == HIGH)
  //  {
  //    digitalWrite(STARTUP_PROPORTIONAL_VALUE, LOW);
  //    client.publish("WorkShop/AirCompressor/OnOff", "OFF");
  //  }
  }

  //PValueState = analogRead(PROPORTIONAL_VALUE_ANALOG_INPUT);

  //Serial.println(PValueState);

/********************************** START SETUP WIFI*****************************************/
void setup_wifi() 
{

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  //Serial.print("Message arrived [");
  //Serial.print(topic);
  //Serial.print("] ");

  //String messageInfo;
  //for (int i = 0; i < length; i++) 
  //{
  //  messageInfo.concat((char)payload[i]);
  //}

  //Serial.println(messageInfo);

  char p[length + 1];
  memcpy(p, payload, length);
  p[length] = NULL;
  String message(p);
  String mytopic(topic);
 Serial.println(mytopic);
 Serial.println(message);
 
  if (mytopic.equals(String(brightness_command_topic)))
  {
    PValueSet = (message.toInt())/255.0*0.36;
    dac.setVoltage(InCalcLSB(PValueSet), false);
    Serial.println(PValueSet);
	  client.publish("WorkShop/AirCompressor/levelState", p);
  }
  else if (mytopic.equals(String(switch_set_topic)))
  {
    if(message.equals("ON"))
    {
        digitalWrite(STARTUP_PROPORTIONAL_VALUE, HIGH);
	      client.publish("WorkShop/AirCompressor/State", "ON");
    }
    else if(message.equals("OFF"))
    {
        digitalWrite(STARTUP_PROPORTIONAL_VALUE, LOW);
	      client.publish("WorkShop/AirCompressor/State", "OFF");
    }
  }
}


/********************************** START RECONNECT*****************************************/
void reconnect() 
{
	// Loop until we're reconnected
	while (!client.connected()) 
	{
		Serial.print("Attempting MQTT connection...");
		// Attempt to connect
		if (client.connect(SENSORNAME, mqtt_username, mqtt_password)) {
			Serial.println("connected");
			client.subscribe(switch_set_topic);
      client.subscribe(brightness_command_topic);
      //digitalWrite(STARTUP_PROPORTIONAL_VALUE, HIGH);
      //client.publish("WorkShop/AirCompressor/OnOff", "ON");
      CheckPress();
      client.publish("WorkShop/AirCompressor/levelState", "113");
		}
		else {
			Serial.print("failed, rc=");
			Serial.print(client.state());
			Serial.println(" try again in 5 seconds");
			// Wait 5 seconds before retrying
			delay(5000);
		}
	}
}

int CalcValue(float x)
{
  return (int)(730.0*x+164.7);
}

int InCalcLSB(float MPA)
{
  return (int)(12490*MPA-74.1);
}

void CheckPress()
{
  CurrentValueState = digitalRead(READ_PRESSOR_STATE);
  if (CurrentValueState != TempValueState)
  {
    TempValueState = CurrentValueState;
    if (TempValueState == LOW)
    {
      digitalWrite(STARTUP_PROPORTIONAL_VALUE, HIGH);
      client.publish("WorkShop/AirCompressor/OnOff", "ON");
    }
    else if(TempValueState == HIGH)
    {
      digitalWrite(STARTUP_PROPORTIONAL_VALUE, LOW);
      client.publish("WorkShop/AirCompressor/OnOff", "OFF");
    }
  }
 }

//static const uint8_t D0 = 16;
//static const uint8_t D1 = 5;
//static const uint8_t D2 = 4;
//static const uint8_t D3 = 0;
//static const uint8_t D4 = 2;
//static const uint8_t D5 = 14;
//static const uint8_t D6 = 12;
//static const uint8_t D7 = 13;
//static const uint8_t D8 = 15;
//static const uint8_t D9 = 3;
//static const uint8_t D10 = 1;
