#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <Timezone.h>

// --- Network ---
const char* ssid = "SSID";
const char* password = "password";

// --- Alarm ---
const int ALARM_HOUR = 8;
const int ALARM_MINUTE = 30;
const int ALARM_FREQUENCY = 2645;

const int BUZZER_PIN = 11;
const int BUTTON_PIN = 7;

const unsigned long ALARM_DURATION_MS = 60UL * 60UL * 1000UL;
const int CATCHUP_MINUTES = 5;

// --- Eastern time ---
TimeChangeRule usEDT = {"EDT", Second, Sun, Mar, 2, -240};
TimeChangeRule usEST = {"EST", First, Sun, Nov, 2, -300};
Timezone usEastern(usEDT, usEST);

// --- NTP: always UTC ---
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 3600000UL);

const unsigned long WIFI_RETRY_MS = 30000UL;
const unsigned long NTP_RETRY_MS = 60000UL;
const unsigned long NTP_SYNC_MS = 3600000UL;

unsigned long lastWifiAttempt = 0;
unsigned long lastNtpAttempt = 0;
unsigned long ntpAttemptInterval = NTP_RETRY_MS;

bool udpStarted = false;
bool ntpAttempted = false;
bool clockValid = false;

// --- Alarm state ---
bool alarmActive = false;
unsigned long alarmStartedMs = 0;
unsigned long lastAlarmDate = 0;

void serviceNetwork() {
  unsigned long ms = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (udpStarted) {
      timeClient.end();
      udpStarted = false;
    }

    // Request an immediate NTP attempt after reconnection.
    ntpAttempted = false;

    if (ms - lastWifiAttempt >= WIFI_RETRY_MS) {
      Serial.println("Retrying WiFi...");
      WiFi.begin(ssid, password);
      lastWifiAttempt = millis();
    }

    return;
  }

  if (!udpStarted) {
    timeClient.begin();
    udpStarted = true;
    Serial.println("WiFi connected.");
  }

  if (ntpAttempted &&
      ms - lastNtpAttempt < ntpAttemptInterval) {
    return;
  }

  // forceUpdate() can block briefly, but we only call it
  // at the controlled intervals above.
  bool success = timeClient.forceUpdate();

  lastNtpAttempt = millis();
  ntpAttempted = true;

  if (success) {
    setTime((time_t)timeClient.getEpochTime()); // Store UTC.
    clockValid = true;
    ntpAttemptInterval = NTP_SYNC_MS;
    Serial.println("NTP synchronized.");
  } else {
    ntpAttemptInterval = NTP_RETRY_MS;
    Serial.println("NTP failed; local clock continues if set.");
  }
}

void playAlarmPattern() {
  // Anchor the pattern to the start of this alarm.
  unsigned long phase = (millis() - alarmStartedMs) % 1000UL;

  if (phase < 150 || (phase >= 300 && phase < 450)) {
    tone(BUZZER_PIN, ALARM_FREQUENCY);
  } else {
    noTone(BUZZER_PIN);
  }
}

void setup() {
  Serial.begin(115200); // Do not wait for Serial.

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  noTone(BUZZER_PIN);

  WiFi.begin(ssid, password);
  lastWifiAttempt = millis();

  // No indefinite waits for WiFi or NTP.
}

void loop() {
  // TimeLib keeps advancing locally even without WiFi.
  // Calling now() regularly also keeps its millis tracking current.
  time_t utc = now();

  // Keep network delays out of the audible alarm.
  if (!alarmActive) {
    serviceNetwork();
    utc = now();
  }

  // After boot, we need one successful sync before knowing the time.
  if (!clockValid) {
    return;
  }

  time_t local = usEastern.toLocal(utc);

  unsigned long dateKey =
      (unsigned long)year(local) * 10000UL +
      (unsigned long)month(local) * 100UL +
      (unsigned long)day(local);

  int currentMinutes = hour(local) * 60 + minute(local);
  int alarmMinutes = ALARM_HOUR * 60 + ALARM_MINUTE;

  bool inAlarmWindow =
      currentMinutes >= alarmMinutes &&
      currentMinutes < alarmMinutes + CATCHUP_MINUTES;

  if (!alarmActive &&
      dateKey != lastAlarmDate &&
      inAlarmWindow) {
    alarmActive = true;
    lastAlarmDate = dateKey;
    alarmStartedMs = millis();
    Serial.println("Alarm started.");
  }

  if (alarmActive) {
    bool buttonPressed = digitalRead(BUTTON_PIN) == LOW;
    bool timedOut =
        millis() - alarmStartedMs >= ALARM_DURATION_MS;

    if (buttonPressed || timedOut) {
      alarmActive = false;
      noTone(BUZZER_PIN);
      Serial.println("Alarm stopped.");
    } else {
      playAlarmPattern();
    }
  }
}
