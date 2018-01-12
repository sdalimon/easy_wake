// Gentle Wake
// Shawn D'Alimonte
// 2018-01-11

#include <EEPROM.h>
#include <Time.h>
#include <TimeLib.h>
#include <Wire.h>
#include <string.h>
#include <ctype.h>

String Buffer; // Serial Command Buffer

// Address of I2C 7 segment display
const int DISP_ADDR = 0x71;

// Pin assignments
const int DEBUG_PIN = 23;  // Not used
const int R_PIN = 5;       // PWM for Red LEDs HI==LEDs on
const int G_PIN = 4;	   // PWM for Green LEDs HI==LEDs on
const int B_PIN = 3;       // PWM for Blue LEDs HI==LEDs on
const int LED_PIN = 13;    // On Board LED for debug use HI==LED on
const int TIME_PIN = 14;   // Time button 0==Pressed
const int ALM_PIN = 15;    // Alarm button 0==Pressed
const int HOUR_PIN = 16;   // Hour button 0==Pressed
const int MIN_PIN = 17;   // Minute button 0==Pressed

int r, g, b;  // Current PWM value for LEDs (0-16383)
int disp_state = 0;  // 0 == Showing time, 1 == showing alarm setting

time_t alarm_time;          // Clock time to start ramp up of LEDs
time_t ramp_len = 15 * 60;  // Length of ramp up (Default 15 minutes)
time_t alarm_len = 60 * 60; // How long LEDs stay on (Default 1 hour)

// Setup
void setup() {
  // Configure serial port for menu
  Serial.begin(9600);
  delay(200);

  // I2C for display
  Wire.begin();

  // RTC Setup
  // Needs RTC crystal and battery connected as in Teensy documentation
  setSyncProvider(getTeensy3Time);
  setTime(getTeensy3Time());
  setSyncInterval(60);

  // Configure pins
  digitalWrite(R_PIN, LOW);
  digitalWrite(G_PIN, LOW);
  digitalWrite(B_PIN, LOW);
  digitalWrite(DEBUG_PIN, LOW);
  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);
  pinMode(DEBUG_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(TIME_PIN, INPUT_PULLUP);
  pinMode(ALM_PIN, INPUT_PULLUP);
  pinMode(HOUR_PIN, INPUT_PULLUP);
  pinMode(MIN_PIN, INPUT_PULLUP);

  // Configure PWM
  analogWriteResolution(14);
  analogWriteFrequency(R_PIN, 1464.843);  // Ideal value for 14 bit with 24MHz clock
  analogWriteFrequency(R_PIN, 1464.843);
  analogWriteFrequency(R_PIN, 1464.843);
  analogWrite(R_PIN, 0);
  analogWrite(G_PIN, 0);
  analogWrite(B_PIN, 0);
  r = 0; b = 0; g = 0;

  // Get alarm parameters from NVRAM
  alarm_time = LoadAlarm();
  alarm_len = ReadNVRAM32(16);
  ramp_len = ReadNVRAM32(24);

  // Print Banner
  Serial.println("Gentle Wake");
  Serial.println("-----------");
  Serial.println("Type 'help' for commands");
  Serial.print("> ");
}

// Main Loop
void loop() {

  // Check for data from serial port.  If so receive and parse the charcter
  if (Serial.available() > 0) {
    parseChar();
  }

  // Check buttons every 500 ms (Avoids debounce...)
  // CheckButtons() will update time or allarm as needed
  // Note that display mode is handled in DisplayTime() (Alarm is displayed when ALM is pressed
  if (millis() % 500 == 0) {
    CheckButtons();
  }

  // Update the display and LEDs every 100 ms
  if (millis() % 100 == 0) {
    DisplayTime();
    UpdateLEDS();
    CheckAlarm();
  }

}

// Receive a character from the serial port
// Update the buffer, including handling BS and DEL
// When newline is received call parseBuffer to execurte the command
void parseChar(void) {
  int c = Serial.read();
  if (isprint(c)) {
    Serial.write(c);
    Buffer += char(c);
  } else if ((c == '\n') || (c == '\r')) {
    Buffer.trim(); // Remove excess white space
    parseBuffer();
    Serial.println();
    Serial.print("> ");
  } else if ((c == 0x08) || (c == 0x7f)) { // BS or DEL
    Buffer = Buffer.substring(0, Buffer.length()); // Remove last char from buffer
    Serial.write(0x08); Serial.write(0x20); Serial.write(0x08);
  }
}

