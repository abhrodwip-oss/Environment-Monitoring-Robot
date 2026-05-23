#include <WiFi.h>
#include <WebServer.h>  // simpler synchronous web server

// ---- WiFi credentials ----
const char* ssid = "Galaxy A14 AB";
const char* password = "Capricorn@9";

// ---- Create web server on port 80 ----
WebServer server(80);

// ---- Motor pins ----
#define AIN1 25
#define AIN2 26
#define PWMA 27
#define BIN1 32
#define BIN2 33
#define PWMB 14
#define STBY 22

// ---- Ultrasonic pins ----
#define TRIG 5
#define ECHO 18

// ---- Analog sensors ----
#define MQ135_PIN 36   // Gas sensor on ADC1
#define LM35_PIN 34

// ---- Flame & LDR ----
#define FLAME_PIN 23
#define LDR_PIN 21

// ---- Buzzer ----
#define BUZZER 19

long duration;
int distance;

// ---- Bot state ----
String botState = "STOP";

void setup() {
  Serial.begin(115200);

  // Motor pins
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH);

  // Ultrasonic
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  // Sensors
  pinMode(MQ135_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(LM35_PIN,INPUT);

  // LEDs & Buzzer
  pinMode(BUZZER, OUTPUT);

  // ADC setup
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // WiFi connect
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected! IP: ");
  Serial.println(WiFi.localIP());

  // Web routes
  server.on("/", handleRoot);
  server.on("/forward",   [](){ botState="FORWARD";   server.sendHeader("Location", "/"); server.send(302, ""); });
  server.on("/backward",  [](){ botState="BACKWARD";  server.sendHeader("Location", "/"); server.send(302, ""); });
  server.on("/stop",      [](){ botState="STOP";      server.sendHeader("Location", "/"); server.send(302, ""); });
  server.on("/left",      [](){ botState="LEFT";      server.sendHeader("Location", "/"); server.send(302, ""); });
  server.on("/right",     [](){ botState="RIGHT";     server.sendHeader("Location", "/"); server.send(302, ""); });

  server.begin();
}

void loop() {
  server.handleClient(); // process HTTP requests

  int dist = getDistance();

  // ---- Obstacle avoidance ----
  if (dist <= 20) {
    stopMotors();
    digitalWrite(BUZZER, HIGH);
    delay(500);
    turnLeft(120);
    delay(600);
    moveForward(150);
    delay(800);
    stopMotors();
    digitalWrite(BUZZER, LOW);
    botState = "STOP";
  } else {
    digitalWrite(BUZZER, LOW);

    // Bot movement from webpage
    if(botState=="FORWARD") moveForward(200);
    else if(botState=="BACKWARD") moveBackward(200);
    else if(botState=="LEFT") turnLeft(200);
    else if(botState=="RIGHT") turnRight(200);
    else stopMotors();
  }

  delay(100);
}

// ---- Webpage handler ----
void handleRoot() 
{
  String html = "<html><body>";
  html += "<h2>ESP32 Bot Control</h2>";
  html += "<button onclick=\"location.href='/forward'\">Right</button>";
  html += "<button onclick=\"location.href='/backward'\">Left</button>";
  html += "<button onclick=\"location.href='/left'\">Backward</button>";
  html += "<button onclick=\"location.href='/right'\">Forward</button>";
  html += "<button onclick=\"location.href='/stop'\">Stop</button><br><br>";

  int flameVal = digitalRead(FLAME_PIN);
  String flameStatus = (flameVal == LOW) ? "YES" : "NO";

  int rawGas = analogRead(MQ135_PIN);
  float gasPercent = (rawGas / 4095.0) * 100;

  // ---- Air Quality Classification ----
  String airQuality;
  if (gasPercent < 25) airQuality = "Good";
  else if (gasPercent < 50) airQuality = "Moderate";
  else if (gasPercent < 75) airQuality = "Poor";
  else airQuality = "Very Poor";

  int dist = getDistance();

  int ldrVal = digitalRead(LDR_PIN);
  String lightStatus = (ldrVal == HIGH) ? "Bright" : "Dark";

  // ---- Temperature (LM35) ----
  int rawTemp = analogRead(LM35_PIN);
  float voltage = (rawTemp / 4095.0) * 12.28;   // ✔ correct ADC scaling
  float tempC  = voltage * 100.0;             // ✔ LM35 = 10 mV per °C


  // Display
  html += "<p>Flame: " + flameStatus + "</p>";
  html += "<p>Distance: " + String(dist) + " cm</p>";
  html += "<p>Gas: " + String(gasPercent, 1) + " % (" + airQuality + ")</p>";
  html += "<p>Light (LDR): " + lightStatus + "</p>";
  html += "<p>Temperature: " + String(tempC, 1) + " °C</p>";

  html += "<meta http-equiv='refresh' content='1'>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// ---- Motor functions ----
void moveForward(int speed) {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMA, speed);
  analogWrite(PWMB, speed);
}

void moveBackward(int speed) {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMA, speed);
  analogWrite(PWMB, speed);
}

void stopMotors() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
}

void turnRight(int speed){
  digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH);
  analogWrite(PWMA, speed);
  analogWrite(PWMB, speed);
}

void turnLeft(int speed){
  digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
  analogWrite(PWMA, speed);
  analogWrite(PWMB, speed);
}

// ---- Ultrasonic ----
int getDistance(){
  digitalWrite(TRIG,LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG,HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG,LOW);

  duration = pulseIn(ECHO,HIGH,20000);
  if(duration==0) return 999;
  return duration*0.034/2;
}
