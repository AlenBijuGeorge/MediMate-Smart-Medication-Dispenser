#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "HX711.h"

bool isLoggedIn = false;
const char* correctUsername = "admin";
const char* correctPassword = "password123";

// Preferences for saving personal data
Preferences preferences;

// Add the loginPage string here, outside of any function:
String loginPage = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
      <title>MediMate Login</title>
      <style id="theme-style">
          body {
              font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
              background-color: #f0f8ff; /* Default light blue */
              color: #212121; /* Default dark text */
              text-align: center;
              margin-top: 100px;
              transition: background-color 0.3s ease, color 0.3s ease;
          }
          .login-container {
              background-color: #ffffff;
              padding: 30px;
              border-radius: 12px;
              box-shadow: 0 6px 12px rgba(0, 0, 0, 0.15);
              display: inline-block;
              max-width: 400px;
              width: 90%;
          }
          h1 {
              color: #1976d2; /* Darker blue title */
              margin-bottom: 30px;
          }
          input[type="text"], input[type="password"] {
              width: calc(100% - 16px);
              padding: 12px;
              margin-bottom: 20px;
              border: 2px solid #bbdefb; /* Lighter blue border */
              border-radius: 8px;
              font-size: 16px;
              background-color: #ffffff; /* Default white input */
              color: #212121; /* Default dark input text */
              transition: background-color 0.3s ease, border-color 0.3s ease, color 0.3s ease;
          }
          button[type="submit"], #contactSupportBtn {
              background-color: #2196f3; /* Blue button */
              color: white;
              padding: 14px 25px;
              border: none;
              border-radius: 8px;
              cursor: pointer;
              font-size: 16px;
              transition: background-color 0.3s ease;
              margin-top: 10px;
          }
          button[type="submit"]:hover, #contactSupportBtn:hover {
              background-color: #1565c0; /* Darker blue hover */
          }
          p#loginStatus {
              color: #d32f2f;
              margin-top: 20px;
              font-weight: 500;
          }
          #supportInfo {
              display: none;
              text-align: left;
              margin: 20px auto;
              width: 80%;
              max-width: 400px;
              background-color: #e3f2fd; /* Very light blue support box */
              padding: 15px;
              border-radius: 8px;
              box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
              color: #212121; /* Default dark text in support info */
              transition: background-color 0.3s ease, color 0.3s ease;
          }
          /* Dark Theme Styles */
          .dark-mode body {
              background-color: #303030;
              color: #f5f5f5;
          }
          .dark-mode .login-container {
              background-color: #424242;
              box-shadow: 0 4px 8px rgba(0, 0, 0, 0.5);
          }
          .dark-mode h1 {
              color: #90caf9;
          }
          .dark-mode input[type="text"], .dark-mode input[type="password"] {
              background-color: #616161;
              color: #f5f5f5;
              border-color: #757575;
          }
          .dark-mode #supportInfo {
              background-color: #424242;
              box-shadow: 0 4px 8px rgba(0, 0, 0, 0.5);
              color: #f5f5f5;
          }
      </style>
  </head>
  <body>
      <div class="login-container">
          <h1>MediMate Login</h1>
          <input type="text" id="username" placeholder="Username" required />
          <input type="password" id="password" placeholder="Password" required />
          <button type="submit" id="loginButton">Login</button>
          <button id="contactSupportBtn">Contact Support</button>
          <p id="loginStatus"></p>
          <div id="supportInfo">
              <p>Email: support@medimate.com</p>
              <p>Phone: +1-555-123-4567</p>
          </div>
      </div>
      <script>
          const loginButton = document.getElementById('loginButton');
          const loginStatus = document.getElementById('loginStatus');
          const contactSupportBtn = document.getElementById('contactSupportBtn');
          const supportInfo = document.getElementById('supportInfo');
          const body = document.body;

          loginButton.addEventListener('click', function() {
              const username = document.getElementById('username').value;
              const password = document.getElementById('password').value;
              fetch('/login', {
                  method: 'POST',
                  headers: { 'Content-Type': 'application/json' },
                  body: JSON.stringify({ username, password })
              })
              .then(response => response.json())
              .then(data => {
                  if (data.success) {
                      window.location.href = '/dashboard'; // Redirect to dashboard
                  } else {
                      loginStatus.innerText = data.message;
                  }
              })
              .catch(error => {
                  loginStatus.innerText = 'Error: ' + error;
              });
          });

          contactSupportBtn.addEventListener('click', function() {
              supportInfo.style.display = supportInfo.style.display === 'none' ? 'block' : 'none';
          });

          // Dark Mode Toggle Logic (applied to login page as well)
          document.addEventListener('DOMContentLoaded', function() {
              const themeToggleButton = document.createElement('button');
              themeToggleButton.id = 'themeToggleButton';
              themeToggleButton.innerText = 'Toggle Dark Mode';
              themeToggleButton.style.position = 'fixed';
              themeToggleButton.style.bottom = '20px';
              themeToggleButton.style.left = '20px';
              document.body.appendChild(themeToggleButton);

              themeToggleButton.addEventListener('click', function() {
                  body.classList.toggle('dark-mode');
                  // You could also save the user's preference to local storage here
                  localStorage.setItem('theme', body.classList.contains('dark-mode') ? 'dark' : 'light');
              });

              // Check for saved preference on load and apply theme
              const savedTheme = localStorage.getItem('theme');
              if (savedTheme === 'dark') {
                  body.classList.add('dark-mode');
              }
          });
      </script>
  </body>
  </html>
)rawliteral";

