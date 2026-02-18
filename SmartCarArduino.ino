#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);
WiFiServer tcpServer(5204);
WiFiClient processingClient;

// IR Receiver Setup
const int irReceiverPin = 34;
volatile unsigned long irValue = 0;
volatile bool irReceived = false;

// System State
bool systemEnabled = false;
bool carLocked = true;

// pin definitions...
int driveEnable = 2;
int driveForward = 4;
int driveBackward = 16;
int steerEnable = 5;
int steerLeft = 18;
int steerRight = 19;
int frontLeftLED = 13;
int frontRightLED = 12;
int rearLeftLED = 27;
int rearRightLED = 14;
int trigPin = 22;
int echoPin = 21;
int buzzer = 26;
int thermistorPin = 35;
int waterLevelPin = 32;

// Car parameters
float currentSpeed = 0;
float totalDistance = 0;
long encoderCount = 0;
int distance = 999;
int safeDistance = 100;
float oilTemperature = 0;
int fuelLevel = 0;

// Thermistor constants
const int seriesResistor = 10000;
const int adcResolution = 4095;
const float thermistorNominal = 10000.0;
const float temperatureNominal = 25.0;
const float bCoefficient = 3950.0;

// Ultrasonic variables
unsigned long lastUltrasonicTime = 0;
const unsigned long ultrasonicInterval = 200; // ms between readings

// Buzzer timing variables
unsigned long lastBeepTime = 0;

// IR INTERRUPT FUNCTION
void IRAM_ATTR irInterrupt() {
  static unsigned long lastPulse = 0;
  static int bitPosition = 0;
  static unsigned long code = 0;
  
  unsigned long currentTime = micros();
  unsigned long pulseDuration = currentTime - lastPulse;
  
  if (pulseDuration > 5000) { // Start of frame
    if (bitPosition >= 12) { // Minimum valid code length
      irValue = code;
      irReceived = true;
    }
    bitPosition = 0;
    code = 0;
  } else if (pulseDuration > 1000) { // Logical 1
    code |= (1UL << bitPosition);
    bitPosition++;
  } else if (pulseDuration > 300) { // Logical 0
    bitPosition++;
  }
  
  lastPulse = currentTime;
}

// SIMPLIFIED ultrasonic function
int getDistance() {
  // Send pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Read echo with timeout
  long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout
  
  if (duration == 0) {
    return 999; // No echo detected
  }
  
  int distance = duration * 0.0343 / 2;
  
  // Validate distance (ultrasonic typically works from 2cm to 400cm)
  if (distance < 2 || distance > 400) {
    return 999;
  }
  
  return distance;
}

// Thermistor reading function
float readThermistor() {
  int adcValue = analogRead(thermistorPin);
  float voltage = adcValue * (3.3 / adcResolution);
  float resistance = seriesResistor * (3.3 / voltage - 1);
  float steinhart = resistance / thermistorNominal;
  steinhart = log(steinhart);
  steinhart /= bCoefficient;
  steinhart += 1.0 / (temperatureNominal + 273.15);
  steinhart = 1.0 / steinhart;
  steinhart -= 273.15;
  return steinhart;
}

// Water level reading function
int readWaterLevel() {
  int sensorValue = analogRead(waterLevelPin);
  int waterLevel = map(sensorValue, 0, 4095, 0, 100);
  waterLevel = constrain(waterLevel, 0, 100);
  return waterLevel;
}

