// Frequence cardiaque start
#include "MAX30105.h"
#include "heartRate.h"
#include "config.h"  // Include configuration file
// Create an instance of the MAX30105 class to interact with the sensor
MAX30105 particleSensor;
// end Frequence cardiaque


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

/* LED */
#define LED_PIN 32

/* BUZZER */
#define BUZZER_PIN 33

/* BUTTON */
#define BUTTON_PIN 15

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
// WIFI_SSID and WIFI_PASSWORD are now defined in config.h

// Insert Firebase project API Key
// API_KEY, USER_EMAIL, USER_PASSWORD, and DATABASE_URL are now defined in config.h

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
String bpmPath = "/BPM";
String movPath = "/movement"; //bool
String timePath = "/timestamp";
String GPSdate = "/GPSdate";
String GPStime = "/GPStime";
String speedPath = "/speed";
String altPath = "/altitude";
String longDegPath = "/longitudeDegrees";
String latDegPath = "/latitudeDegrees";
String timeImmobPath = "/tempsDimmobilite";
String buttonStatePath = "/ButtonState";
String SG_Path = "/ScoreDeGravite";
String AlpiStatePath = "/EtatDeLalpiniste";


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
  Serial.print(F("Connecting to WiFi .."));
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


/* Definition des dictionnaires de donnees pour chaque capteur */

/*Definition d un dictionnaire de donnees generique */
struct DicCapteurs {
  const char* ID;
  float min;
  float max;
};

/*Tableau statique avec les limites des capteurs */
const DicCapteurs tab_limites[] = {
  {"FC", 30, 220}, //Frequence Cardiaque (BPM)
  {"T_c", 25, 42}, //Temperature corporelle (°C)
  {"T_e", -60, 50}, //Temperature exterieure (°C)
  {"t_immobile", 0, 10800} //Temps d immobilite (s)
};

bool check_val_dic(const char* capteur, const float& valeur ){
  for (const auto& c : tab_limites){
    if(strcmp(c.ID, capteur) == 0) return (valeur >= c.min && valeur <= c.max);
  }
  return false; //Capteur non reconnu ou valeurs invalides
}

struct extern_temp_struct {
  float h;
  float t;
};

struct extern_temp_struct* extern_temp(){
  //External temp start
  static struct extern_temp_struct my_extern_temp; // Utilsation de memoire statique 
  my_extern_temp.h = dht.readHumidity();
  my_extern_temp.t = dht.readTemperature();
  // Check if any reads failed and exit early (to try again).
  if (isnan(my_extern_temp.h) || isnan(my_extern_temp.t) ) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return NULL;
  }
  // Serial.print(F("Humidity: "));
  // Serial.print(h);
  // Serial.print(F("%  External Temperature: "));
  // Serial.print(t);
  // Serial.println(F("°C "));
  return &my_extern_temp;
  //External temperature end

}

float intern_temp(){
  //Internal temperature start
  tempsensor.wake();
  //Serial.print("Resolution in mode: ");
  //Serial.println (tempsensor.getResolution());
  float celsius = tempsensor.readTempC();
  // Serial.print("Internal Temp: "); 
  // Serial.print(celsius, 4); Serial.print("*C\n"); 
  delay(500);
  Serial.println(F("Shutdown MCP9808.... "));
  tempsensor.shutdown_wake(1);
  //Serial.println("");
  return celsius;
  //Internal temp end
}

unsigned long last_mov = 0;
float temps_immobile = 0;

int accelerometer(){
  //Accelerometer start
  int movement = 0 ; //by default we assume no mouvement
  if(mpu.getMotionInterruptStatus() ) {
    /* Get new sensor events with the readings */
    movement = 1;// If one of the measured value changed, motion detected
    temps_immobile = 0;
    last_mov = millis(); // Mise a jour du dernier mouvement
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
  }
  if(movement){
    Serial.println(F("Movement Detected"));
  }else{
    temps_immobile = (float(millis()) - float(last_mov)) / 60000;
    Serial.print(F("Stationary, Temps immobile : "));
    Serial.print(temps_immobile);
    Serial.println(F(" min"));
  }
  return movement;
  //Accelerometer end

}

