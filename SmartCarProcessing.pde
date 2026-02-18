import processing.net.*;

Client myClient;
float totalDistance = 0;
long encoderCount = 0;
int ultrasonicDistance = 0;
float oilTemperature = 0;
int fuelLevel = 0;  // 0-100%
String statusMessage = "Enter ESP32 IP and press CONNECT";
String esp32IP = "192.168.4.1";
boolean connected = false;

// LED status variables
boolean frontLeftLED = false;
boolean frontRightLED = false;
boolean rearLeftLED = false;
boolean rearRightLED = false;

void setup() {
  size(1200, 800);
  smooth();
  connectToCar(); // Auto-connect to ESP32 AP
}

void draw() {
  background(0);
  
  if (connected) {
    // Read data from ESP32
    if (myClient.available() > 0) {
      String data = myClient.readString();
      if (data != null) {
        data = data.trim();
        parseData(data);
      }
    }
    
    // Draw UI
    drawLEDStatus();
    drawTemperatureDisplay();
    drawFuelGauge();
    drawRearParkingDisplay();
    drawDistance();
    drawEncoder();
    drawStatus();
    drawConnectionStatus();
  } else {
    drawConnectionScreen();
  }
}

void parseData(String data) {
  try {
    String[] parts = split(data, ',');
    for (String part : parts) {
      String[] keyValue = split(part, ':');
      if (keyValue.length == 2) {
        String key = keyValue[0].trim();
        String value = keyValue[1].trim();
        
        if (key.equals("DISTANCE")) {
          totalDistance = float(value);
        } else if (key.equals("ENCODER")) {
          encoderCount = Long.parseLong(value);
        } else if (key.equals("ULTRASONIC")) {
          ultrasonicDistance = int(value);
        } else if (key.equals("TEMPERATURE")) {
          oilTemperature = float(value);
        } else if (key.equals("FUEL")) {
          fuelLevel = int(value);
        } else if (key.equals("FRONT_LEFT")) {
          frontLeftLED = value.equals("1");
        } else if (key.equals("FRONT_RIGHT")) {
          frontRightLED = value.equals("1");
        } else if (key.equals("REAR_LEFT")) {
          rearLeftLED = value.equals("1");
        } else if (key.equals("REAR_RIGHT")) {
          rearRightLED = value.equals("1");
        }
      }
    }
    statusMessage = "Connected - Receiving data";
  } catch (Exception e) {
    println("Error parsing data: " + data);
  }
}

