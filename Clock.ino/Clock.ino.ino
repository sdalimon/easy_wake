// Gentle Wake
// Shawn D'Alimonte

#include <EEPROM.h>

#include <Time.h>
#include <TimeLib.h>
#include <Wire.h>
#include <string.h>
#include <ctype.h>

String Buffer; // Serial Command Buffer

const int DISP_ADDR = 0x71;

const int DEBUG_PIN = 23;
const int R_PIN = 5;
const int G_PIN = 4;
const int B_PIN = 3;
const int LED_PIN = 13;

int r, g, b;

time_t alarm_time;
time_t ramp_len = 15 * 60;
time_t alarm_len = 60 * 60;

void setup() {
  Serial.begin(9600);
  //while (!Serial) ; // wait for serial
  delay(200);
  Wire.begin();
  setSyncProvider(getTeensy3Time);
  setTime(getTeensy3Time());
  setSyncInterval(60);
  digitalWrite(R_PIN, LOW);
  digitalWrite(G_PIN, LOW);
  digitalWrite(B_PIN, LOW);
  digitalWrite(DEBUG_PIN, LOW);
  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);
  pinMode(DEBUG_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  analogWriteResolution(14);
  analogWriteFrequency(R_PIN, 1464.843);  // Ideal value for 14 bit with 24MHz clock
  analogWriteFrequency(R_PIN, 1464.843);
  analogWriteFrequency(R_PIN, 1464.843);
  analogWrite(R_PIN, 0);
  analogWrite(G_PIN, 0);
  analogWrite(B_PIN, 0);
  r = 0; b = 0; g = 0;

  alarm_time = LoadAlarm();
  alarm_len = ReadNVRAM32(16);
  ramp_len = ReadNVRAM32(24);

  Serial.println("Gentle Wake");
  Serial.println("-----------");
  Serial.println("Type 'help' for commands");
  Serial.print("> ");
}

void loop() {
  //   Serial.println(Serial.available());
  if (Serial.available() > 0) {
    parseChar();
  }
  if (millis() % 500 == 0) { // Update display twice a second
    DisplayTime();
    UpdateLEDS();
    CheckAlarm();
  }
}

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

time_t getTeensy3Time() {
  return Teensy3Clock.get();
}

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

void DisplayTime(void) {
  int h10, h, colon, m10, m;
  h10 = hour() / 10;
  h = hour() % 10;
  m10 = minute() / 10;
  m = minute() % 10;
  colon = second() % 2;

  Wire.beginTransmission(DISP_ADDR);
  Wire.write(0x79); Wire.write(0x00); // Move cursor to first character
  Wire.write(h10 == 0 ? ' ' : h10);
  Wire.write(h);
  Wire.write(m10);
  Wire.write(m);
  Wire.write(0x77); Wire.write(colon ? 0x10 : 0x00);
  Wire.endTransmission(1);

  //digitalWrite(LED_PIN, colon);
}

void DisplayBright(int b) {
  if ((b < 0) || (b > 255)) {
    Serial.println("Invalid Brightness (0 - 255)");
    return;
  }
  Wire.beginTransmission(DISP_ADDR);
  Wire.write(0x7A); Wire.write(b);
  Wire.endTransmission(1);
}

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
  //RTC.set(now());
  Teensy3Clock.set(now());
  Serial.println("Time Set");
}

void TimeRead(void) {
  char buf[50];
  sprintf(buf, "Time: % d / % d / % d % d: % d: % d\n", year(), month(), day(), hour(), minute(), second());
  Serial.println(buf);
}
// Handle a Character from the serial port


void AlarmSet(String t) {
  int pos = 0;
  int lpos;
  char buf[50];
  TimeElements tm;

  Serial.println(t);

  //  pos = t.indexOf("/");
  tm.Year = 0; //t.substring(0, pos).toInt() - 1970;
  //  lpos = pos;
  //  pos = t.indexOf("/", lpos + 1);
  tm.Month = 1;//t.substring(lpos + 1, pos).toInt();
  //  lpos = pos;
  //  pos = t.indexOf(" ", lpos + 1);
  tm.Day = 1;//t.substring(lpos + 1, pos).toInt();
  //  lpos = pos;
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
void CheckAlarm(void) {
  time_t ct = Time_NoDate(now());
  time_t at = Time_NoDate(alarm_time);
  time_t diff = (ct - at) % (24 * 3600);

  //  Serial.print(ct); Serial.print(", \t");
  //  Serial.print(at); Serial.print(", \t");
  //  Serial.print(diff); Serial.print(", \t");
  //  Serial.println(r);

  if ((diff >= 0) && (diff <= ramp_len)) {
    //    r = g = b = round(16383.0 * log((float)diff / (float)ramp_len));
    r = g = b = round(pow(16383., (float)diff / (float)ramp_len)); // try to linearize brigthness
  } else if ((diff >= 0) && (diff <= alarm_len)) {
    r = g = b = 16383;
  } else {
    r = g = b = 0;
  }
}

// Save the Alarm Time in the RTC chip
// Assumes time_t is 32 bit...
void SaveAlarm(time_t t) {
  WriteNVRAM32(8, t);
}

// Retrieve Alarm from RTC
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



