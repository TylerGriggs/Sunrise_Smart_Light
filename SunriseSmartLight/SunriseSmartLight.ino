/*
  Created by Tyler Griggs for Tim's Sunrise Bulb Idea
    Adapted from TimeNTP_ESP8266WiFi.ino
      Udp NTP Client
      Get the time from a Network Time Protocol (NTP) time server
      Demonstrates use of UDP sendPacket and ReceivePacket
      For more on NTP time servers and the messages needed to communicate with them,
      see http://en.wikipedia.org/wiki/Network_Time_Protocol
      created 4 Sep 2010
      by Michael Margolis
      modified 9 Apr 2012
      by Tom Igoe
      updated for the ESP8266 12 Apr 2015
      by Ivan Grokhotkov
      Udp NTP Client code is in the public domain.
    This sketch uses the Dusk2Dawn Arduino Library
    This sketch uses the ESP8266WiFi library
*/
#include <Adafruit_NeoPixel.h>
#include <time.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <Wire.h>
#include <Dusk2Dawn.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins 4,5 / D1 D2 on ESP-12E)
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#ifndef STASSID
#define STASSID "YOUR_SSID_HERE"        // your network SSID (name) 
#define STAPSK  "YOUR_PASSWORD_HERE"    // your network password
#endif

#define SEQUENCE_MINS   30  // Number of minutes before sunrise to start the sequence
#define BUTTON_PIN      12  // Button Pin on controller
#define LED_PIN         14  // LED Pin on controller
#define LED_COUNT       64  // Number of LEDs
#define LED_BRIGHTNESS 100  // Brightness out of 255
#define LED_WAIT        50  // Speed Constant

const char * ssid = STASSID; 
const char * pass = STAPSK;  

// Declare our NeoPixel strip object and make some colors:
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
uint32_t CGREEN = strip.Color(  0,255,   0);
uint32_t CYELLOW = strip.Color(255,  128,   0);
uint32_t CRED = strip.Color  (255,  0,   0);
uint32_t CBLUE = strip.Color (  0,  0, 255);

// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";
//static const char ntpServerName[] = "time.nist.gov";
//static const char ntpServerName[] = "time-a.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-b.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-c.timefreq.bldrdoc.gov";

//const int timeZone = 1;     // Central European Time
const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)

// Dusk2Dawn Setup
// Coordinates are used to initialize a Dusk2Dawn location object, with UTC offset for timezone
// Example: (68.8552, -42.7988, -8)
Dusk2Dawn Location(00.0000, 00.0000, timeZone);

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

time_t t;
time_t prevDisplay = 0; // when the digital clock was displayed
time_t midnight, sunrise, start_time; 
           
int curr_brightness = 0;
int curr_minute = 1;
int prev_hour, prev_minute;     // Store the hour and min on startup
bool tmw = true;                // When set to TRUE, Sunrise Sequence starts immediately on Start-Up if after sunrise

void setup()
{
  Serial.begin(115200);
  
  // Button Setup
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // IC2 Screen Setup - SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  } display.display(); // Displays Splash screen while connecting to WIFI

  // LED Strip Setup
  strip.begin();           // INITIALIZE NeoPixel strip object 
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(LED_BRIGHTNESS); // Set BRIGHTNESS (max = 255)
  
  while (!Serial) ; // Needed for Leonardo only
  delay(250);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    flash_yellow();
    Serial.print(".");
  } flash_green();

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");

  // Clear the Screen once WIFI is connected and waiting to get TIME
  display.clearDisplay();
  display.display();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.println("Connected to WIFI\n\nSending REQUEST\n to NTP server...");
  display.display();
  
  setSyncProvider(getNtpTime);
  setSyncInterval(300); // Syncs Time from NTP every 5 mins (300 seconds)

  midnight = get_time(year(), month(), day(), 0, 0, 1);   // Initialize with Today's "midnight" in Epoch seconds
  sunrise = midnight + get_sunrise_offset();
}

void loop()
{ 
  if (timeStatus() != timeNotSet) {
    t = now();                  // Now() is equivalent to the Epoch in seconds right now
    if (t != prevDisplay) {     // update the display only if time has changed
      digitalClockDisplay();
      
      if (hour(t) < prev_hour) {// When the hour suddenly drops from 24 to 0, it's midnight
        tmw = true;
        curr_minute = 1;
        midnight = t;
        sunrise = midnight + get_sunrise_offset();
        print_sunrise_calc();
      }
      if (tmw && t > (sunrise - SEQUENCE_MINS) && curr_minute <= SEQUENCE_MINS && minute(t) != prev_minute) {
          print_sunrise_calc();
          sunrise_sequence();
          curr_minute++;
      }      
      prevDisplay = t;
      prev_hour = hour(t);
      prev_minute = minute(t);
      print_time_serial();
    }
  }
  if (digitalRead(BUTTON_PIN) == LOW) {
    tmw = false;
    wipe_off();
    Serial.println("BUTTON PUSHED!! ENDING SUNRISE SEQUENCE.");
    }
}