// Parse a buffer and execute the command
void parseBuffer(void) {
  Serial.println();
  if (Buffer.startsWith("set ")) {
    TimeSet(Buffer.substring(Buffer.indexOf(" ")));
  } else if (Buffer.startsWith("read")) {
    TimeRead();
  } else if (Buffer.startsWith("aset ")) {
    AlarmSet(Buffer.substring(Buffer.indexOf(" ")));
  } else if (Buffer.startsWith("aread")) {
    AlarmRead();
  } else if (Buffer.startsWith("r ")) {
    r = Buffer.substring(Buffer.indexOf(" ")).toInt();
    Serial.print("Red: "); Serial.println(r);
  } else if (Buffer.startsWith("g ")) {
    g = Buffer.substring(Buffer.indexOf(" ")).toInt();
    Serial.print("Green: "); Serial.println(g);
  } else if (Buffer.startsWith("b ")) {
    b = Buffer.substring(Buffer.indexOf(" ")).toInt();
    Serial.print("Blue: "); Serial.println(b);
  } else if (Buffer.startsWith("w ")) {
    r = g = b = Buffer.substring(Buffer.indexOf(" ")).toInt();
    Serial.print("White: "); Serial.println(b);
  } else if (Buffer.startsWith("len ")) {
    alarm_len = Buffer.substring(Buffer.indexOf(" ")).toInt();
    Serial.print("Len: "); Serial.println(alarm_len);
    WriteNVRAM32(16, alarm_len);
  } else if (Buffer.startsWith("ramp ")) {
    ramp_len = Buffer.substring(Buffer.indexOf(" ")).toInt();
    Serial.print("Ramp: "); Serial.println(ramp_len);
    WriteNVRAM32(24, ramp_len);
  } else if (Buffer.startsWith("lstat")) {
    Serial.print("R: "); Serial.print(r);
    Serial.print(",\tG: "); Serial.print(g);
    Serial.print(",\tB: "); Serial.println(b);
  } else if (Buffer.startsWith("help")) {
    Serial.println("help             - Show commands");
    Serial.println("set y/m/d h:m:s  - Set time");
    Serial.println("read             - Show time");
    Serial.println("aset h:m:s       - Set alarm");
    Serial.println("ramp s           - Set LED Ramp Time");
    Serial.println("len s            - Set LED On Time");
    Serial.println("aread            - Show alarm");
    Serial.println("bright xxx       - Set Display Brightness");
    Serial.println("r / g / b /w xxx - Set R, G, B, all LED brightness");
    Serial.println("lstat            - Display LED brightness");
  }  else if (Buffer.startsWith("bright")) {
    DisplayBright(Buffer.substring(Buffer.indexOf(" ")).toInt());
  } else {
    Serial.println("Unknown command");
  }
  Buffer = "";
}

// Read tiem from Teensy RTC
time_t getTeensy3Time() {
  return Teensy3Clock.get();
}

// Update the LED PWM from the globals r, g and b
// Doesn't update if there is no PWM change to avoid flickering
void UpdateLEDS(void) {
  static int old_r, old_g, old_b;
  if (r != old_r) {
    analogWrite(R_PIN, r);
    old_r = r;
  }
  if (g != old_g) {
    analogWrite(G_PIN, g);
    old_g = g;
  }
  if (b != old_b) {
    analogWrite(B_PIN, b);
    old_b = b;
  }
}

