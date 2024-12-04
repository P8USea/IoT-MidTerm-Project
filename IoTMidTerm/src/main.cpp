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

    int sensor_pin = 33;
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

    //Xu ly thong tin cua cam bien
    if(String(topic) == "sensor/moisture"){
        sensorHandler();
    }

    //Neu nhan thong tin tuoi nuoc cua nguoi dung
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
            mqttClient.subscribe(motor_topic); //Dang ky motor_topic de nguoi dung dua ra quyet dinh tuoi nuoc

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


//MCU dua ra quyet dinh dua tren cam bien
int reliability_count = 0; //Bien dem kiem tra do on dinh cua cam bien
int moisture_threshold = 70; //Nguong am de tuoi nuoc
int noise_threshold = 3; //Nguong sai so trong truong hop nhieu tin hieu
int sensor_last = 0;
void sensorHandler(){
    int sensor_now = data;
    //Neu khong bi bien dong do nhieu trong 10 lan dem
    if(reliability_count >= 10){
        //Neu dat du nguong am de tuoi nuoc
        if(data < moisture_threshold){
            doMotorStuff();
            Serial.println("Watering...");
        }
        //Reset bien dem ve 0
        reliability_count = 0;
    }
    else{
        //Neu lan do truoc va lan do sau khong bi chenh lech qua nhieu do nhieu
        if(abs(sensor_last - sensor_now) < noise_threshold){
            //Tang bien dem tin cay
            reliability_count++;
        }
        sensor_last = sensor_now;
    }
    
}

//Ham dieu khien dong co voi xung pwm o muc 75%
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

    mqttClient.setCallback(mqttCallback);
    mqttClient.setServer(MQTT::broker, MQTT::port);
    mqttPulishTicker.attach(1, mqttPublish);
}

void loop()
{
    int value = analogRead(sensor_pin);
    data = map(value, 0, 4095, 0, 100); // map gia tri 0-12 bit ->> 0-100%
    delay(20);

    
    if (!mqttClient.connected())
    {
        mqttReconnect();
    }
    mqttClient.loop();
}