struct GPS_struct{
  char* date;
  char* time;
  float speed_kmh;
  float altitude;
  float longitude;
  float latitude;
};

void getGPSDateTime(char* dateStr, char* timeStr, size_t size) {
    // Formater la date au format "JJ/MM/AAAA"
    snprintf(dateStr, size, "%02d/%02d/20%02d", GPS.day, GPS.month, GPS.year);

    // Formater l'heure au format "HH:MM:SS"
    snprintf(timeStr, size, "%02d:%02d:%02d", GPS.hour - 4, GPS.minute, GPS.seconds);
}

struct GPS_struct* GPS_func(){
  //GPS start
  static struct GPS_struct my_gps;
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
      return NULL; // we can fail to parse a sentence in which case we should just wait for another
  }

  // approximately every 2 seconds or so, print out the current stats
  
  if (millis() - timer > 2000) { //
    timer = millis(); // reset the timer
    // Serial.print("\nTime: ");
    // if (GPS.hour < 10) { Serial.print('0'); }
    // Serial.print((GPS.hour-4), DEC); Serial.print(':');
    // if (GPS.minute < 10) { Serial.print('0'); }
    // Serial.print(GPS.minute, DEC); Serial.print(':');
    // if (GPS.seconds < 10) { Serial.print('0'); }
    // Serial.print(GPS.seconds, DEC); Serial.print('.');
    // if (GPS.milliseconds < 10) {
    //   Serial.print("00");
    // } else if (GPS.milliseconds > 9 && GPS.milliseconds < 100) {
    //   Serial.print("0");
    // }
    // Serial.println(GPS.milliseconds);
    // Serial.print("Date: ");
    // Serial.print(GPS.day, DEC); Serial.print('/');
    // Serial.print(GPS.month, DEC); Serial.print("/20");
    // Serial.println(GPS.year, DEC);
    // Serial.print("Fix: "); Serial.print((int)GPS.fix);
    // Serial.print(" quality: "); Serial.println((int)GPS.fixquality);

    // Déclaration des buffers pour stocker la date et l'heure
    char dateStr[20];  // "DD/MM/YYYY\0" 
    char timeStr[20];   // "HH:MM:SS\0" 

    // Obtenir la date et l'heure
    getGPSDateTime(dateStr, timeStr, sizeof(dateStr));
    my_gps.date = dateStr;
    my_gps.time = timeStr;

    // Affichage
    Serial.print(F("Date GPS: "));
    Serial.println(dateStr);
    Serial.print(F("Heure GPS: "));
    Serial.println(timeStr);

    Serial.print(F("Fix: ")); Serial.print((int)GPS.fix);
    if (GPS.fix) {
      Serial.print(F("Location: "));
      Serial.print(GPS.latitude, 4); Serial.print(GPS.lat);
      Serial.print(", ");
      Serial.print(GPS.longitude, 4); Serial.println(GPS.lon);
      Serial.print(F("Location in Degrees: "));
      Serial.print(GPS.latitudeDegrees, 8); 
      Serial.print(F(", "));
      Serial.println(GPS.longitudeDegrees, 8);
      Serial.print(F("Speed (knots): ")); Serial.println(GPS.speed);
      Serial.print(F("Speed (km/h): ")); Serial.println(1.852*GPS.speed);
      Serial.print(F("Angle: ")); Serial.println(GPS.angle);
      Serial.print(F("Altitude: ")); Serial.println(GPS.altitude);
      Serial.print(F("Satellites: ")); Serial.println((int)GPS.satellites);
      Serial.print(F("Antenna status: ")); Serial.println((int)GPS.antenna);
      latDeg=GPS.latitudeDegrees; longDeg=GPS.longitudeDegrees; speedKMH=1.852*GPS.speed; Altitude=GPS.altitude;

      my_gps.speed_kmh = 1.852*GPS.speed;
      my_gps.altitude = GPS.altitude;
      my_gps.longitude = GPS.longitudeDegrees;
      my_gps.latitude = GPS.latitudeDegrees;

    }
  }
  return &my_gps;
  //GPS end
}

