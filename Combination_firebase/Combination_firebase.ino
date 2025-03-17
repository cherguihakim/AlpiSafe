#include <SD.h>
//External Temperature def start
#include "DHT.h"
#define DHTPIN 4
#define DHTTYPE DHT11  
DHT dht(DHTPIN, DHTTYPE);
// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 3 (on the right) of the sensor to GROUND (if your sensor has 3 pins)
// Connect pin 4 (on the right) of the sensor to GROUND and leave the pin 3 EMPTY (if your sensor has 4 pins)
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor
//External Temperature def end

//Internal temp start
#include <Wire.h>
#include "Adafruit_MCP9808.h"
// Create the MCP9808 temperature sensor object
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();
//Internal temp end

//Accelerometer start
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
Adafruit_MPU6050 mpu;
//Accelerometer end

//GPS start
#include <HardwareSerial.h>
#include <Adafruit_GPS.h>
#define GPS_TX 17  // TX pin of GPS module (connect to ESP32 RX)
#define GPS_RX 16  // RX pin of GPS module (connect to ESP32 TX)
float latDeg, longDeg, speedKMH, Altitude, gpsDate, gpsTime;
HardwareSerial GPSSerial(2);
Adafruit_GPS GPS(&GPSSerial);
#define GPSECHO false
uint32_t timer = millis();
//GPS end

//Firebase start
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Insert Firebase project API Key
#define API_KEY "AIzaSyC9IodquzD8AbE-0-MGZDm-osyQjg8pVgs"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "Alpiniste@gmail.com"
#define USER_PASSWORD "Qwerty1234567"

// Insert RTDB URLefine the RTDB URL
#define DATABASE_URL "https://alpisafe-ce734-default-rtdb.firebaseio.com/"


// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID
String uid;

// Database main path (to be updated in setup with the user UID)
String databasePath;
// Database child nodes
String extTempPath = "/ExternalTemperature";
String intTempPath = "/InternalTemperature";
String humPath = "/humidity";
String movPath = "/movement"; //bool
String timePath = "/timestamp";
String GPSdate = "/GPSdate";
String GPStime = "/GPStime";
String speedPath = "/speed";
String altPath = "/altitude";
String longDegPath = "/longitudeDegrees";
String latDegPath = "/latitudeDegrees";

// Parent Node (to be updated in every loop)
String parentPath;

FirebaseJson json;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// randoom values
int timestamp;

// Timer variables (send new readings every 10 sec)
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 10000;


// Initialize WiFi
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}

// Function that gets current epoch time
unsigned long getTime() {
  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  return now;
}
//Firebase  end

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println(F("Start"));
  dht.begin();

  //internal temperature start
  if (!tempsensor.begin(0x18)) {
    Serial.println("Couldn't find MCP9808! Check your connections and verify the address is correct.");
    while (1);
  }
  Serial.println("Found MCP9808!");
  tempsensor.setResolution(3);
  //internal temperature end

  //accelerometer start
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");
  mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
  mpu.setMotionDetectionThreshold(1);
  mpu.setMotionDetectionDuration(20);
  mpu.setInterruptPinLatch(true);	// Keep it latched.  Will turn off when reinitialized.
  mpu.setInterruptPinPolarity(true);
  mpu.setMotionInterrupt(true);
  //accelerometer end

  //GPS start
  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);  // Start GPS communication
  // 9600 NMEA is the default baud rate for Adafruit MTK GPS's- some use 4800
  GPS.begin(9600 );
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // For parsing data, we don't suggest using anything but either RMC only or RMC+GGA since
  // the parser doesn't care about other sentences at this time
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // 1 Hz update rate
  // For the parsing code to work nicely and have time to sort thru the data, and
  // print it out we don't suggest using anything higher than 1 Hz

  // Request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);

  delay(1000);

  // Ask for firmware version
  GPSSerial.println(PMTK_Q_RELEASE);
  //GPS end


  //Firebase start
  initWiFi();
  timeClient.begin();

  // Assign the api key (required)
  config.api_key = API_KEY;

  // Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the RTDB URL (required)
  config.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  // Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);

  // Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Update database path
  databasePath = "/UsersData/" + uid + "/readings";
  //Firebase end
}

