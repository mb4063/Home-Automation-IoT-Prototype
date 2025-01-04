#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <DHT22.h>
#include <IRRemoteESP32.h>
#include <IRsend.h>
#include <ir_Gree.h>

//define DHT22 pin and ledPIN
#define DHTpin 21
#define ledPIN 27

DHT22 dht22(DHTpin);

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID ""
#define WIFI_PASSWORD ""


// Insert Firebase project API Key (this project is public so we dont need it)
#define API_KEY ""

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "" 

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

// Variables
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;
bool lamp = false;
bool acOnOff = false;
const uint16_t IRLed = 14;
IRGreeAC ac(IRLed);
uint8_t temp = 25;

// TODO: This must change with interrupt
bool isAcChanged = false;
String acMode = "";
String acStatus = "";


void writeToDB(String room, float temp, float humidity) {
  room.toLowerCase();
  String path = "";
  if (room == "livingroom" || room == "living room") {
      path = "rooms/Living Room";
  } else if(room == "bedroom") {
      path = "rooms/Bedroom";
  } else if (room == "kitchen") {
      path = "rooms/Kitchen";
  } else {
    Serial.print(room + " does not exists.");
  }
      

  
  // Write an Int number on the database path test/int
  if (Firebase.RTDB.setFloat(&fbdo, path + "/realTemperature", temp)){
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  }
  else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }

  
  // Write an Float number on the database path test/float
  if (Firebase.RTDB.setFloat(&fbdo, path + "/realHumidity", humidity)){
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  }
  else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }


}

void readFromDB(){
  String newMode = "";
  String newStatus = "";
  String value = "";
  int newTemp = 0;
  if (Firebase.RTDB.getString(&fbdo, "/rooms/Living Room/lamp")) {
      if (fbdo.dataType() == "string") {
        value = fbdo.stringData();
        value.toLowerCase();
        Serial.println(value);
      }
    }

    if (Firebase.RTDB.getString(&fbdo, "/rooms/Living Room/ACMode")) {
      if (fbdo.dataType() == "string") {
        newMode = fbdo.stringData();
        newMode.toLowerCase();
        Serial.println(newMode);
        isAcChanged = (acMode != newMode);
      }
    }

    if (Firebase.RTDB.getString(&fbdo, "/rooms/Living Room/airConditioner")) {
      if (fbdo.dataType() == "string") {
        newStatus = fbdo.stringData();
        newStatus.toLowerCase();
        Serial.println(newStatus);

        isAcChanged = (acStatus != newStatus);
      }
    }

    if (Firebase.RTDB.getInt(&fbdo, "/rooms/Living Room/reqTemperature")) {
      if (fbdo.dataType() == "int") {
        newTemp = fbdo.intData();
        Serial.println(newTemp);
        isAcChanged = (newTemp != temp);
      }
    }

    // Initialization of corresponding values
    lamp = (value == "on") ? true : false;
    if(isAcChanged) {
      acOnOff = (acStatus == "on") ? true : false;
      (acMode == "heat") ? ac.setMode(kGreeHeat) : (acMode == "cool") ? ac.setMode(kGreeCool) : ac.setMode(kGreeDry);
    }
     
}

void execute() {
  digitalWrite(ledPIN, lamp);

  // Execute if AC values changed
  if(isAcChanged) {
    ac.setTemp(temp);
    (acOnOff) ? ac.on() : ac.off();
    ac.send();
  }
  
}

void printState() {
  // Display the settings.
  Serial.println("GREE A/C remote is in the following state:");
  Serial.printf("  %s\n", ac.toString().c_str());
  // Display the encoded IR sequence.
  unsigned char* ir_code = ac.getRaw();
  Serial.print("IR Code: 0x");
  for (uint8_t i = 0; i < kGreeStateLength; i++)
    Serial.printf("%02X", ir_code[i]);
  Serial.println();
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(ledPIN, OUTPUT);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  ac.begin();
  ac.setFan(0);
  ac.setTemp(temp);
  ac.setSwingVertical(false, kGreeSwingAuto);
  ac.setXFan(false);
  ac.setLight(true);
  ac.setSleep(false);
  ac.setTurbo(false);

}

void loop() {
    // Read temperature and humidity values
    float temp = dht22.getTemperature();
    float humidity = dht22.getHumidity();

    // Access firebase per second
    if(Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 100 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();

    // Write reading data to RTDB
    Serial.println("livingroom ");
    writeToDB("livingroom", temp*1.02, humidity*1.03);
    Serial.println("kitchen ");
    writeToDB("kitchen", temp*1.05, humidity*0.92);
    Serial.println("bedroom ");
    writeToDB("bedroom", temp*1.07, humidity);
    Serial.println("reading lamp");

    // Read from RTDB
    readFromDB();

    // Executing lamp and ac commands
    execute();
    ac.send();
    // Printing ac state
    printState();

    
  }
}