// Define the size of the rates array for averaging BPM; can be adjusted for smoother results
  const byte RATE_SIZE = 5; // Increase this for more averaging. 4 is a good starting point.
  byte rates[RATE_SIZE]; // Array to store heart rate readings for averaging
  byte rateSpot = 0; // Index for inserting the next heart rate reading into the array
  long lastBeat = 0; // Timestamp of the last detected beat, used to calculate BPM

  float beatsPerMinute; // Calculated heart rate in beats per minute
  int beatAvg; // Average heart rate after processing multiple readings

/* Fonction pour le capteur de fréquence cardiaque */
void BPM_func(void* pvParameters){
  while(1){
    long irValue = particleSensor.getIR(); // Read the infrared value from the sensor
 
    if (checkForBeat(irValue) == true) { // Check if a heart beat is detected
      long delta = millis() - lastBeat; // Calculate the time between the current and last beat
      lastBeat = millis(); // Update lastBeat to the current time
 
      beatsPerMinute = 60 / (delta / 1000.0); // Calculate BPM
 
      // Ensure BPM is within a reasonable range before updating the rates array
      if (beatsPerMinute < 255 && beatsPerMinute > 20) {
        rates[rateSpot++] = (byte)beatsPerMinute; // Store this reading in the rates array
        rateSpot %= RATE_SIZE; // Wrap the rateSpot index to keep it within the bounds of the rates array
 
        // Compute the average of stored heart rates to smooth out the BPM
        beatAvg = 0;
        for (byte x = 0 ; x < RATE_SIZE ; x++)
          beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }
    }
 
    // Output the current IR value, BPM, and averaged BPM to the serial monitor
    /*Serial.print("IR="); Serial.print(irValue); Serial.print(", BPM="); Serial.print(beatsPerMinute); Serial.print(", Avg BPM="); Serial.print(beatAvg);*/
 
    // Check if the sensor reading suggests that no finger is placed on the sensor
    if (irValue < 50000)
      Serial.println(F(" No finger?"));
      //beatAvg =0;
    // delay(1000);
  }
}

//Calcule le risque lie a la frequence cardiaque et la temperature corpporelle
float score_frequence_cardiaque_temp(const float& T_c, const int& FC){
  if (FC < 40 || T_c < 32) return 1.0; // Situation critique : risque vital
  else if (FC > 120 && T_c >= 32 && T_c < 35) return 0.9; // Hypothermie compensatoire
  else if (FC > 140 || (FC < 50 && T_c >= 32 && T_c < 36)) return 0.8; // Tachychardie ou bradycardie moderee
  else if ( (FC >= 100 && FC <= 140) && (T_c >= 35 and T_c < 36) ) return 0.7; //risque leger
  else if ( (FC >= 50 && FC < 100) && (T_c >= 36 and T_c <= 38.5) ) return 0.5; //valeurs normales
  else if ( FC > 180 || T_c > 40) return 1; //Hyperthermie ou crise cardiaque
  else return 0.0; //aucun risque detecte
}

//Calcule le risque d immobilite prolongee 
float score_immobilite(const int& mov, const float& t_immobile){
  if (mov == 0){ //Personne me bouge plus
    if (t_immobile > 30) return 1.0; // Immobilite critique
    else if (t_immobile > 20) return 0.8; // Immobilite prolongee avec risque elevee
    else if (t_immobile > 10) return 0.5; // Surveillance necessaire
  }
  else return 0.0; // Personne en mouvement
}

//Calcule le risque lie a la temperature corporelle et a la temperature externe 
float score_temp_env (const float& T_c, const float& T_e, const int& FC){
  float score = 0.0;
  if (T_c < 32 || T_c > 40) score = 1.0; // Hypothermie ou hyperthermie critique 
  else if ( (T_c >= 32 && T_c < 35) && T_e < -10) score = 0.9; // Hypothermie severe dans un environnement froid
  else if ( (T_c >= 35 && T_c < 36) && T_e < -15) score = 0.8; // Risque d hypothermie rapide
  else if ( (T_c >= 36 && T_c < 37) && T_e < -10) score = 0.7; // Debut d impact du froid
  else if ( T_c >= 37 && T_c <= 38.5) score = 0.5; // Temperature corporelle normale
  else if ( T_c > 39 && FC > 150) score = 0.9; // Coup de chaleur potentielle

  if (T_e < -15) score += 0.2; //Risque aggrave en cas de temperature exterieure tres basse 

  return std::min(1.0f, score); 
}