// ==== Pin Configuration ====
#define BUZZER_PIN 13
#define IR_SENSOR_PIN 32
#define SERVO_1_PIN 26
#define SERVO_2_PIN 27
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define DOUT 33
#define SCK 25

HX711 scale;
float calibration_factor = 250;
float tareWeight = 0;
float weight;

RTC_DS3231 rtc;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
Servo servo1;
Servo servo2;
// Duration to rotate 360° (calibrate this for your motor)
int rotationTime = 1000;  // in milliseconds, adjust based on your servo's speed
WebServer server(80);

// ==== WiFi Configuration ====
const char* ssid = "Alen";
const char* password = "12345678";

// ==== Schedule ====
int alarm1Hour = 19, alarm1Minute = 36;
int alarm2Hour = 19, alarm2Minute = 37;
bool alarm1Triggered = false;
bool alarm2Triggered = false;
bool alarm1FingerprintVerified = false;
bool alarm2FingerprintVerified = false;
bool pill1Dispensed = false;
bool pill2Dispensed = false;

// ==== Function Declarations ====
void displayTime(DateTime now);
void displayMessage(String msg);
bool fingerprintVerified();
bool checkFingerprint(); // New function for checking fingerprint
bool checkPillDispensed(int irPin){
  int sensorValue = digitalRead(irPin);
  Serial.print("IR Sensor Value: ");
  Serial.println(sensorValue);
  return sensorValue == HIGH; // Assuming LOW indicates pill presence with TCRT5000
}
void rotateServo(Servo &servo, int angle); // Modified to take angle
void checkIRSensor(); // Might not be needed separately now
void showTemperature();
void handleSetTime();
void serveWebPage();
void displayWeight();

// ==== SERVO CONTROL FUNCTIONS ====
void rotateServo1(int angle) {
    servo1.write(angle);
}

void rotateServo2(int angle) {
    servo2.write(angle);
}

// --- MOVE handleSaveInfo() HERE, BEFORE setup() ---
void handleSaveInfo() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    DynamicJsonDocument doc(256); // Adjust size as needed
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      server.send(400, "application/json", "{\"message\": \"Invalid JSON data\"}");
      return;
    }

    String name = doc["name"].as<String>();
    String age = doc["age"].as<String>();
    String gender = doc["gender"].as<String>();

    preferences.putString("name", name);
    preferences.putString("age", age);
    preferences.putString("gender", gender);

    server.send(200, "application/json", "{\"message\": \"Personal information saved successfully\"}");
  } else {
    server.send(400, "application/json", "{\"message\": \"Invalid data\"}");
  }
}

