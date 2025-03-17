
//External Temperature def start
#include "DHT.h" //Install DHT sensor library by Adafruit
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
#include "Adafruit_MCP9808.h" //Install library Adafruit MCP9808 Library
// Create the MCP9808 temperature sensor object
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();
//Internal temp end

//Accelerometer start
#include <Adafruit_MPU6050.h> // Install Adafruit MPU6050 by Adafruit
#include <Adafruit_Sensor.h> // Install Adafruit Unified Sensor by Adafruit
Adafruit_MPU6050 mpu;
//Accelerometer end

//GPS start
#include <HardwareSerial.h>
#include <Adafruit_GPS.h> // Install Adafruit GPS Library by Adafruit
#define GPS_TX 17  // TX pin of GPS module (connect to ESP32 RX)
#define GPS_RX 16  // RX pin of GPS module (connect to ESP32 TX)
float latDeg, longDeg, speedKMH, Altitude;
HardwareSerial GPSSerial(2);
Adafruit_GPS GPS(&GPSSerial);
#define GPSECHO false
uint32_t timer = millis();
//GPS end

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

    Serial.println();
}