//Calcule le risque lie aux interactions medicales
float score_interaction_medicale (const float& T_c, const int& FC, const float& t_immobile){
  if ( T_c < 32 && FC > 40 && t_immobile > 20) return 1.0; // Risque vital : hypothermie avancee, bradycardie et immobilite
  else if ( T_c < 35 && FC > 120 && t_immobile > 15) return 0.8; // Tachycardie associee a une hypothermie et une immobilite
  else if ( T_c < 35 || t_immobile > 15) return 0.5; // Risque modere  
  else return 0.0; // Aucun risque identifie  
}

//Calcule le score de gravite general (SG)
float calcul_score_gravite (const float& T_c, const int& FC, const int& mov, const float& t_immobile, const float& T_e){
  float w1, w2, w3, w4; //Ponderations pour chaque facteur de risque
  w1 = 0.2; //Poids pour la frequence cardiaque
  w2 = 0.2; //Poids pour l immobilite
  w3 = 0.4; //Poids pour la temp corporelle et exterieure
  w4 = 0.2; //Poids pour l interaction medicale
  
  //Calcul des scores individuels
  float score_fc = score_frequence_cardiaque_temp (T_c, FC);
  //Serial.print("Le score fc est : ");
  //Serial.println(score_fc);
  float score_mov = score_immobilite (mov, t_immobile);
  //Serial.print("Le score mov est : ");
  //Serial.println(score_mov);
  float score_tc_te = score_temp_env (T_c, T_e, FC);
  //Serial.print("Le score tc te est : ");
  //Serial.println(score_tc_te);
  float score_inter = score_interaction_medicale (T_c, FC, t_immobile);
  //Serial.print("Le score inter est : ");
  //Serial.println(score_inter);

  // Combinaison des scores individuels
  float SG = (w1 * score_fc) + (w2 * score_mov) + (w3 * score_tc_te) + (w4 * score_inter);

  return std::min(1.0f, SG);
}

// Donne une interpretation du score de gravite
char* evaluer_niveau_gravite (const float& SG){
  if (SG < 0.3) return "Situation normale";
  else if ( SG >= 0.3 && SG < 0.6) return "Pre-alerte : Risque modere";
  else if ( SG >= 0.6 && SG < 0.8) return "Alerte serieuse : Confirmation requise";
  else return "Alerte critique";
}

/* Tests unitaires pour verifier le bon fonctionnement*/
// void test_cases(){
//   /*Tests pour le score de gravite */
//   struct TestCase_SG {
//     const float temp_intern;
//     const int freq_card;
//     const int movement;
//     const float time_immobilite;
//     const float temp_ext;
//     const char* expected;
//   };

//   TestCase_SG test_cases_sg[] = {
//     {34, 130, 0, 35, -10, "Alerte serieuse : Confirmation requise"},
//     {37, 80, 1, 5, 20, "Situation normale"},
//     {40, 150, 0, 30, 35, "Pre-alerte : Risque modere"},
//     {32, 45, 0, 25, -15, "Alerte serieuse : Confirmation requise"},
//     {38, 100, 1, 0, 30, "Situation normale"},
//   };

//   int total = 0;
//   float score_final;
//   int total_cases = sizeof(test_cases_sg) / sizeof(TestCase_SG);
//   int correct = 0;

//   Serial.println("Tests unitaires pour le SG en cours ...");
//   for (int i = 0; i < total_cases; i++){
//     TestCase_SG tc = test_cases_sg[i];
//     float SG = calcul_score_gravite(tc.temp_intern, tc.freq_card, tc.movement, tc.time_immobilite, tc.temp_ext);
//     const char* desc = evaluer_niveau_gravite(SG);