void drawFuelGauge() {
  float centerX = 600;
  float centerY = 220;
  float radius = 70;
  float startAngle = PI;
  float endAngle = TWO_PI;
  
  // Title
  fill(255);
  textSize(20);
  textAlign(CENTER, CENTER);
  text("FUEL LEVEL", centerX, centerY - 100);
  
  // Draw semi-circle base (gray background)
  fill(50);
  stroke(100);
  strokeWeight(2);
  arc(centerX, centerY, radius * 2, radius * 2, startAngle, endAngle, PIE);
  
  // Draw fuel level with gradient effect
  float fuelAngle = map(fuelLevel, 0, 100, startAngle, endAngle);
  noStroke();
  
  // Color changes based on fuel level
  if (fuelLevel <= 15) {
    fill(255, 0, 0, 200); // Red for low fuel
  } else if (fuelLevel <= 30) {
    fill(255, 165, 0, 200); // Orange for warning
  } else {
    fill(0, 200, 0, 200); // Green for normal
  }
  
  arc(centerX, centerY, radius * 2, radius * 2, startAngle, fuelAngle, PIE);
  
  // Draw gauge outline
  noFill();
  stroke(255);
  strokeWeight(3);
  arc(centerX, centerY, radius * 2, radius * 2, startAngle, endAngle);
  
  // Draw tick marks
  stroke(200);
  strokeWeight(1);
  for (int i = 0; i <= 100; i += 25) {
    float angle = map(i, 0, 100, startAngle, endAngle);
    float innerX = centerX + cos(angle) * (radius * 0.7);
    float innerY = centerY + sin(angle) * (radius * 0.7);
    float outerX = centerX + cos(angle) * (radius * 0.9);
    float outerY = centerY + sin(angle) * (radius * 0.9);
    line(innerX, innerY, outerX, outerY);
    
    // Draw percentage labels
    fill(200);
    textSize(12);
    textAlign(CENTER, CENTER);
    float labelX = centerX + cos(angle) * (radius * 1.15);
    float labelY = centerY + sin(angle) * (radius * 1.15);
    text(i + "%", labelX, labelY);
  }
  
  // Draw E and F markers with better positioning
  fill(255);
  textSize(18);
  textAlign(CENTER, CENTER);
  
  // E marker (left)
  text("E", centerX - radius - 20, centerY + 5);
  
  // F marker (right)
  text("F", centerX + radius + 20, centerY + 5);
  
  // Draw fuel percentage in center
  fill(255);
  textSize(24);
  text(fuelLevel + "%", centerX, centerY - 15);
  
  // Draw status text below gauge
  textSize(16);
  if (fuelLevel <= 15) {
    fill(255, 0, 0);
    text("LOW FUEL - REFUEL SOON!", centerX, centerY + 50);
  } else if (fuelLevel <= 30) {
    fill(255, 165, 0);
    text("FUEL LOW", centerX, centerY + 50);
  } else {
    fill(100, 255, 100);
    text("FUEL OK", centerX, centerY + 50);
  }
}

void drawTemperatureDisplay() {
  float centerX = 800;
  float centerY = 200;
  
  // Title
  fill(255);
  textSize(24);
  textAlign(CENTER, CENTER);
  text("OIL TEMPERATURE", centerX, centerY - 80);
  
  // Temperature value
  if (oilTemperature > 80) {
    fill(255, 0, 0); // Red for hot
  } else if (oilTemperature > 40) {
    fill(255, 165, 0); // Orange for normal
  } else {
    fill(0, 255, 255); // Cyan for cool
  }
  textSize(48);
  text(nf(oilTemperature, 0, 1) + " °C", centerX, centerY - 30);
  
  // Temperature gauge background
  noStroke();
  fill(50);
  rect(centerX - 150, centerY, 300, 40, 20);
  
  // Temperature gradient
  for (int i = 0; i < 300; i++) {
    float temp = map(i, 0, 300, 0, 120);
    if (temp <= 40) {
      fill(0, 255, 255, 200); // Cyan
    } else if (temp <= 80) {
      fill(255, 165, 0, 200); // Orange
    } else {
      fill(255, 0, 0, 200); // Red
    }
    rect(centerX - 150 + i, centerY, 1, 40);
  }
  
  // Temperature indicator
  float indicatorX = map(constrain(oilTemperature, 0, 120), 0, 120, centerX - 150, centerX + 150);
  stroke(255);
  strokeWeight(3);
  line(indicatorX, centerY - 10, indicatorX, centerY + 50);
  fill(255);
  triangle(indicatorX - 8, centerY - 10, indicatorX + 8, centerY - 10, indicatorX, centerY - 20);
  
  // Temperature scale
  fill(200);
  textSize(14);
  textAlign(CENTER, TOP);
  text("0°C", centerX - 150, centerY + 45);
  text("40°C", centerX - 50, centerY + 45);
  text("80°C", centerX + 50, centerY + 45);
  text("120°C", centerX + 150, centerY + 45);
  
  // Status indicator
  textSize(16);
  if (oilTemperature > 80) {
    fill(255, 0, 0);
    text("HOT - CHECK OIL", centerX, centerY + 80);
  } else if (oilTemperature > 40) {
    fill(255, 165, 0);
    text("NORMAL OPERATING TEMP", centerX, centerY + 80);
  } else {
    fill(0, 255, 255);
    text("COLD - WARMING UP", centerX, centerY + 80);
  }
}