void handleEnrollFingerprint() {
  displayMessage("Place finger now");
  delay(5000); // Give user time to place finger

  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    server.send(200, "application/json", "{\"message\": \"Error getting image\"}");
    return;
  }

  p = finger.image2Tz(1); // Template slot 1
  if (p != FINGERPRINT_OK) {
    server.send(200, "application/json", "{\"message\": \"Error creating template 1\"}");
    return;
  }

  displayMessage("Place finger again");
  delay(2000); // Give user time to place finger

  p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    server.send(200, "application/json", "{\"message\": \"Error getting image 2\"}");
    return;
  }

  p = finger.image2Tz(2); // Template slot 2
  if (p != FINGERPRINT_OK) {
    server.send(200, "application/json", "{\"message\": \"Error creating template 2\"}");
    return;
  }

  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    server.send(200, "application/json", "{\"message\": \"Error creating model\"}");
    return;
  }

  p = finger.storeModel(1); // Store in location 1
  if (p != FINGERPRINT_OK) {
    server.send(200, "application/json", "{\"message\": \"Error storing model\"}");
    return;
  }

  server.send(200, "application/json", "{\"message\": \"Fingerprint enrolled successfully\"}");
  displayMessage("Enrollment done!");
}

void handleScanFingerprint() {
  displayMessage("Scan finger now");
  delay(5000); // Give user time to place finger

  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    server.send(200, "application/json", "{\"message\": \"Error getting image\"}");
    return;
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    server.send(200, "application/json", "{\"message\": \"Error creating template\"}");
    return;
  }

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    String message = "Found fingerprint #" + String(finger.fingerID) + " with confidence " + String(finger.confidence);
    server.send(200, "application/json", "{\"message\": \"" + message + "\"}");
    displayMessage(message);
  } else if (p == FINGERPRINT_NOTFOUND) {
    server.send(200, "application/json", "{\"message\": \"Fingerprint not found\"}");
    displayMessage("Fingerprint not found");
  } else {
    server.send(200, "application/json", "{\"message\": \"Error searching fingerprint\"}");
    displayMessage("Error scanning");
  }
}

// Add these new functions here:
void handleLogin() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    DynamicJsonDocument doc(256); // Use JsonDocument instead of DynamicJsonDocument
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      server.send(400, "application/json", "{\"success\": false, \"message\": \"Invalid JSON\"}");
      return;
    }

    String username = doc["username"].as<String>();
    String password = doc["password"].as<String>();

    if (username == correctUsername && password == correctPassword) {
      isLoggedIn = true;
      server.send(200, "application/json", "{\"success\": true}");
    } else {
      server.send(200, "application/json", "{\"success\": false, \"message\": \"Invalid credentials\"}");
    }
  } else {
    server.send(400, "application/json", "{\"success\": false, \"message\": \"No data received\"}");
  }
}

void handleDashboard() {
  if (isLoggedIn) {
    serveWebPage(); // Serve the main dashboard page
  } else {
    server.send(401, "text/plain", "Unauthorized"); // Send unauthorized if not logged in
  }
}
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!rtc.begin()) {
    Serial.println("RTC not found");
    while (1);
  }

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
    while (1);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("Initializing...");
  display.display();

  if (!mlx.begin()) {
    Serial.println("MLX90614 not detected. Check wiring!");
    while (1);
  } else {
    Serial.println("MLX90614 detected and ready.");
  }

  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor ready");
    finger.LEDcontrol(true);
  } else {
    Serial.println("Fingerprint sensor FAILED");
    while (1);
  }

  //servo1.attach(26);
  //servo2.attach(27);
      // Rotate Servo 1 clockwise for 360°
      //Serial.println("Servo 1 rotating 360°...");
      //servo1.write(180);  // Max speed CW
      //delay(rotationTime);
      //servo1.write(90);   // Stop
      //Serial.println("Servo 1 stopped.");

      // Wait before starting Servo 2
      //delay(5000);

      // Rotate Servo 2 clockwise for 360°
      //Serial.println("Servo 2 rotating 360°...");
      //servo2.write(180);  // Max speed CW
      //delay(rotationTime);
      //servo2.write(90);   // Stop
      //Serial.println("Servo 2 stopped.");
  //servo1.write(90);
  //servo2.write(90);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);

  displayMessage("System Ready");
  delay(1000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/set-time", HTTP_POST, handleSetTime);
  server.on("/enroll-fingerprint", HTTP_POST, handleEnrollFingerprint);
  server.on("/scan-fingerprint", HTTP_POST, handleScanFingerprint);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/dashboard", HTTP_GET, handleDashboard);
  server.on("/save-info", HTTP_POST, handleSaveInfo);
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", loginPage);
  });
  server.begin();
  Serial.println("Server started");

  preferences.begin("user_info");

  // Initialize HX711
  scale.begin(DOUT, SCK);
  scale.set_scale(calibration_factor);
  scale.tare();
  Serial.println("HX711 Tare Completed");
}