//     Serial.print("Donnees : (");
//     Serial.print(tc.temp_intern);
//     Serial.print(", ");
//     Serial.print(tc.freq_card);
//     Serial.print(", ");
//     Serial.print(tc.movement);
//     Serial.print(", ");
//     Serial.print(tc.time_immobilite);
//     Serial.print(", ");
//     Serial.print(tc.temp_ext);
//     Serial.print(") => Score : ");
//     Serial.print(SG);
//     Serial.print(", Description : ");
//     Serial.print(desc);
//     if(strcmp(desc, tc.expected) == 0){
//       Serial.println(" ✅ OK ");
//       correct++;
//     } 
//     else Serial.println(" ❌ Erreur");
//   }

//   score_final = (correct / (float)total_cases) * 100.0;
//   Serial.print("\nScore de precision pour SG est : ");
//   Serial.print(score_final);
//   Serial.println("/100");
//   Serial.println("Fin des tests unitaires pour le SG.");

//   struct TestCase_Dic {
//     const char* name;
//     float valeur;
//     bool attendu;
//   };

//   /* Test pour le dictionnaire de donnees */
//   TestCase_Dic test_cases_dic[] = {
//     {"FC", 60, true},      // Fréquence cardiaque normale
//     {"FC", 250, false},    // Trop élevé
//     {"FC", 20, false},     // Trop bas
//     {"T_c", 37, true},     // Température corporelle normale
//     {"T_c", 50, false},    // Trop élevée
//     {"T_c", 10, false},    // Trop basse
//     {"T_e", -10, true},    // Température extérieure normale
//     {"T_e", -100, false},  // Trop basse
//     {"T_e", 60, false},    // Trop élevée
//     {"t_immobile", 5000, true},  // Temps d'immobilité normal
//     {"t_immobile", 11000, false}, // Trop élevé
//     {"t_immobile", -1, false},   // Valeur négative
//     {"Inconnu", 50, false}      // Capteur inexistant
//   };

//   total_cases = sizeof(test_cases_dic) / sizeof(TestCase_Dic); 
//   correct = 0;

//   Serial.println("Tests unitaires pour le dic de donnees en cours ...");
//   for(int i = 0; i < total_cases; i++){
//     bool res = check_val_dic(test_cases_dic[i].name, test_cases_dic[i].valeur);
//     if(res == test_cases_dic[i].attendu){
//       Serial.print(" ✅ OK ");
//       correct++;
//     }
//     else Serial.print(" ❌ Échec");
//     Serial.print(i + 1);
//     Serial.print(": ");
//     Serial.print(test_cases_dic[i].name);
//     Serial.print(" (");
//     Serial.print(test_cases_dic[i].valeur);
//     Serial.print(") -> ");
//     Serial.println(res ? "Valide" : "Invalide");
//   }

//   Serial.print("\nRésultat des tests : ");
//   Serial.print(correct);
//   Serial.print("/");
//   Serial.print(total_cases);
//   Serial.println(" réussis.");
//   Serial.println("Fin des tests unitaires pour le dic de donnees.");


// }

void send_firebase(const float& temp_ext, const float& int_temp, const float& hum, const int& bpm, const int& mov, const char* gps_date, 
                  const char* gps_time, const float& speed, const float& alt, const float& longi, const float& lat, const float& sg, const char* alpi_state, const float& time_immobile,
                  const int& button_state){
  //firebase start
  // Send new readings to database
  if (Firebase.ready() ){ //&& (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)
    sendDataPrevMillis = millis();

    //Get current timestamp
    timestamp = getTime();
    Serial.print (F("time: "));
    Serial.println (timestamp);
    Serial.print (F("BPM :"));
    Serial.println(bpm);

    parentPath= databasePath + "/" + String(timestamp);

    json.set(extTempPath.c_str(), String(temp_ext));
    json.set(intTempPath.c_str(), String(int_temp));
    json.set(humPath.c_str(), String(hum));
    json.set(bpmPath.c_str(), String(bpm));
    json.set(movPath.c_str(), String(mov));
    json.set(timePath, String(timestamp));
    json.set(GPSdate.c_str(), String(gps_date));
    json.set(GPStime.c_str(), String(gps_time));
    json.set(speedPath.c_str(), String(speed));
    json.set(altPath.c_str(), String(alt));
    json.set(longDegPath.c_str(), String(longi));
    json.set(latDegPath.c_str(), String(lat));
    json.set(SG_Path.c_str(), String(sg));
    json.set(AlpiStatePath.c_str(), String(alpi_state));
    json.set(timeImmobPath.c_str(), String(time_immobile));
    json.set(buttonStatePath.c_str(), String(button_state));
    Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
  }
  //firebase end

}

