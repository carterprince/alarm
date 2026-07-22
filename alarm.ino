#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <Timezone.h>

// --- Network Credentials ---
const char* ssid = "SSID";
const char* password = "PASSWORD"; // Replace with your password

// --- Alarm Settings ---
const int ALARM_HOUR = 8;
const int ALARM_MINUTE = 0;
const int ALARM_FREQUENCY = 2650;  // Frequency in Hz (2000-4000 is typically loudest)

// --- Hardware Pins ---
const int BUZZER_PIN = 11;
const int BUTTON_PIN = 7;

// --- Timezone Rules (US Eastern Time) ---
TimeChangeRule usEDT = {"EDT", Second, Sun, Mar, 2, -240};  
TimeChangeRule usEST = {"EST", First, Sun, Nov, 2, -300};   
Timezone usEastern(usEDT, usEST);

// --- NTP Setup ---
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 3600000);

// --- Alarm State Variables ---
bool alarmActive = false;
bool alarmTriggeredToday = false;
unsigned long alarmStartTime = 0; 
unsigned long lastPrintTime = 0;   

void setup() {
  Serial.begin(115200);
  
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  
  while(!timeClient.update()) {
    timeClient.forceUpdate();
    delay(500);
  }
  Serial.println("Time synchronized successfully.\n");
}

void loop() {
  timeClient.update();

  unsigned long utcEpoch = timeClient.getEpochTime();
  time_t localTime = usEastern.toLocal(utcEpoch);

  int currentHour = hour(localTime);
  int currentMinute = minute(localTime);
  int currentSecond = second(localTime);

  // Print current time to Serial once per second
  if (millis() - lastPrintTime >= 1000) {
    lastPrintTime = millis();
    
    char timeStr[16];
    sprintf(timeStr, "[%02d:%02d:%02d]", currentHour, currentMinute, currentSecond);
    
    Serial.print(timeStr);
    Serial.print(" Alarm is set for ");
    Serial.print(ALARM_HOUR < 10 ? "0" : "");
    Serial.print(ALARM_HOUR);
    Serial.print(":");
    Serial.print(ALARM_MINUTE < 10 ? "0" : "");
    Serial.print(ALARM_MINUTE);

    if (alarmActive) {
      Serial.println(" - *** ALARM RINGING ***");
    } else {
      Serial.println(" - Waiting...");
    }
  }

  // Alarm Trigger Logic
  if (currentHour == ALARM_HOUR && currentMinute == ALARM_MINUTE) {
    if (!alarmTriggeredToday && !alarmActive) {
      alarmActive = true;
      alarmTriggeredToday = true;
      alarmStartTime = utcEpoch;
    }
  } else {
    alarmTriggeredToday = false; 
  }

  // Alarm Action Logic
  if (alarmActive) {
    if (digitalRead(BUTTON_PIN) == LOW || (utcEpoch - alarmStartTime >= 3600)) {
      alarmActive = false;
      noTone(BUZZER_PIN);
      Serial.println("\n--- Alarm stopped ---");
      delay(500); 
    } 
    else {
      playAlarmPattern();
    }
  }
}

// Clean, solid, high-pitch dual-beep pattern
void playAlarmPattern() {
  unsigned long cycleTime = millis() % 1000;
  
  if (cycleTime < 150) {
    tone(BUZZER_PIN, ALARM_FREQUENCY); // First solid beep
  } 
  else if (cycleTime < 300) {
    noTone(BUZZER_PIN);     
  } 
  else if (cycleTime < 450) {
    tone(BUZZER_PIN, ALARM_FREQUENCY); // Second solid beep
  } 
  else {
    noTone(BUZZER_PIN);     
  }
}