void loop() {
  server.handleClient();
  float weight = scale.get_units(10); // Declare 'weight' locally in loop()
  DateTime now = rtc.now();

  // Check for Temperature Scan
  float objectTemp = mlx.readObjectTempC();

  // Display Real Time, Date, and Temperature on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 10);
  display.printf("Time: %02d:%02d:%02d", now.hour(), now.minute(), now.second());
  display.setCursor(10, 20);
  display.printf("Date: %04d-%02d-%02d", now.year(), now.month(), now.day());
  display.setCursor(10, 30);
  display.printf("Temp: %.2fC", objectTemp);
  display.display();

  // Check Alarm 1
  if (now.hour() == alarm1Hour && now.minute() == alarm1Minute && !alarm1Triggered) {
      Serial.println("Alarm 1 Triggered");
      tone(BUZZER_PIN, 1000, 3000); // 3-second buzzer
      alarm1Triggered = true;
      displayMessage("Scan Fingerprint for Pill 1");
      delay(5000);
      alarm1FingerprintVerified = checkFingerprint();
  }

  if (alarm1Triggered && alarm1FingerprintVerified && !pill1Dispensed) {
        pill1Dispensed = true;
        servo1.attach(26);
      Serial.println("Fingerprint 1 Verified - Dispensing Pill 1");
      rotateServo1(180); // Rotate Servo 1 to dispensing angle
      delay(1000);
      delay(1000);
      rotateServo1(90);  // Rotate back to default
      pill1Dispensed = checkPillDispensed(IR_SENSOR_PIN);
      if (pill1Dispensed) {
          displayMessage("Pill 1 Ready");
      } else {
          displayMessage("Pill 1 Dispense Failed");
      }
      alarm1FingerprintVerified = false; // Reset for next alarm
      servo1.detach();
  }

  if (now.hour() > alarm1Hour || (now.hour() == alarm1Hour && now.minute() > alarm1Minute)) {
      alarm1Triggered = false;
      pill1Dispensed = false; // Reset dispense status for the day
  }

  // Check Alarm 2
  if (now.hour() == alarm2Hour && now.minute() == alarm2Minute && !alarm2Triggered) {
      Serial.println("Alarm 2 Triggered");
      tone(BUZZER_PIN, 1000, 3000); // 3-second buzzer
      alarm2Triggered = true;
      displayMessage("Scan Fingerprint for Pill 2");
      delay(5000);
      alarm2FingerprintVerified = checkFingerprint();
  }

  if (alarm2Triggered && alarm2FingerprintVerified && !pill2Dispensed) {
    pill2Dispensed = true;
    servo2.attach(27);
      Serial.println("Fingerprint 2 Verified - Dispensing Pill 2");
      rotateServo2(180); // Rotate Servo 2 to dispensing angle
      delay(1000);
      delay(1000);
      rotateServo2(90);  // Rotate back to default
      pill2Dispensed = checkPillDispensed(IR_SENSOR_PIN);
      if (pill2Dispensed) {
          displayMessage("Pill 2 Ready");
      } else {
          displayMessage("Pill 2 Dispense Failed");
      }
      alarm2FingerprintVerified = false; // Reset for next alarm
      servo2.detach();
  }

  if (now.hour() > alarm2Hour || (now.hour() == alarm2Hour && now.minute() > alarm2Minute)) {
      alarm2Triggered = false;
      pill2Dispensed = false; // Reset dispense status for the day
  }

  // Display Weight
  displayWeight();
}