void drawLEDStatus() {
  // Title
  fill(255);
  textSize(24);
  textAlign(CENTER, CENTER);
  text("CAR LIGHT STATUS", 300, 50);
  
  // Draw car outline
  stroke(100);
  strokeWeight(2);
  noFill();
  rect(150, 100, 300, 200, 20); // Car body
  
  // Front of car
  line(150, 100, 100, 50);
  line(450, 100, 500, 50);
  
  // Rear of car
  line(150, 300, 100, 350);
  line(450, 300, 500, 350);
  
  // Front LEDs
  drawLED(180, 80, frontLeftLED, color(255, 255, 0)); // Front Left - Yellow
  drawLED(420, 80, frontRightLED, color(255, 255, 0)); // Front Right - Yellow
  
  // Rear LEDs  
  drawLED(180, 320, rearLeftLED, color(255, 0, 0)); // Rear Left - Red
  drawLED(420, 320, rearRightLED, color(255, 0, 0)); // Rear Right - Red
  
  // Labels
  fill(200);
  textSize(14);
  text("FRONT", 300, 70);
  text("REAR", 300, 340);
  
  // Status text
  textSize(16);
  fill(255);
  text("Front Left: " + (frontLeftLED ? "ON" : "OFF"), 150, 370);
  text("Front Right: " + (frontRightLED ? "ON" : "OFF"), 350, 370);
  text("Rear Left: " + (rearLeftLED ? "ON" : "OFF"), 150, 395);
  text("Rear Right: " + (rearRightLED ? "ON" : "OFF"), 350, 395);
}

void drawRearParkingDisplay() {
  float centerX = 800;
  float centerY = 500; // Position at bottom for rear view
  
  // Title
  fill(255);
  textSize(24);
  textAlign(CENTER, CENTER);
  text("REAR PARKING ASSIST - 40° WAVES", centerX, 350);
  
  // Display distance
  fill(0, 255, 0);
  textSize(48);
  text(ultrasonicDistance + " cm", centerX, 400);
  
  // Draw car rear
  fill(100);
  noStroke();
  rect(centerX - 40, centerY - 10, 80, 20, 5);
  
  // Draw 40-degree parking waves
  noFill();
  strokeWeight(4);
  
  // Layer 1: Green (Safe distance: 50-100cm)
  if (ultrasonicDistance >= 50 && ultrasonicDistance <= 100) {
    stroke(0, 255, 0, 200);
    draw40DegreeWave(centerX, centerY, 120, 150);
  }
  
  // Layer 2: Orange (Medium distance: 30-49cm)
  if (ultrasonicDistance >= 30 && ultrasonicDistance <= 49) {
    stroke(255, 165, 0, 200);
    draw40DegreeWave(centerX, centerY, 80, 110);
    
    // Also show green layer
    stroke(0, 255, 0, 200);
    draw40DegreeWave(centerX, centerY, 120, 150);
  }
  
  // Layer 3: Red (Close distance: 1-29cm)
  if (ultrasonicDistance >= 1 && ultrasonicDistance <= 29) {
    stroke(255, 0, 0, 200);
    draw40DegreeWave(centerX, centerY, 40, 70);
    
    // Also show orange and green layers
    stroke(255, 165, 0, 200);
    draw40DegreeWave(centerX, centerY, 80, 110);
    stroke(0, 255, 0, 200);
    draw40DegreeWave(centerX, centerY, 120, 150);
  }
  
  // Draw range indicators
  drawRangeIndicator(centerX - 200, centerY + 50, "50-100cm", color(0, 255, 0));
  drawRangeIndicator(centerX, centerY + 50, "30-50cm", color(255, 165, 0));
  drawRangeIndicator(centerX + 200, centerY + 50, "0-30cm", color(255, 0, 0));
}