void sunrise_sequence() {
  Serial.print("Sequence Active for ");
  Serial.print(curr_minute);
  Serial.println(" minutes.");
  int r,g,b;
  int b_itr = round(255 / SEQUENCE_MINS) * (curr_minute + 1);
  if (b_itr > 255 || curr_minute == SEQUENCE_MINS) {b_itr = 255;}   // Dont let Brightness go over 255, ensure max birghtness
  for (int j = curr_brightness; j < b_itr; j++) { 
      r = j;
      g = 0.002 * sq(j); // 0.002x^2
      b = 0;
      for (int i=0; i<strip.numPixels(); i++) {
          strip.setPixelColor(i, r, g, b);
      }
      strip.show();
      delay(LED_WAIT);
  }
  curr_brightness = b_itr;
  Serial.print("LED Brightness currently: ");
  Serial.print(curr_brightness);
  Serial.println("/255");
  Serial.print("Minutes Left: ");
  Serial.println(SEQUENCE_MINS - curr_minute);
}

void digitalClockDisplay()
{
  display.clearDisplay();
  display.setTextSize(2);      // Normal 1:1 pixel scale
  //display.setFontType(0);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  // Print the Time to the Display
  display.print(hourFormat12());
  printDigits_display(minute());
  printDigits_display(second());
  if (isAM()) {display.print(" A");}
  else        {display.print(" P");}
  display.setTextSize(1);
  display.print("\n\nDate:     ");
  display.print(month());
  display.print(".");
  display.print(day());
  display.print(".");
  display.print(year());
  display.print("\nSun @ ");
  display.print(hour(sunrise));
  printDigits_display(minute(sunrise));
  if (tmw) {
    if (t > sunrise) { display.println(" Running");}
    else             { display.println(" Time Set");}
    }
  else     {display.println(" Waiting");}
  display.display();
}

time_t get_sunrise_offset() {
  time_t s  = Location.sunrise(year(), month(), day(), false); // number of minutes elaspsed after midnight
  return (s * 60);    // return number of seconds past midnight
}

time_t get_time(int YYYY, byte MM, byte DD, byte hh, byte mm, byte ss)
{
  tmElements_t tmSet;
  tmSet.Year = YYYY - 1970;
  tmSet.Month = MM;
  tmSet.Day = DD;
  tmSet.Hour = hh;
  tmSet.Minute = mm;
  tmSet.Second = ss;
  return makeTime(tmSet);         //convert to time_t
}

/* ----- Human Readable Outputs ------ */
void print_sunrise_calc() {
    Serial.print("Sunrise was calculated for ");
    Serial.print(month(sunrise));
    Serial.print(".");
    Serial.print(day(sunrise));
    Serial.print(".");
    Serial.print(year(sunrise));
    Serial.print(" ");
    Serial.print(hour(sunrise));
    Serial.print(":");
    Serial.print(minute(sunrise));
    if (isAM(sunrise)) {Serial.println(" AM");}
    else               {Serial.println(" PM");}
}
void print_time_serial() {
    Serial.print(month());
    Serial.print("/");
    Serial.print(day());
    Serial.print("/");
    Serial.print(year());
    Serial.print(" ");
    Serial.print(hour());
    printDigits_serial(minute());
    printDigits_serial(second());
    if (isAM()) {Serial.print(" AM");}
    else        {Serial.print(" PM");}
    Serial.println();
}
    // Print the appropriate '0's for the minute and second if needed
void printDigits_serial(int digits)
{
  Serial.print(":");
  if (digits < 10) {
    Serial.print('0');
  }
  Serial.print(digits);
}
void printDigits_display(int digits)
{
  display.print(":");
  if (digits < 10) {
    display.print('0');
  }
  display.print(digits);
}

/*------- LED LOGIC FUNCTIONS ---------*/
void flash_green() {
  colorWipe(CGREEN, round(LED_WAIT/4));    // When finished with Setup() Flash strip Green
  wipe_off();
  }
void flash_yellow() {
  colorWipe(CYELLOW, round(LED_WAIT/4));   // When finished with Setup() Flash strip Red
  wipe_off();
  }
void flash_red() {
  colorWipe(CRED, round(LED_WAIT/4));      // When finished with Setup() Flash strip Red
  wipe_off();
  }
void wipe_off() {
  colorWipe(strip.Color(  0, 0,   0), round(LED_WAIT/4));    // turn off one by one
  }
void colorWipe(uint32_t color, int wait) {
  strip.show();
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
}
void two_color(uint32_t color1, uint32_t color2, int wait) {
  strip.show();
  bool switch_color = true;
  for(int r=0; r<20; r++) {
    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
      if(i%2 == 0) {
          switch_color = !switch_color;
          strip.show();                          //  Update strip to match
          delay(wait);                           //  Pause for a moment
        }
      if(switch_color == true) {
        strip.setPixelColor(i, color1);        //  Set pixel's color (in RAM)
        }
      else {
        strip.setPixelColor(i, color2);        //  Set pixel's color (in RAM)
        }
    }
  }
}
void rainbow(int wait) {
  strip.show();
  for(long firstPixelHue = 0; firstPixelHue < 5*65536; firstPixelHue += 256) {
    for(int i=0; i<strip.numPixels(); i++) {
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
  }
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      //flash_green();
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  // digital clock display of the time
  Serial.print(hour());
  display.println("No NTP Response :-(");
  display.display();
  //flash_red();
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
  //flash_yellow();
}