void handleSetTime() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    Serial.println("Received Body: " + body);

    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      server.send(400, "application/json", "{\"message\": \"Invalid JSON data\"}");
      return; // Exit the function if JSON parsing fails
    }

    String pill1Time = doc["pill1Time"].as<String>();
    String pill2Time = doc["pill2Time"].as<String>();
    // Assuming you also send the current date (year, month, day) or want to keep the RTC's current date
    //DateTime now = rtc.now();
    //int year = now.year();
    //int month = now.month();
    //int day = now.day();

    alarm1Hour = pill1Time.substring(0, 2).toInt();
    alarm1Minute = pill1Time.substring(3, 5).toInt();

    alarm2Hour = pill2Time.substring(0, 2).toInt();
    alarm2Minute = pill2Time.substring(3, 5).toInt();

    Serial.printf("Pill 1 Time Set: %02d:%02d\n", alarm1Hour, alarm1Minute);
    Serial.printf("Pill 2 Time Set: %02d:%02d\n", alarm2Hour, alarm2Minute);

    // Update the RTC with the new time (keeping the current date)
    //rtc.adjust(DateTime(year, month, day, alarm1Hour, alarm1Minute, 0)); // You might want a separate endpoint for setting the actual system time

    server.send(200, "application/json", "{\"message\": \"Times updated successfully\"}");
  } else {
    server.send(400, "application/json", "{\"message\": \"Invalid data\"}");
  }
}
void serveWebPage() {
  String html = R"rawliteral(
      <!DOCTYPE html>
      <html>
      <head>
          <title>MediMate: Set Pill Dispensing Time</title>
          <style id="theme-style">
              body {
                  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
                  background-color: #f0f8ff; /* Default light blue */
                  color: #212121; /* Default dark text */
                  text-align: center;
                  margin-top: 50px;
                  transition: background-color 0.3s ease, color 0.3s ease;
              }
              h1 {
                  color: #1976d2;
                  margin-bottom: 30px;
              }
              form {
                  background-color: #ffffff;
                  padding: 30px;
                  border-radius: 12px;
                  box-shadow: 0 6px 12px rgba(0, 0, 0, 0.15);
                  display: inline-block;
                  max-width: 400px;
                  width: 90%;
                  margin-bottom: 20px;
              }
              label {
                  font-weight: 600;
                  color: #263238;
                  display: block;
                  margin-bottom: 8px;
                  text-align: left;
              }
              input[type="time"], input[type="text"] {
                  width: calc(100% - 16px);
                  padding: 12px;
                  margin-bottom: 20px;
                  border: 2px solid #bbdefb;
                  border-radius: 8px;
                  font-size: 16px;
                  background-color: #ffffff; /* Default white input */
                  color: #212121; /* Default dark input text */
                  transition: background-color 0.3s ease, border-color 0.3s ease, color 0.3s ease;
              }
              .button-container {
                  display: flex;
                  justify-content: center;
                  flex-wrap: wrap;
                  margin-top: 20px;
              }
              button, .fingerprint-btn, .troubleshoot-btn, #personalInfoBtn, #saveInfoBtn {
                  background-color: #2196f3;
                  color: white;
                  padding: 14px 25px;
                  border: none;
                  border-radius: 8px;
                  cursor: pointer;
                  font-size: 16px;
                  transition: background-color 0.3s ease;
                  margin: 5px;
              }
              button:hover, .fingerprint-btn:hover, .troubleshoot-btn:hover, #personalInfoBtn:hover, #saveInfoBtn:hover {
                  background-color: #1565c0;
              }
              p#status {
                  color: #4caf50;
                  margin-top: 20px;
                  font-weight: 500;
              }
              .troubleshoot-btn {
                  background-color: #ffc107;
                  color: #212121;
              }
              .troubleshoot-btn:hover {
                  background-color: #ffb300;
              }
              #troubleshooting {
                  display: none;
                  text-align: left;
                  margin: 30px auto;
                  width: 80%;
                  max-width: 600px;
                  background-color: #e3f2fd;
                  padding: 20px;
                  border-radius: 8px;
                  box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
                  color: #212121; /* Default dark text in troubleshooting */
                  transition: background-color 0.3s ease, color 0.3s ease;
              }
              #troubleshooting h2 {
                  color: #1976d2;
                  margin-bottom: 15px;
              }
              #troubleshooting ul {
                  list-style-type: disc;
                  padding-left: 20px;
              }
              #troubleshooting li {
                  color: #2e7d32;
                  margin-bottom: 8px;
              }
              .fingerprint-btn {
                  background-color: #4db6ac;
              }
              .fingerprint-btn:hover {
                  background-color: #009688;
              }
              #personalInfo {
                  display: none;
                  position: absolute;
                  top: 10px;
                  right: 10px;
                  background-color: #e3f2fd;
                  padding: 10px;
                  border-radius: 5px;
                  color: #212121; /* Default dark text in personal info */
                  transition: background-color 0.3s ease, color 0.3s ease;
              }
              #personalInfo label {
                  color: #263238;
              }
              #personalInfo input[type="text"] {
                  background-color: #ffffff;
                  color: #212121;
              }
              /* Dark Theme Styles */
              .dark-mode body {
                  background-color: #303030;
                  color: #f5f5f5;
              }
              .dark-mode h1 {
                  color: #90caf9;
              }
              .dark-mode form {
                  background-color: #424242;
                  box-shadow: 0 4px 8px rgba(0, 0, 0, 0.5);
              }
              .dark-mode label {
                  color: #e0e0e0;
              }
              .dark-mode input[type="time"], .dark-mode input[type="text"] {
                  background-color: #616161;
                  color: #f5f5f5;
                  border-color: #757575;
              }
              .dark-mode #troubleshooting {
                  background-color: #424242;
                  box-shadow: 0 4px 8px rgba(0, 0, 0, 0.5);
                  color: #f5f5f5;
              }
              .dark-mode #troubleshooting h2 {
                  color: #90caf9;
              }
              .dark-mode #troubleshooting ul {
                  list-style-type: disc;
                  padding-left: 20px;
              }
              .dark-mode #troubleshooting li {
                  color: #aed581;
              }
              .dark-mode #personalInfo {
                  background-color: #424242;
                  box-shadow: 0 4px 8px rgba(0, 0, 0, 0.5);
                  color: #f5f5f5;
              }
              .dark-mode #personalInfo label {
                  color: #e0e0e0;
              }
              .dark-mode #personalInfo input[type="text"] {
                  background-color: #616161;
                  color: #f5f5f5;
                  border-color: #757575;
              }
              #themeToggleButton {
                  position: fixed;
                  bottom: 20px;
                  left: 20px;
                  background-color: #2196f3;
                  color: white;
                  padding: 14px 25px;
                  border: none;
                  border-radius: 8px;
                  cursor: pointer;
                  font-size: 16px;
                  transition: background-color 0.3s ease;
                  z-index: 1000; /* Ensure it's above other elements */
              }
              #themeToggleButton:hover {
                  background-color: #1565c0;
              }
          </style>
      </head>
      <body>
          <h1>MediMate: Set Pill Dispensing Time</h1>
          <form id="timeForm">
              <label for="pill1Time">Pill 1 Time:</label>
              <input type="time" id="pill1Time" required />
              <br />
              <label for="pill2Time">Pill 2 Time:</label>
              <input type="time" id="pill2Time" required />
              <br />
              <button type="submit">Set Time</button>
          </form>
          <div class="button-container">
              <button class="fingerprint-btn" id="enrollButton">Enroll Fingerprint</button>
              <button class="fingerprint-btn" id="scanButton">Scan Fingerprint</button>
              <button class="troubleshoot-btn" id="troubleshootButton">Troubleshooting Tips</button>
              <button id="personalInfoBtn">Personal Info</button>
          </div>
          <p id="status"></p>
          <div id="troubleshooting">
              <h2>Troubleshooting</h2>
              <ul>
                  <li><strong>WiFi Connection:</strong> Ensure your ESP32 is connected to the correct WiFi network.</li>
                  <li><strong>Power Supply:</strong> Verify that the power supply is stable and sufficient.</li>
                  <li><strong>Sensor Wiring:</strong> Double-check the wiring of all sensors (RTC, MLX90614, Fingerprint).</li>
                  <li><strong>Servo Movement:</strong> Ensure servos are correctly attached and not obstructed.</li>
                  <li><strong>RTC Time:</strong> If the time is incorrect, consider re-syncing the RTC.</li>
                  <li><strong>Fingerprint Sensor:</strong> Make sure the sensor's LED is lit and fingerprints are registered properly.</li>
              </ul>
          </div>
          <div id="personalInfo">
              <label for="name">Name:</label>
              <input type="text" id="name" />
              <br />
              <label for="age">Age:</label>
              <input type="text" id="age" />
              <br />
              <label for="gender">Gender:</label>
              <input type="text" id="gender" />
              <br />
              <button id="saveInfoBtn">Save Info</button>
          </div>
          <button id="themeToggleButton">Toggle Dark Mode</button>
          <script>
              document.getElementById('timeForm').addEventListener('submit', function(event) {
                  event.preventDefault();
                  const pill1Time = document.getElementById('pill1Time').value;
                  const pill2Time = document.getElementById('pill2Time').value;
                  fetch('/set-time', {
                      method: 'POST',
                      headers: { 'Content-Type': 'application/json' },
                      body: JSON.stringify({ pill1Time, pill2Time })
                  })
                  .then(response => response.json())
                  .then(data => {
                      document.getElementById('status').innerText = data.message;
                  })
                  .catch(error => {
                      document.getElementById('status').innerText = 'Error: ' + error;
                  });
              });
              document.getElementById('troubleshootButton').addEventListener('click', function() {
                  var troubleshootingDiv = document.getElementById('troubleshooting');
                  troubleshootingDiv.style.display = troubleshootingDiv.style.display === 'none' ? 'block' : 'none';
              });
              document.getElementById('enrollButton').addEventListener('click', function() {
                  fetch('/enroll-fingerprint', { method: 'POST' })
                  .then(response => response.json())
                  .then(data => { document.getElementById('status').innerText = data.message; })
                  .catch(error => { document.getElementById('status').innerText = 'Error: ' + error; });
              });
              document.getElementById('scanButton').addEventListener('click', function() {
                  fetch('/scan-fingerprint', { method: 'POST' })
                  .then(response => response.json())
                  .then(data => { document.getElementById('status').innerText = data.message; })
                  .catch(error => { document.getElementById('status').innerText = 'Error: ' + error; });
              });
              document.getElementById('saveInfoBtn').addEventListener('click', function() {
                  const name = document.getElementById('name').value;
                  const age = document.getElementById('age').value;
                  const gender = document.getElementById('gender').value;
                  fetch('/save-info', {
                      method: 'POST',
                      headers: { 'Content-Type': 'application/json' },
                      body: JSON.stringify({ name, age, gender })
                  })
                  .then(response => response.json())
                  .then(data => {
                      document.getElementById('status').innerText = data.message;
                  })
                  .catch(error => {
                      document.getElementById('status').innerText = 'Error: ' + error;
                  });
              });
              document.getElementById('personalInfoBtn').addEventListener('click', function() {
                  var personalInfoDiv = document.getElementById('personalInfo');
                  personalInfoDiv.style.display = personalInfoDiv.style.display === 'none' ? 'block' : 'none';
              });

              // Dark Mode Toggle Logic (Now with fixed positioning)
              document.addEventListener('DOMContentLoaded', function() {
                  const themeToggleButton = document.getElementById('themeToggleButton');
                  const body = document.body;

                  themeToggleButton.addEventListener('click', function() {
                      body.classList.toggle('dark-mode');
                      localStorage.setItem('theme', body.classList.contains('dark-mode') ? 'dark' : 'light');
                  });

                  const savedTheme = localStorage.getItem('theme');
                  if (savedTheme === 'dark') {
                      body.classList.add('dark-mode');
                  }
              });
          </script>
      </body>
      </html>
  )rawliteral";
  server.send(200, "text/html", html);
}
bool checkFingerprint() {
    Serial.println("Waiting for finger...");
    while (finger.getImage() != FINGERPRINT_OK);
    Serial.println("Image taken");
    if (finger.image2Tz() != FINGERPRINT_OK) return false;
    Serial.println("Templated");
    if (finger.fingerSearch() == FINGERPRINT_OK) {
        Serial.print("Found fingerprint #");
        Serial.print(finger.fingerID);
        Serial.print(" with confidence ");
        Serial.println(finger.confidence);
        return true;
    } else {
        Serial.println("Did not find match!");
        return false;
    }
}

void displayMessage(String msg) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println(msg);
    display.display();
    Serial.println("OLED: " + msg);
}

void displayWeight() {
  long reading = scale.get_units(10); // Get average of 10 readings
  float displayedWeight = reading * 10.0; // Multiply the reading by 10

  display.setTextSize(1);
  display.setCursor(10, 40);
  display.print("Weight: ");
  display.print(displayedWeight); // Use the multiplied weight for display
  display.println(" g");
  display.display();
  Serial.print("Weight: ");
  Serial.print(displayedWeight); // Use the multiplied weight for serial output
  Serial.println(" g");
}