// Display the current time or alarm settinf on the I2C display based on the state of the alarm switch
// Blinks the display colon every other second
void DisplayTime(void) {
  int h10, h, colon, m10, m;
  if (digitalRead(ALM_PIN) == HIGH) {
    h10 = hour() / 10;
    h = hour() % 10;
    m10 = minute() / 10;
    m = minute() % 10;
    colon = second() % 2;
  } else {
    h10 = hour(alarm_time) / 10;
    h = hour(alarm_time) % 10;
    m10 = minute(alarm_time) / 10;
    m = minute(alarm_time) % 10;
    colon = 1;
  }
  Wire.beginTransmission(DISP_ADDR);
  Wire.write(0x79); Wire.write(0x00); // Move cursor to first character
  Wire.write(h10 == 0 ? ' ' : h10);
  Wire.write(h);
  Wire.write(m10);
  Wire.write(m);
  Wire.write(0x77); Wire.write(colon ? 0x10 : 0x00);
  Wire.endTransmission(1);
}

// Set the display brightness
// The display remembers this on its own, so it only needs to be
// called when the brightness is changed
void DisplayBright(int b) {
  if ((b < 0) || (b > 255)) {
    Serial.println("Invalid Brightness (0 - 255)");
    return;
  }
  Wire.beginTransmission(DISP_ADDR);
  Wire.write(0x7A); Wire.write(b);
  Wire.endTransmission(1);
}

// Parse a string in the format y/m/d h:m:s
// Convert it to a time_t and set it as the current time
void TimeSet(String t) {
  int pos = 0;
  int lpos;
  int y, mn, d, h, m, s;
  char b[50];

  pos = t.indexOf("/");
  y = t.substring(0, pos).toInt();
  lpos = pos;
  pos = t.indexOf("/", lpos + 1);
  mn = t.substring(lpos + 1, pos).toInt();
  lpos = pos;
  pos = t.indexOf(" ", lpos + 1);
  d = t.substring(lpos + 1, pos).toInt();
  lpos = pos;
  pos = t.indexOf(":", lpos + 1);
  h = t.substring(lpos + 1, pos).toInt();
  lpos = pos;
  pos = t.indexOf(":", lpos + 1);
  m = t.substring(lpos + 1, pos).toInt();
  s = t.substring(pos + 1).toInt();

  sprintf(b, " % d / % d / % d % d: % d: % d\n", y, mn, d, h, m, s);
  Serial.println(b);

  if ((y < 0) || (y > 9999)) {
    Serial.println("Error, Invalid Year)");
    return;
  }
  if ((mn < 1) || (mn > 12)) {
    Serial.println("Error, Invalid Month)");
    return;
  }
  if ((d < 1) || (d > 31)) {
    Serial.println("Error, Invalid Day)");
    return;
  }
  if ((h < 0) || (h > 23)) {
    Serial.println("Error, Invalid Hour)");
    return;
  }
  if ((m < 0) || (m > 59)) {
    Serial.println("Error, Invalid Minute)");
    return;
  }
  if ((s < 0) || (s > 59)) {
    Serial.println("Error, Invalid Second)");
    return;
  }

  setTime(h, m , s, d, mn, y);
  Teensy3Clock.set(now());
  Serial.println("Time Set");
}

// Display the current time
void TimeRead(void) {
  char buf[50];
  sprintf(buf, "Time: % d / % d / % d % d: % d: % d\n", year(), month(), day(), hour(), minute(), second());
  Serial.println(buf);
}