// SIMPLIFIED parking alert function
void handleParkingAlert() {
  if (!systemEnabled) {
    noTone(buzzer);
    return;
  }
  
  unsigned long currentMillis = millis();
  
  // Only beep for valid distances between 2cm and 100cm
  if (distance >= 2 && distance <= 100) {
    unsigned long beepInterval;
    
    if (distance <= 10) {
      beepInterval = 200; // Fast beeping
    } else if (distance <= 30) {
      beepInterval = 400; // Medium beeping
    } else {
      beepInterval = 800; // Slow beeping
    }
    
    if (currentMillis - lastBeepTime >= beepInterval) {
      lastBeepTime = currentMillis;
      tone(buzzer, 1500, 100);
    }
  } else {
    noTone(buzzer);
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(irReceiverPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(irReceiverPin), irInterrupt, CHANGE);
  Serial.println("IR Receiver Ready! Power button: 0x55544045");
  
  pinMode(driveEnable, OUTPUT);
  pinMode(driveForward, OUTPUT);
  pinMode(driveBackward, OUTPUT);
  pinMode(steerEnable, OUTPUT);
  pinMode(steerLeft, OUTPUT);
  pinMode(steerRight, OUTPUT);
  pinMode(frontLeftLED, OUTPUT);
  pinMode(frontRightLED, OUTPUT);
  pinMode(rearLeftLED, OUTPUT);
  pinMode(rearRightLED, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(thermistorPin, INPUT);
  pinMode(waterLevelPin, INPUT);
  
  // Initialize outputs to LOW
  digitalWrite(driveEnable, LOW);
  digitalWrite(steerEnable, LOW);
  digitalWrite(driveForward, LOW);
  digitalWrite(driveBackward, LOW);
  digitalWrite(steerLeft, LOW);
  digitalWrite(steerRight, LOW);
  digitalWrite(frontLeftLED, LOW);
  digitalWrite(frontRightLED, LOW);
  digitalWrite(rearLeftLED, LOW);
  digitalWrite(rearRightLED, LOW);
  digitalWrite(trigPin, LOW);
  noTone(buzzer);
  
  // Create WiFi
  WiFi.softAP("SmartCar", "12345678");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
  
  // Web interface - shows system status
  server.on("/", []() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<style>";
    html += "body { margin: 0; padding: 20px; background: #1a1a2e; color: white; font-family: Arial; text-align: center; }";
    html += ".status { background: " + String(systemEnabled ? "#27ae60" : "#e74c3c") + "; padding: 15px; border-radius: 10px; margin: 20px 0; }";
    html += ".control-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; max-width: 500px; margin: 0 auto; }";
    html += ".steer-section, .drive-section { background: #0f3460; padding: 20px; border-radius: 15px; }";
    html += ".button-column { display: flex; flex-direction: column; align-items: center; gap: 15px; margin: 15px 0; }";
    html += ".button-row { display: flex; justify-content: center; gap: 15px; margin: 15px 0; }";
    html += ".steer-btn { width: 100px; height: 80px; font-size: 18px; background: #e94560; color: white; border: none; border-radius: 10px; }";
    html += ".drive-btn { width: 120px; height: 80px; font-size: 20px; background: #3498db; color: white; border: none; border-radius: 10px; }";
    html += "h2 { margin: 10px 0; color: #4ecca3; }";
    html += ".sensor-data { background: #2c3e50; padding: 15px; border-radius: 10px; margin: 10px 0; }";
    html += "</style>";
    html += "</head><body>";
    
    html += "<h1>SmartCar Control</h1>";
    
    html += "<div class='status'>";
    html += "<h3>System: " + String(systemEnabled ? "ENABLED" : "DISABLED") + "</h3>";
    html += "<p>IR Remote: Press Power button (0x55544045) to toggle system</p>";
    html += "</div>";
    
    
    // Only show controls if system is enabled
    if (systemEnabled) {
      html += "<div class='control-grid'>";
      
      // STEERING SECTION
      html += "<div class='steer-section'>";
      html += "<h2>STEERING</h2>";
      html += "<div class='button-row'>";
      html += "<button class='steer-btn' ontouchstart='control(\"left\")' ontouchend='stopSteer()'>LEFT</button>";
      html += "<button class='steer-btn' ontouchstart='control(\"right\")' ontouchend='stopSteer()'>RIGHT</button>";
      html += "</div>";
      html += "</div>";
      
      // DRIVE SECTION
      html += "<div class='drive-section'>";
      html += "<h2>DRIVE</h2>";
      html += "<div class='button-column'>";
      html += "<button class='drive-btn' ontouchstart='control(\"forward\")' ontouchend='stopDrive()'>FORWARD</button>";
      html += "<button class='drive-btn' ontouchstart='control(\"backward\")' ontouchend='stopDrive()'>BACKWARD</button>";
      html += "</div>";
      html += "</div>";
      
      html += "</div>";
    } else {
      html += "<div style='background: #34495e; padding: 20px; border-radius: 10px; margin: 20px 0;'>";
      html += "<h3>System Locked</h3>";
      html += "<p>Use IR Remote Power button to enable the system</p>";
      html += "</div>";
    }
    
    html += "<script>";
    html += "function control(command) {";
    html += "  var xhr = new XMLHttpRequest();";
    html += "  if (command == 'forward' || command == 'backward') {";
    html += "    xhr.open('GET', '/control?drive=' + command + '&speed=80', true);";
    html += "  } else {";
    html += "    xhr.open('GET', '/control?steering=' + command, true);";
    html += "  }";
    html += "  xhr.send();";
    html += "}";
    html += "";
    html += "function stopDrive() {";
    html += "  var xhr = new XMLHttpRequest();";
    html += "  xhr.open('GET', '/control?drive=stop', true);";
    html += "  xhr.send();";
    html += "}";
    html += "";
    html += "function stopSteer() {";
    html += "  var xhr = new XMLHttpRequest();";
    html += "  xhr.open('GET', '/control?steering=stop', true);";
    html += "  xhr.send();";
    html += "}";
    html += "</script>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
  });
  
  server.on("/control", []() {
    // If system is disabled, ignore commands
    if (!systemEnabled) {
      server.send(200, "text/plain", "SYSTEM_DISABLED");
      return;
    }
    
    // Turn off all LEDs first (clean state)
    digitalWrite(frontLeftLED, LOW);
    digitalWrite(frontRightLED, LOW);
    digitalWrite(rearLeftLED, LOW);
    digitalWrite(rearRightLED, LOW);
    
    // Handle steering commands
    if (server.hasArg("steering")) {
      String steering = server.arg("steering");
      Serial.print("Steering: ");
      Serial.println(steering);
      
      if (steering == "left") {
        digitalWrite(steerLeft, HIGH);
        digitalWrite(steerRight, LOW);
        analogWrite(steerEnable, 200);
        // Turn on left indicators
        digitalWrite(frontLeftLED, HIGH);
        digitalWrite(rearLeftLED, HIGH);
        currentSpeed = 5.0;
        Serial.println("Turning LEFT - Left indicators ON");
      }
      else if (steering == "right") {
        digitalWrite(steerLeft, LOW);
        digitalWrite(steerRight, HIGH);
        analogWrite(steerEnable, 200);
        // Turn on right indicators
        digitalWrite(frontRightLED, HIGH);
        digitalWrite(rearRightLED, HIGH);
        currentSpeed = 5.0;
        Serial.println("Turning RIGHT - Right indicators ON");
      }
      else if (steering == "stop") {
        digitalWrite(steerLeft, LOW);
        digitalWrite(steerRight, LOW);
        analogWrite(steerEnable, 0);
        currentSpeed = 0;
        Serial.println("Steering STOP - Indicators OFF");
      }
    }
    
    // Handle drive commands
    if (server.hasArg("drive")) {
      String drive = server.arg("drive");
      int speed = 200;
      
      Serial.print("Drive: ");
      Serial.println(drive);
      
      if (drive == "forward") {
        digitalWrite(driveForward, HIGH);
        digitalWrite(driveBackward, LOW);
        analogWrite(driveEnable, speed);
        // Turn on front lights
        digitalWrite(frontLeftLED, HIGH);
        digitalWrite(frontRightLED, HIGH);
        currentSpeed = 8.0;
        Serial.println("Driving FORWARD - Front lights ON");
      }
      else if (drive == "backward") {
        digitalWrite(driveForward, LOW);
        digitalWrite(driveBackward, HIGH);
        analogWrite(driveEnable, speed);
        // Turn on rear lights
        digitalWrite(rearLeftLED, HIGH);
        digitalWrite(rearRightLED, HIGH);
        currentSpeed = 6.0;
        Serial.println("Driving BACKWARD - Rear lights ON");
      }
      else if (drive == "stop") {
        digitalWrite(driveForward, LOW);
        digitalWrite(driveBackward, LOW);
        analogWrite(driveEnable, 0);
        currentSpeed = 0;
        Serial.println("Drive STOP - All lights OFF");
      }
    }
    
    server.send(200, "text/plain", "OK");
  });
  
  server.begin();
  tcpServer.begin();
  
  Serial.println("SmartCar Ready! Connect to: 192.168.4.1");
  Serial.println("Use IR Power button (0x55544045) to enable system");
  Serial.println("Testing ultrasonic sensor...");
  
  // Test ultrasonic sensor on startup
  for (int i = 0; i < 3; i++) {
    int testDist = getDistance();
    Serial.print("Ultrasonic test ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(testDist);
    Serial.println("cm");
    delay(500);
  }
}

// SIMPLE IR COMMAND HANDLER - ONLY POWER BUTTON
void handleIRCommand(unsigned long value) {
  Serial.print("IR Command: 0x");
  Serial.println(value, HEX);
  
  // ONLY handle the power button - ignore everything else
  if (value == 0x55544045) {  // YOUR POWER BUTTON CODE
    if (systemEnabled) {
      disableSystem();
    } else {
      enableSystem();
    }
  }
  // All other IR codes are ignored
}

void enableSystem() {
  systemEnabled = true;
  carLocked = false;
  
  Serial.println("=== SYSTEM ENABLED - Car Unlocked ===");
  
  // Car unlock sound (beep-beep)
  tone(buzzer, 1500, 200);
  delay(250);
  tone(buzzer, 1500, 200);
  delay(250);
  noTone(buzzer);
  
  // Turn on all LEDs briefly to show system ready
  digitalWrite(frontLeftLED, HIGH);
  digitalWrite(frontRightLED, HIGH);
  digitalWrite(rearLeftLED, HIGH);
  digitalWrite(rearRightLED, HIGH);
  delay(1000);
  
  // Turn off LEDs (system ready but lights off)
  digitalWrite(frontLeftLED, LOW);
  digitalWrite(frontRightLED, LOW);
  digitalWrite(rearLeftLED, LOW);
  digitalWrite(rearRightLED, LOW);
  
  Serial.println("Car ready to drive! Use web interface to control.");
}

void disableSystem() {
  systemEnabled = false;
  carLocked = true;
  
  Serial.println("=== SYSTEM DISABLED - Car Locked ===");
  
  // Stop all motors immediately
  digitalWrite(driveForward, LOW);
  digitalWrite(driveBackward, LOW);
  analogWrite(driveEnable, 0);
  digitalWrite(steerLeft, LOW);
  digitalWrite(steerRight, LOW);
  analogWrite(steerEnable, 0);
  
  // Turn off all LEDs
  digitalWrite(frontLeftLED, LOW);
  digitalWrite(frontRightLED, LOW);
  digitalWrite(rearLeftLED, LOW);
  digitalWrite(rearRightLED, LOW);
  
  // Car lock sound (one beep)
  tone(buzzer, 1200, 300);
  delay(500);
  noTone(buzzer);
  
  currentSpeed = 0;
  
  Serial.println("Car locked and powered down");
}

void loop() {
  server.handleClient();
  
  // Handle IR commands - ONLY power button
  if (irReceived) {
    handleIRCommand(irValue);
    irReceived = false;
  }
  
  // Update ultrasonic sensor every 200ms - SIMPLIFIED
  if (millis() - lastUltrasonicTime >= ultrasonicInterval) {
    distance = getDistance();
    lastUltrasonicTime = millis();
    
    // Debug output - only print when distance changes significantly
    static int lastPrintedDistance = -1;
    if (abs(distance - lastPrintedDistance) > 5 || distance == 999) {
      Serial.print("Distance: ");
      Serial.print(distance);
      Serial.println("cm");
      lastPrintedDistance = distance;
    }
  }
  
  // Handle parking alerts
  handleParkingAlert();
  
  // Your existing sensor and Processing client code...
  if (!processingClient || !processingClient.connected()) {
    processingClient = tcpServer.available();
    if (processingClient) {
      Serial.println("Processing client connected!");
    }
  }
  
  // Update sensors only if system is enabled
  if (systemEnabled) {
    // Update oil temperature
    static unsigned long lastTempRead = 0;
    static float tempSum = 0;
    static int tempReadings = 0;
    
    if (millis() - lastTempRead > 100) {
      tempSum += readThermistor();
      tempReadings++;
      lastTempRead = millis();
    }
    
    if (tempReadings >= 5) {
      oilTemperature = tempSum / tempReadings;
      tempSum = 0;
      tempReadings = 0;
    }
    
    // Update fuel level
    fuelLevel = readWaterLevel();
    
    // Send data to Processing
    if (processingClient && processingClient.connected()) {
      String data = "DISTANCE:" + String(totalDistance, 2) + 
                    ",ENCODER:" + String(encoderCount) +
                    ",ULTRASONIC:" + String(distance) +
                    ",TEMPERATURE:" + String(oilTemperature, 1) +
                    ",FUEL:" + String(fuelLevel) +
                    ",SYSTEM_ENABLED:" + String(systemEnabled ? "1" : "0") +
                    ",FRONT_LEFT:" + String(digitalRead(frontLeftLED)) +
                    ",FRONT_RIGHT:" + String(digitalRead(frontRightLED)) +
                    ",REAR_LEFT:" + String(digitalRead(rearLeftLED)) +
                    ",REAR_RIGHT:" + String(digitalRead(rearRightLED)) + "\n";
      processingClient.print(data);
    }
    
    // Simulate encoder data when moving
    if (currentSpeed > 0) {
      encoderCount += (int)(currentSpeed * 10);
      totalDistance += currentSpeed * 0.01;
    }
  } else {
    // System disabled - make sure buzzer is off
    noTone(buzzer);
  }
  
  delay(50);
}