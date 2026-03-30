#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <NewPing.h>
#include <WiFi.h>

// ── WiFi CREDENTIALS ─────────────────────────────────────────
const char* ssid     = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
const String BUS_ID  = "OSRTC-BUS-07";

// ── PIN DEFINITIONS ──────────────────────────────────────────
#define TRIG_PIN       5
#define ECHO_PIN       18
#define PIR_PIN        19
#define DHT_PIN        4
#define MQ_PIN         34
#define VIBRATION_PIN  35
#define SOUND_PIN      32
#define IR_PIN         33
#define BUZZER_PIN     13
#define RELAY_PIN      26
#define LED_GREEN      25
#define LED_YELLOW     12
#define LED_RED        27

// ── THRESHOLDS ───────────────────────────────────────────────
#define DIST_CRITICAL   20      // cm — collision risk
#define DIST_WARNING    80      // cm — caution zone
#define TEMP_WARNING    40      // C  — engine overheat
#define TEMP_CRITICAL   50      // C  — fire risk
#define MQ_THRESHOLD    1600    // 12-bit ADC scaled (0-4095)
#define MAX_DISTANCE    400     // cm — sonar max range
#define NO_RESPONSE_MS  5000    // ms — Level 4 countdown

// ── OBJECTS ──────────────────────────────────────────────────
DHT dht(DHT_PIN, DHT11);
LiquidCrystal_I2C lcd(0x27, 16, 2);
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

// ── ALERT LEVELS ─────────────────────────────────────────────
#define ALL_SAFE  0
#define LEVEL_1   1   // Caution  — single beep every 3s, green LED
#define LEVEL_2   2   // Warning  — double beep, yellow LED
#define LEVEL_3   3   // Critical — continuous alarm, red LED, relay cuts ignition
#define LEVEL_4   4   // Emergency — WiFi SOS sent to authorities

int  currentLevel         = ALL_SAFE;
unsigned long lastRead    = 0;
unsigned long lastBeep    = 0;
unsigned long level3Start = 0;
bool sosSent              = false;

// ── FUNCTION DECLARATIONS ────────────────────────────────────
void setAlert(int level, String line1, String line2);
void handleBuzzer();
void sendWiFiSOS();

// ============================================================
//  SETUP — runs once on power on
// ============================================================
void setup() {
  Serial.begin(115200);
  dht.begin();
  lcd.init();
  lcd.backlight();

  pinMode(PIR_PIN,       INPUT);
  pinMode(VIBRATION_PIN, INPUT);
  pinMode(SOUND_PIN,     INPUT);
  pinMode(IR_PIN,        INPUT);
  pinMode(BUZZER_PIN,    OUTPUT);
  pinMode(RELAY_PIN,     OUTPUT);
  pinMode(LED_GREEN,     OUTPUT);
  pinMode(LED_YELLOW,    OUTPUT);
  pinMode(LED_RED,       OUTPUT);

  // Relay OFF by default (active LOW relay)
  digitalWrite(RELAY_PIN, HIGH);

  // Connect to WiFi
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi.");
  WiFi.begin(ssid, password);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    tries++;
  }

  // Startup screen
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  KAVACH  v2.0  ");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.status() == WL_CONNECTED ? " WiFi Connected " : " No WiFi-Local  ");
  delay(2000);
  lcd.clear();

  Serial.println("KAVACH v2.0 Ready — " + BUS_ID);
}