// Parse a string in the format of h:m:s
// Convert to a time_t and save as the alarm time
void AlarmSet(String t) {
  int pos = 0;
  int lpos;
  char buf[50];
  TimeElements tm;

  Serial.println(t);

  tm.Year = 0; // Date is not used for alarm
  tm.Month = 1;
  tm.Day = 1;
  pos = t.indexOf(":");
  tm.Hour = t.substring(0, pos).toInt();
  lpos = pos;
  pos = t.indexOf(":", lpos + 1);
  tm.Minute = t.substring(lpos + 1, pos).toInt();
  tm.Second = t.substring(pos + 1).toInt();

  sprintf(buf, " % d / % d / % d % d: % d: % d\n", tm.Year, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
  Serial.println(buf);

  if ((tm.Year < 0) || (tm.Year > 99)) {
    Serial.println("Error, Invalid Year)");
    return;
  }
  if ((tm.Month < 1) || (tm.Month > 12)) {
    Serial.println("Error, Invalid Month)");
    return;
  }
  if ((tm.Day < 1) || (tm.Day > 31)) {
    Serial.println("Error, Invalid Day)");
    return;
  }
  if ((tm.Hour < 0) || (tm.Hour > 23)) {
    Serial.println("Error, Invalid Hour)");
    return;
  }
  if ((tm.Minute < 0) || (tm.Minute > 59)) {
    Serial.println("Error, Invalid Minute)");
    return;
  }
  if ((tm.Second < 0) || (tm.Second > 59)) {
    Serial.println("Error, Invalid Second)");
    return;
  }

  alarm_time = makeTime(tm);
  SaveAlarm(makeTime(tm));
  Serial.println("Alarm Set");
}

// Display the current alarm setting
void AlarmRead(void) {
  char buf[50];
  sprintf(buf, "Alarm: % d / % d / % d % d: % d: % d\n", year(alarm_time), month(alarm_time), day(alarm_time), hour(alarm_time), minute(alarm_time), second(alarm_time));
  Serial.println(buf);
  Serial.print("Len: "); Serial.print(alarm_len); Serial.print(", \tRamp: "); Serial.print(ramp_len);
}

// Strip date from a time_t (seconds since midnight)
time_t Time_NoDate(time_t t) {
  time_t ret;
  ret = 3600 * hour(t) + 60 * minute(t) + second(t);
  return ret;
}

// Check the Alarm and adjust the LEDs based on the time
// Applies a logarithic respnse to give a more linear ramp in apparent brightness
void CheckAlarm(void) {
  time_t ct = Time_NoDate(now());
  time_t at = Time_NoDate(alarm_time);
  time_t diff = (ct - at) % (24 * 3600);

  if ((diff >= 0) && (diff <= ramp_len)) {
    r = g = b = round(pow(16383., (float)diff / (float)ramp_len)); // try to linearize brigthness
  } else if ((diff >= 0) && (diff <= alarm_len)) {
    r = g = b = 16383;
  } else {
    r = g = b = 0;
  }
}

// Save the Alarm Time in the NVRAM
// Assumes time_t is 32 bit...
void SaveAlarm(time_t t) {
  WriteNVRAM32(8, t);
}

// Retrieve Alarm from NVRAM
time_t LoadAlarm(void) {
  time_t t;
  t = ReadNVRAM32(8);
  return t;
}

// Write 32 bit value to NVRAM)
void WriteNVRAM32(uint8_t addr, uint32_t t) {
  EEPROM.put(addr, t);
}

// Read 32 bit value from NVRAM
uint32_t ReadNVRAM32(uint8_t addr) {
  uint32_t r;
  EEPROM.get(addr, r);
  return r;
}

// UPdate time or alarm based on button state
// Call every 500 ms
void CheckButtons() {
  int val;
  tmElements_t tm;
  static int last_alm_button = HIGH;
  int alm = digitalRead(ALM_PIN);
  int tim = digitalRead(TIME_PIN);
  int hr = digitalRead(HOUR_PIN);
  int mn = digitalRead(MIN_PIN);
  
  if (tim == LOW && hr == LOW) { // Increment time hours
    val = hour();
    val++;
    if (val > 23) val = 0;
    setTime(val, minute(), second(), day(), month(), year());
  } else if (tim == LOW && mn == LOW) { // Increment time minutes
    val = minute();
    val++;
    if (val > 59) val = 0;
    setTime(hour(), val, second(), day(), month(), year());
  } else if (alm == LOW && hr == LOW) { // Increment alarm hours
    breakTime(alarm_time, tm);
    tm.Hour++;
    if (tm.Hour > 23) tm.Hour = 0;
    alarm_time = makeTime(tm);
  } else if (alm == LOW && mn == LOW) { // Increment alarm minutes
    breakTime(alarm_time, tm);
    tm.Minute++;
    if (tm.Minute > 59) tm.Minute = 0;
    alarm_time = makeTime(tm);
  }

  // Only update alarm in NVRAM when button is released.  This save wear on NVRAM
  if(last_alm_button == LOW && alm==HIGH) {
    SaveAlarm(alarm_time);
  }
  last_alm_button=alm;
}

