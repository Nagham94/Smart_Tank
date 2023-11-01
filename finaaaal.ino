#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <LiquidCrystal.h>
#include <WiFiManager.h>

LiquidCrystal lcd(19, 23, 18, 17, 16, 15);
Servo myservo;  // servo object
Servo rainServo;
Servo flameServo;
#define TRIGGER_PIN 32  //Configuration button
int timeout = 120;

//WiFi
const char *ssid = "Nagham";
const char *password = "12344321";
//MQTT Broker
const char *mqtt_broker = "broker.emqx.io";            // public broker
const char *topic_publish = "emqx/esp32/publish";      // topic esp publish
const char *topic_publish2 = "emqx/esp32/rain";        // topic esp publish
const char *topic_publish3 = "emqx/esp32/flame";       // topic esp publish
const char *topic_subscribe = "emqx/esp32/subscribe";  // topic esp subscribe
const char *mqtt_username = "water";
const char *mqtt_password = "level";
const int mqtt_port = 1883;

WiFiClient espClient;  //WiFi Object
PubSubClient Client(espClient);

#define PIN_TRIG 27
#define PIN_ECHO 26
#define buzzer 12
#define DO_PIN 13  // ESP32's pin GPIO13 connected to DO pin of the rain sensor
#define AO_PIN 36  // ESP32's pin GPIO36 connected to AO pin of the rain sensor
#define flame 4

//The tank maximum hight in cm
float tank_height = 30;

//The indication levels
float Very_Low = (tank_height * 80) / 100;     // 320 ----> 20%
float Low = (tank_height * 60) / 100;          // 240 ----> 40%
float Halfway = (tank_height * 50) / 100;      // 200 ----> 50%
float Almost_full = (tank_height * 40) / 100;  // 160 ----> 60%
float Full = (tank_height * 20) / 100;         // 80 ----> 80%

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(flame, INPUT);
  //wifi_setup(); 
  Serial.println("After connect MQTT");
  myservo.attach(25);
  rainServo.attach(22);
  flameServo.attach(33);
  Client.subscribe(topic_subscribe);
  Client.setCallback(callback);
  // initialize the Arduino's pin as an input
  pinMode(DO_PIN, INPUT);
  lcd.begin(16, 2);
}

void loop() {
  Client.loop();
  if (digitalRead(TRIGGER_PIN) == LOW) {
    WiFiManager wm;

    //reset settings - for testing
    //wm.resetSettings();

    // set configportal timeout
    wm.setConfigPortalTimeout(timeout);

    if (!wm.startConfigPortal("OnDemandAP")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(5000);
    }

    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  }
  // Start a new measurement:
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  water_level();
  rainSensor();
  flameSensor();
  delay(1000);
  lcd.clear();
}

void callback(char *topic, byte *message, unsigned int length) {
  printPayload(topic, message, length);
}

void printPayload(char *topic, byte *message, unsigned int length) {
  //printing recieved message
  Serial.print("Message recieved on topic: ");
  Serial.print(topic);
  String messageTemp;
  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }
  Serial.println(messageTemp);
  if (String(topic) == "emqx/esp32/subscribe") {  /// esp32 subscribe topic //0,50,100
    int status = messageTemp.toInt();
    //int pos = map(status, 1, 100, 0, 180);//100
    Serial.println(status);
    myservo.write(status);
    delay(15);
  }
}

void wifi_setup() {
  //Connecting to WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
  //utilize pubsubClient to establish a connection with the MQTT broker
  //connecting to a MQTT broker
  Serial.println("Before connect to MQTT");
  //broker configuration
  Client.setServer(mqtt_broker, mqtt_port);
  while (!Client.connected()) {
    String client_id = "esp32.client";
    client_id += String(WiFi.macAddress());
    // esp32.id -----> esp32.client.macaddress
    //variable.C_str() -----> char [] = string
    Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (Client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Public MQTT broker connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(Client.state());
      delay(2000);
    }
  }
}

void water_level() {
  // Read the result:
  int duration = pulseIn(PIN_ECHO, HIGH);
  Serial.print("Distance in CM: ");
  Serial.println(duration / 58);
  int distance = duration / 58;
  double percentage = ((tank_height - distance) * 100.0) / tank_height;
  if (0 < percentage < 100) {
    char payload[10];
    sprintf(payload, "%f", percentage);
    Client.publish(topic_publish, payload);
  }
  if(0 < percentage < 10){
    myservo.write(180);
    delay(15);
  }
  else if(90 < percentage < 100){
    myservo.write(0);
    delay(15);
  }

  if (distance >= tank_height) {
    delay(1000);
  } else if (Very_Low <= distance) {
    Serial.println("Very low level");
    Serial.print("Percentage is: ");
    Serial.print(percentage);
    Serial.println("%");
    lcd.setCursor(0, 0);
    lcd.print("Very low level");
    for (int i = 0; i < 3; i++) {
      digitalWrite(buzzer, HIGH);  // Turn on the buzzer
      delay(500);
      digitalWrite(buzzer, LOW);  // Turn off the buzzer
      delay(50);
    }

  } else if (Low <= distance && Very_Low > distance) {

    Serial.println("low level");
    Serial.print("Percentage is: ");
    Serial.print(percentage);
    Serial.println("%");
    lcd.setCursor(0, 0);
    lcd.print("low level");
    delay(1000);
  } else if (Halfway <= distance && Low > distance) {

    Serial.println("Halfway");
    Serial.print("Percentage is: ");
    Serial.print(percentage);
    Serial.println("%");
    lcd.setCursor(0, 0);
    lcd.print("Halfway");
    delay(1000);
  } else if (Almost_full <= distance && Halfway > distance) {

    Serial.println("High level");
    Serial.print("Percentage is: ");
    Serial.print(percentage);
    Serial.println("%");
    lcd.setCursor(0, 0);
    lcd.print("High level");
    delay(1000);
  } else if (Full <= distance && Almost_full > distance) {

    Serial.println("Very High level");
    Serial.print("Percentage is: ");
    Serial.print(percentage);
    Serial.println("%");
    lcd.setCursor(0, 0);
    lcd.print("Very High level");
    delay(1000);
  } else if (Full >= distance) {

    Serial.println("Tank is Full");
    Serial.print("Percentage is: ");
    Serial.print(percentage);
    Serial.println("%");
    lcd.setCursor(0, 0);
    lcd.print("Tank is Full");
    for (int i = 0; i < 3; i++) {
      digitalWrite(buzzer, HIGH);  // Turn on the buzzer
      delay(500);
      digitalWrite(buzzer, LOW);  // Turn off the buzzer
      delay(50);
    }
  }
}

void rainSensor() {
  int rain_state = digitalRead(DO_PIN);
  if (rain_state == HIGH) {
    Serial.println("The rain is NOT detected");
    rainServo.write(0);
    Client.publish(topic_publish2, "There is no rain detected");
    delay(15);
  } else {
    Serial.println("The rain is detected");
    rainServo.write(180);
    Client.publish(topic_publish2, "Rain is detected");
    delay(15);
  }
  int rain_value = analogRead(AO_PIN);
  Serial.println(rain_value);  // print out the analog value
}

void flameSensor() {
  int output = digitalRead(flame);
  Client.publish(topic_publish3, "NO FIRE DETECTED");
  flameServo.write(0);
  delay(15);
  if (output == 1) {
    Serial.print("FIRE DETECTED!");
    Client.publish(topic_publish3, "FIRE DETECTED!");
    flameServo.write(180);
    delay(15);
  }
}