void loop() {
  // put your main code here, to run repeatedly:
  delay (10000);// delai arbitraire 

  //External temp start
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) ) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  External Temperature: "));
  Serial.print(t);
  Serial.println(F("°C "));
  //External temperature end

  //Internal temperature start
  tempsensor.wake();
  //Serial.print("Resolution in mode: ");
  //Serial.println (tempsensor.getResolution());
  float celsius = tempsensor.readTempC();
  Serial.print("Internal Temp: "); 
  Serial.print(celsius, 4); Serial.print("*C\n"); 
  delay(500);
  Serial.println("Shutdown MCP9808.... ");
  tempsensor.shutdown_wake(1);
  //Serial.println("");
  //Internal temp end

  //Accelerometer start
  int movement = 0 ; //by default we assume no mouvement
  if(mpu.getMotionInterruptStatus() ) {
    /* Get new sensor events with the readings */
    movement = 1;// If one of the measured value changed, motion detected
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    /* Print out the values */
    Serial.print("AccelX:");
    Serial.print(a.acceleration.x);
    Serial.print(",");
    Serial.print("AccelY:");
    Serial.print(a.acceleration.y);
    Serial.print(",");
    Serial.print("AccelZ:");
    Serial.print(a.acceleration.z);
    Serial.print(", ");
    Serial.print("GyroX:");
    Serial.print(g.gyro.x);
    Serial.print(",");
    Serial.print("GyroY:");
    Serial.print(g.gyro.y);
    Serial.print(",");
    Serial.print("GyroZ:");
    Serial.print(g.gyro.z);
    Serial.println("");
  }
  if(movement){
    Serial.println("Movement Detected");
  }else{
    Serial.println("Stationary");
  }
  //Accelerometer end

  //GPS start
  // read data from the GPS in the 'main loop'
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
  if (GPSECHO)
    if (c) Serial.print(c);
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences!
    // so be very wary if using OUTPUT_ALLDATA and trying to print out data
    Serial.print(GPS.lastNMEA()); // this also sets the newNMEAreceived() flag to false
    if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
      return; // we can fail to parse a sentence in which case we should just wait for another
  }

  // approximately every 2 seconds or so, print out the current stats
  
  if (millis() - timer > 2000) { //
    timer = millis(); // reset the timer
    Serial.print("\nTime: ");
    if (GPS.hour < 10) { Serial.print('0'); }
    Serial.print((GPS.hour-4), DEC); Serial.print(':');
    if (GPS.minute < 10) { Serial.print('0'); }
    Serial.print(GPS.minute, DEC); Serial.print(':');
    if (GPS.seconds < 10) { Serial.print('0'); }
    Serial.print(GPS.seconds, DEC); Serial.print('.');
    if (GPS.milliseconds < 10) {
      Serial.print("00");
    } else if (GPS.milliseconds > 9 && GPS.milliseconds < 100) {
      Serial.print("0");
    }
    Serial.println(GPS.milliseconds);
    Serial.print("Date: ");
    Serial.print(GPS.day, DEC); Serial.print('/');
    Serial.print(GPS.month, DEC); Serial.print("/20");
    Serial.println(GPS.year, DEC);
    Serial.print("Fix: "); Serial.print((int)GPS.fix);
    Serial.print(" quality: "); Serial.println((int)GPS.fixquality);
    if (GPS.fix) {
      Serial.print("Location: ");
      Serial.print(GPS.latitude, 4); Serial.print(GPS.lat);
      Serial.print(", ");
      Serial.print(GPS.longitude, 4); Serial.println(GPS.lon);
      Serial.print("Location in Degrees: ");
      Serial.print(GPS.latitudeDegrees, 8); 
      Serial.print(", ");
      Serial.println(GPS.longitudeDegrees, 8);
      Serial.print("Speed (knots): "); Serial.println(GPS.speed);
      Serial.print("Speed (km/h): "); Serial.println(1.852*GPS.speed);
      Serial.print("Angle: "); Serial.println(GPS.angle);
      Serial.print("Altitude: "); Serial.println(GPS.altitude);
      Serial.print("Satellites: "); Serial.println((int)GPS.satellites);
      Serial.print("Antenna status: "); Serial.println((int)GPS.antenna);
      latDeg=GPS.latitudeDegrees; longDeg=GPS.longitudeDegrees; speedKMH=1.852*GPS.speed; Altitude=GPS.altitude;
    }
  }
  //GPS end

  //firebase start
  // Send new readings to database
  if (Firebase.ready() ){ //&& (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)
    sendDataPrevMillis = millis();

    //Get current timestamp
    timestamp = getTime();
    Serial.print ("time: ");
    Serial.println (timestamp);

    parentPath= databasePath + "/" + String(timestamp);

    json.set(extTempPath.c_str(), String(5566));
    json.set(humPath.c_str(), String(44345));
    json.set(intTempPath.c_str(), String(65656));
    json.set(timePath, String(timestamp));
    Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
  }
  //firebase end

    Serial.println();
}
