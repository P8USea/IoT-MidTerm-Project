#include "secrets/wifi.h"
#include "wifi_connect.h"
#include <WiFiClientSecure.h>
#include "ca_cert.h"

#include "secrets/mqtt.h"
#include <PubSubClient.h>

#include <Ticker.h>


void sensorHandler();
void doMotorStuff();
namespace
{
    const char *ssid = WiFiSecrets::ssid;
    const char *password = WiFiSecrets::pass;
    const char *moisture_topic = "sensor/moisture";
    const char *motor_topic = "motor";
    unsigned int publish_count = 0;
    uint16_t keepAlive = 15;    // seconds (default is 15)
    uint16_t socketTimeout = 5; // seconds (default is 15)
    int willQoS = 0;

    int sensor_pin = 33;
    int motor_pin = 18;
    int data = 0;

}

WiFiClientSecure tlsClient;
PubSubClient mqttClient(tlsClient);

Ticker mqttPulishTicker;

void mqttPublish()
{
    mqttClient.publish(moisture_topic, String(data).c_str(), false);
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.printf("Message arrived from topic [%s]: %s\n", topic, message.c_str());


    // if(String(topic) == "sensor/moisture"){
    //     sensorHandler();
    // }

    if (String(topic) == "motor") {
        if(message == "ON"){
            doMotorStuff();
            Serial.println("Handy Watering...");

        }
    }

}

void mqttReconnect()
{
    while (!mqttClient.connected())
    {
        Serial.println("Attempting MQTT connection...");
        String client_id = "esp32-client-";
        client_id += String(WiFi.macAddress());
        if (mqttClient.connect(client_id.c_str(), MQTT::username, MQTT::password))
        {
            Serial.print(client_id);
            Serial.println(" connected");
            mqttClient.subscribe(moisture_topic);
            mqttClient.subscribe(motor_topic);

        }
        else
        {
            Serial.print("MTTT connect failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 1 seconds");
            delay(1000);
        }
    }
}

int reliability_count = 0;
int moisture_threshold = 70;
int noise_threshold = 3;
int sensor_last = 0;
void sensorHandler(){
    int sensor_now = data;
    Serial.println(reliability_count);

    if(reliability_count > 10){
        if(data < moisture_threshold){
            doMotorStuff();
            Serial.println("Watering...");
        }
        reliability_count = 0;
    }
    else{
        if(abs(sensor_last - sensor_now) < noise_threshold){
            reliability_count++;
        }
        sensor_last = sensor_now;
    }
    
}
int enA = 25;
int in1 = 26;
int in2 = 27;
int speed = 192;
void doMotorStuff(){
    mqttClient.publish(motor_topic, "Watering...", false);
    
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    analogWrite(enA, speed);

    delay(3000);

    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    analogWrite(enA, 0);

    delay(1000);

}


void setup()
{
    Serial.begin(115200);
    delay(10);
    setup_wifi(ssid, password);
    tlsClient.setCACert(ca_cert);
    // mqttClient.setKeepAlive(keepAlive); // To see how long mqttClient detects the TCP connection is lost
    // mqttClient.setSocketTimeout(socketTimeout); // To see how long mqttClient detects the TCP connection is lost

    mqttClient.setCallback(mqttCallback);
    mqttClient.setServer(MQTT::broker, MQTT::port);
    mqttPulishTicker.attach(1, mqttPublish);
}

void loop()
{
    int value = analogRead(sensor_pin);
    data = map(value, 0, 4095, 0, 100);
    delay(20);

    
    if (!mqttClient.connected())
    {
        mqttReconnect();
    }
    mqttClient.loop();
}