void setup() {
  // put your setup code here, to run once:

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  
  Serial.begin(115200);
  Serial.println(F("Start"));
  dht.begin();

  //internal temperature start
  if (!tempsensor.begin(0x18)) {
    Serial.println(F("Couldn't find MCP9808! Check your connections and verify the address is correct."));
    while (1);
  }
  Serial.println(F("Found MCP9808!"));
  tempsensor.setResolution(3);
  //internal temperature end

  //accelerometer start
  if (!mpu.begin()) {
    Serial.println(F("Failed to find MPU6050 chip"));
    while (1) {
      delay(10);
    }
  }
  Serial.println(F("MPU6050 Found!"));
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
  Serial.println(F("Getting User UID"));
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print(F("User UID: "));
  Serial.println(uid);

  // Update database path
  databasePath = "/UsersData/" + uid + "/readings";
  //Firebase end

  //test_cases();

  //start MAX30102
    // Attempt to initialize the MAX30105 sensor. Check for a successful connection and report.
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) { // Start communication using fast I2C speed
    Serial.println(F("MAX30102 was not found. Please check wiring/power. "));
    //while (1); // Infinite loop to halt further execution if sensor is not found
  }
  Serial.println(F("Place your index finger on the sensor with steady pressure."));
 
  particleSensor.setup(); // Configure sensor with default settings for heart rate monitoring
  particleSensor.setPulseAmplitudeRed(0x0A); // Set the red LED pulse amplitude (intensity) to a low value as an indicator
  particleSensor.setPulseAmplitudeGreen(0); // Turn off the green LED as it's not used here
  //end MAX30102

  // start of Task BPM 
  xTaskCreatePinnedToCore (
    BPM_func,     // Function to implement the task
    "Bpm_function",   // Name of the task
    4096,      // Stack size in bytes
    NULL,      // Task input parameter
    0,         // Priority of the task
    NULL,      // Task handle.
    0          // Core where the task should run
  );
}