void draw40DegreeWave(float x, float y, float startRadius, float endRadius) {
  float angle = radians(40); // 40 degrees in radians
  
  // Left wave (20° to left)
  float leftAngle1 = PI - angle/2;
  float leftAngle2 = PI + angle/2;
  
  // Right wave (20° to right)  
  float rightAngle1 = -angle/2;
  float rightAngle2 = angle/2;
  
  // Draw left wave segment
  arc(x, y, startRadius * 2, startRadius * 2, leftAngle1, leftAngle2);
  arc(x, y, endRadius * 2, endRadius * 2, leftAngle1, leftAngle2);
  
  // Draw right wave segment
  arc(x, y, startRadius * 2, startRadius * 2, rightAngle1, rightAngle2);
  arc(x, y, endRadius * 2, endRadius * 2, rightAngle1, rightAngle2);
  
  // Connect the arcs with lines to create wedge shape
  strokeWeight(2);
  line(x + cos(leftAngle1) * startRadius, y + sin(leftAngle1) * startRadius, 
       x + cos(leftAngle1) * endRadius, y + sin(leftAngle1) * endRadius);
  line(x + cos(leftAngle2) * startRadius, y + sin(leftAngle2) * startRadius, 
       x + cos(leftAngle2) * endRadius, y + sin(leftAngle2) * endRadius);
  line(x + cos(rightAngle1) * startRadius, y + sin(rightAngle1) * startRadius, 
       x + cos(rightAngle1) * endRadius, y + sin(rightAngle1) * endRadius);
  line(x + cos(rightAngle2) * startRadius, y + sin(rightAngle2) * startRadius, 
       x + cos(rightAngle2) * endRadius, y + sin(rightAngle2) * endRadius);
}

void drawRangeIndicator(float x, float y, String label, color c) {
  fill(c);
  textAlign(CENTER, CENTER);
  textSize(16);
  text(label, x, y);
}

void drawLED(float x, float y, boolean isOn, color ledColor) {
  if (isOn) {
    fill(ledColor);
    stroke(255);
    strokeWeight(2);
  } else {
    fill(50);
    stroke(100);
    strokeWeight(1);
  }
  ellipse(x, y, 25, 25);
  
  // Add glow effect when ON
  if (isOn) {
    fill(ledColor, 100);
    noStroke();
    ellipse(x, y, 35, 35);
  }
}

void drawDistance() {
  fill(100, 200, 255);
  textSize(20);
  textAlign(LEFT);
  text("Travel Distance: " + nf(totalDistance, 1, 1) + " m", 50, 650);
}

void drawEncoder() {
  fill(200, 100, 255);
  textSize(20);
  textAlign(LEFT);
  text("Encoder: " + encoderCount + " pulses", 50, 680);
}

void drawStatus() {
  fill(255);
  textSize(16);
  textAlign(LEFT);
  text("Status: " + statusMessage, 50, height - 50);
  text("Press R to reconnect", 50, height - 25);
}

void drawConnectionStatus() {
  fill(0, 255, 0);
  ellipse(width - 50, 50, 20, 20);
  fill(255);
  textSize(14);
  textAlign(RIGHT);
  text("CONNECTED", width - 70, 55);
}

void drawConnectionScreen() {
  fill(255);
  textSize(32);
  textAlign(CENTER, CENTER);
  text("SmartCar Dashboard", width/2, 100);
  text("Press R to reconnect", width/2, 150);
}

void connectToCar() {
  statusMessage = "Connecting to " + esp32IP + "...";
  myClient = new Client(this, esp32IP, 5204);
  
  delay(2000);
  if (myClient.active()) {
    connected = true;
    statusMessage = "Connected to car!";
    println("✅ Connected to: " + esp32IP);
  } else {
    statusMessage = "Failed to connect to " + esp32IP;
    println("❌ Connection failed");
  }
}

void keyPressed() {
  if (key == 'r' || key == 'R') {
    connected = false;
    connectToCar();
  }
}