// ============================================================
//  LOOP — runs every 500ms
// ============================================================
void loop() {
  if (millis() - lastRead < 500) return;
  lastRead = millis();

  // ── READ ALL SENSORS ───────────────────────────────────────
  long dist      = sonar.ping_cm();
  if (dist == 0) dist = MAX_DISTANCE;  // 0 = no echo = clear path

  bool pirHigh   = digitalRead(PIR_PIN);       // HIGH = driver present
  bool vibration = digitalRead(VIBRATION_PIN); // HIGH = impact detected
  bool soundHigh = digitalRead(SOUND_PIN);     // HIGH = sound detected
  bool irBlocked = digitalRead(IR_PIN);        // HIGH = door open or blind spot

  float temp = dht.readTemperature();
  int   mqVal = analogRead(MQ_PIN);

  // ── DERIVED FLAGS ──────────────────────────────────────────
  bool driverAbsent  = !pirHigh;
  bool cabinSilent   = !soundHigh;
  bool obstacleClose = (dist < DIST_CRITICAL);
  bool obstacleNear  = (dist < DIST_WARNING);
  bool overheating   = (temp > TEMP_WARNING);
  bool fireRisk      = (mqVal > MQ_THRESHOLD && temp > TEMP_CRITICAL);
  bool fumeOnly      = (mqVal > MQ_THRESHOLD);
  bool crashDetected = (vibration && driverAbsent);
  bool microsleep    = (driverAbsent && cabinSilent);

  // ── SERIAL DEBUG ───────────────────────────────────────────
  Serial.print("Dist:");    Serial.print(dist);
  Serial.print("cm | PIR:"); Serial.print(pirHigh   ? "PRESENT" : "ABSENT");
  Serial.print(" | Temp:"); Serial.print(temp);
  Serial.print("C | MQ:");  Serial.print(mqVal);
  Serial.print(" | VIB:");  Serial.print(vibration);
  Serial.print(" | SND:");  Serial.print(soundHigh);
  Serial.print(" | IR:");   Serial.println(irBlocked);

  // ── LEVEL 4: No response after 5s at Level 3 ───────────────
  if (currentLevel == LEVEL_3 && !sosSent) {
    if (millis() - level3Start >= NO_RESPONSE_MS) {
      sendWiFiSOS();
      sosSent = true;
      setAlert(LEVEL_4, "SOS SENT!", "HELP COMING");
      return;
    }
  }

  // ── SENSOR FUSION DECISION LOGIC ───────────────────────────

  if (crashDetected) {
    // Vibration spike + PIR no motion = crash event (Level 4 in doc)
    setAlert(LEVEL_3, "CRASH DETECTED", "SOS IN 5s...");

  } else if (fireRisk) {
    // MQ HIGH + DHT11 > 50C = fire confirmed
    setAlert(LEVEL_3, "FIRE RISK!", "STOP NOW");

  } else if (driverAbsent && obstacleClose) {
    // PIR absent + HC-SR04 < 20cm = imminent collision
    setAlert(LEVEL_3, "DANGER!", "STOP NOW");

  } else if (microsleep) {
    // PIR absent + sound silent = possible microsleep
    setAlert(LEVEL_2, "MICROSLEEP?", "CHECK DRIVER");

  } else if (overheating) {
    // Temperature above warning threshold
    setAlert(LEVEL_2, "CHECK ENGINE", "HEAT:" + String((int)temp) + "C");

  } else if (fumeOnly) {
    // MQ HIGH alone = CO / gas leak
    setAlert(LEVEL_2, "CO DETECTED", "VENTILATE NOW");

  } else if (irBlocked) {
    // IR triggered = door open or blind spot blocked
    setAlert(LEVEL_2, "DOOR OPEN /", "BLIND SPOT!");

  } else if (driverAbsent && obstacleNear) {
    // Driver missing + obstacle nearby
    setAlert(LEVEL_2, "DRIVER ABSENT", "OBSTACLE NEAR");

  } else if (obstacleNear) {
    // HC-SR04 < 80cm alone = Level 1 caution
    setAlert(LEVEL_1, "OBSTACLE AHEAD", String(dist) + "cm away");

  } else {
    // Everything is fine
    sosSent = false;
    setAlert(ALL_SAFE, "KAVACH: ACTIVE", "ALL SYSTEMS OK");
  }

  handleBuzzer();
}

// ============================================================
//  SET ALERT LEVEL
// ============================================================
void setAlert(int level, String line1, String line2) {
  // Start Level 4 countdown when Level 3 first triggers
  if (level == LEVEL_3 && currentLevel != LEVEL_3) {
    level3Start = millis();
  }

  currentLevel = level;

  // LCD display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, 16));

  // Turn all LEDs off first
  digitalWrite(LED_GREEN,  LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED,    LOW);

  // Light only the correct LED
  if (level == ALL_SAFE || level == LEVEL_1) {
    digitalWrite(LED_GREEN,  HIGH);
  } else if (level == LEVEL_2) {
    digitalWrite(LED_YELLOW, HIGH);
  } else if (level >= LEVEL_3) {
    digitalWrite(LED_RED,    HIGH);
  }

  // Relay cuts ignition on Level 3 and above
  digitalWrite(RELAY_PIN, (level >= LEVEL_3) ? LOW : HIGH);
}

// ============================================================
//  BUZZER PATTERNS
// ============================================================
void handleBuzzer() {
  unsigned long now = millis();

  if (currentLevel == ALL_SAFE) {
    // Silent
    noTone(BUZZER_PIN);

  } else if (currentLevel == LEVEL_1) {
    // Single beep every 3 seconds
    if (now - lastBeep > 3000) {
      tone(BUZZER_PIN, 1000, 200);
      lastBeep = now;
    }

  } else if (currentLevel == LEVEL_2) {
    // Rhythmic double beep every 1.5 seconds
    if (now - lastBeep > 1500) {
      tone(BUZZER_PIN, 1500, 150);
      delay(200);
      tone(BUZZER_PIN, 1500, 150);
      lastBeep = now;
    }

  } else if (currentLevel >= LEVEL_3) {
    // Continuous loud alarm
    tone(BUZZER_PIN, 2500);
  }
}

// ============================================================
//  WIFI SOS SENDER — Level 4 Emergency
// ============================================================
void sendWiFiSOS() {
  Serial.println("==============================");
  Serial.println("EMERGENCY SOS TRIGGERED");
  Serial.println("Bus ID : " + BUS_ID);
  Serial.println("Alert  : No human response 5s");
  Serial.println("==============================");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected — SOS via Serial only");
    return;
  }

  // Uncomment below and add your server URL to send HTTP POST
  // #include <HTTPClient.h>
  // WiFiClient client;
  // HTTPClient http;
  // http.begin(client, "http://YOUR_SERVER/sos");
  // http.addHeader("Content-Type", "application/json");
  // String payload = "{\"bus\":\"" + BUS_ID + "\",\"alert\":\"EMERGENCY\"}";
  // int code = http.POST(payload);
  // Serial.println("HTTP Response: " + String(code));
  // http.end();
}