void loop() {
  // put your main code here, to run repeatedly:
  delay (10000);// delai arbitraire 

  int button = 0;
  /* Lecture de la temperature externe */ 
  struct extern_temp_struct* my_extern_temp_struct = extern_temp();
  Serial.println(F("Test de la memoire statique ..."));
  Serial.print(F("Humidity: "));
  Serial.print(my_extern_temp_struct->h);
  Serial.print(F("%  External Temperature: "));
  Serial.print(my_extern_temp_struct->t);
  Serial.println(F("°C "));

  /* Lecture de la temperature interne */
  float my_intern_temp = intern_temp();
  Serial.print(F("Depuis le main, Internal Temp: ")); 
  Serial.print(my_intern_temp, 4); Serial.print(F("*C\n")); 

  /* Recuperation du mouvement et du temps d immobilite */
  int mov = accelerometer();
  Serial.print(F("Mouvement 0 ou 1 ? => "));
  Serial.println(mov);
  Serial.print(F("Temps immobile depuis le main : "));
  Serial.println(temps_immobile);

  /* Récupération des données GPS */
  struct GPS_struct* my_gps_struct = GPS_func();

  /* Récupération du BPM */
  int bpm = beatAvg;
  
  /* Calcul du score de gravite avec les valeurs mesurees */
  float SG = calcul_score_gravite(my_intern_temp, bpm, mov, temps_immobile, my_extern_temp_struct->t);
  char* alpi_state = evaluer_niveau_gravite(SG);

  /* Envoyer les donnees a firebase */
  send_firebase(my_extern_temp_struct->t, my_intern_temp, my_extern_temp_struct->h, bpm, mov, 
                my_gps_struct->date, my_gps_struct->time, my_gps_struct->speed_kmh, my_gps_struct->altitude, my_gps_struct->longitude, my_gps_struct->latitude, SG, alpi_state, temps_immobile, button);

  

  // test_cases();
  // digitalWrite(LED_PIN, HIGH);
  // digitalWrite(BUZZER_PIN, HIGH);
  // delay(5000);
  // digitalWrite(LED_PIN, LOW);
  // digitalWrite(BUZZER_PIN, LOW);

  /* Implementation de l algortithme principal */

  if (!strcmp(alpi_state, "Pre-alerte : Risque modere")) {
    digitalWrite(LED_PIN, HIGH); // Allumer la LED rouge
    unsigned long start_time = millis();
    uint8_t pre_alerte = 1; 
    while (millis() - start_time < 30000 && pre_alerte){ // attendre 30s pour que l'alpiniste appuie sur le bouton 
      Serial.println("\t\t\tstill in the first while");
      if(digitalRead(BUTTON_PIN) == HIGH){ // doit être à LOW ou HIGH ?
        digitalWrite(LED_PIN, LOW);
        pre_alerte = 0;
        button = 1;
        delay (5000);// delai arbitraire 
      }
    }
    if(pre_alerte) alpi_state = "Alerte serieuse : Confirmation requise"; // Le bouton n'a pas été pressé, alors on passe à une alerte sérieuse

    /* Actualisation des données sur firebase */
    my_extern_temp_struct = extern_temp();
    my_intern_temp = intern_temp();
    bpm = beatAvg;
    mov = accelerometer();
    my_gps_struct = GPS_func();
    send_firebase(my_extern_temp_struct->t, my_intern_temp, my_extern_temp_struct->h, bpm, mov, 
                  my_gps_struct->date, my_gps_struct->time, my_gps_struct->speed_kmh, my_gps_struct->altitude, my_gps_struct->longitude, my_gps_struct->latitude, SG, alpi_state, temps_immobile,
                  button);

  }

  if (!strcmp(alpi_state,"Alerte serieuse : Confirmation requise")) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH); // Alarme sonore
    unsigned long start_time = millis();
    uint8_t alerte_serieuse = 1;
    while(millis() - start_time < 15000 && alerte_serieuse){
      Serial.println("\t\t\tstill in the second while");
      if(digitalRead(BUTTON_PIN) == HIGH) {
        digitalWrite(LED_PIN, LOW);
        digitalWrite(BUZZER_PIN, LOW);
        alerte_serieuse = 0;
        button = 1;
        delay (5000);// delai arbitraire 
      }
    }
    if(alerte_serieuse) alpi_state = "Alerte critique";

    /* Actualisation des données sur firebase */
    my_extern_temp_struct = extern_temp();
    my_intern_temp = intern_temp();
    bpm = beatAvg;
    mov = accelerometer();
    my_gps_struct = GPS_func();
    send_firebase(my_extern_temp_struct->t, my_intern_temp, my_extern_temp_struct->h, bpm, mov, 
                  my_gps_struct->date, my_gps_struct->time, my_gps_struct->speed_kmh, my_gps_struct->altitude, my_gps_struct->longitude, my_gps_struct->latitude, SG, alpi_state, temps_immobile,
                  button);

  }

  if (!strcmp(alpi_state, "Alerte critique")) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    Serial.println("\t\t\twe are in alerte critique");

    /* Actualisation des données sur firebase */
    my_extern_temp_struct = extern_temp();
    my_intern_temp = intern_temp();
    bpm = beatAvg;
    mov = accelerometer();
    my_gps_struct = GPS_func();
    send_firebase(my_extern_temp_struct->t, my_intern_temp, my_extern_temp_struct->h, bpm, mov, 
                  my_gps_struct->date, my_gps_struct->time, my_gps_struct->speed_kmh, my_gps_struct->altitude, my_gps_struct->longitude, my_gps_struct->latitude, SG, alpi_state, temps_immobile,
                  button);
    delay (5000);
  }

  if (!strcmp(alpi_state, "Situation normale")) {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }
  
  //Serial.println();
}
