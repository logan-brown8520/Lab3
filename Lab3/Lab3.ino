// Time - Version: Latest 
#include <Time.h>
#include <TimeLib.h>
#include <EEPROM.h>
#include <Adafruit_FT6206.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_ILI9341.h" // Hardware-specific library
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>


Adafruit_GPS GPS(&Serial1);
HardwareSerial mySerial = Serial1;

// TFT display and SD card will share the hardware SPI interface.
// Hardware SPI pins are specific to the Arduino board type and
// cannot be remapped to alternate pins.  For Arduino Uno,
// Duemilanove, etc., pin 11 = MOSI, pin 12 = MISO, pin 13 = SCK.

// The FT6206 uses hardware I2C (SCL/SDA)
Adafruit_FT6206 ctp = Adafruit_FT6206();

#define TFT_DC 9
#define TFT_CS 10
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

#define SD_CS 4

int view = 0;         //keeps track of which of the views we are in, 0-15
int mode = 0;         //keeps track of whether we are in off(0), auto(1), heating(2), or cooling(3)

int temper = 70;      //the temperature of the house
int temper2 = 72;     //the hold temperature

//**THESE INITIAL VALUES ARE ONLY USED FOR DEMO PURPOSES. AFTER FIRST WRITE TO EEPROM, FURTHER EXAMPLES WILL READ FROM EEPROM ON STARTUP
int wkT[]={70, 70, 70, 70, 70, 70, 70, 70};   //temperatures for the week(0-3) and weekend(4-7)
int wkH[]={5, 11, 5, 11, 5, 11, 5, 11};     //hours for the week(0-3) and weekend(4-7)
int wkM[]={0, 0, 0, 0, 0, 0, 0, 0};       //minutes for the week(0-3) and weekend(4-7)
bool wkA[] = {true, true, false, false, true, true, false, false};  //AM/PM for the week(0-3) and weekend(4-7), AM is true

//Time variables
int myday;       //the day of the month we are setting the clock to, 1-31
int mymonth = 0;     //the month we are setting the clock to, 1-12
int myyear = 0;    //the year we are setting the clock to
int myhour = 0;       //the hour we are setting the clock to, 1-12
int myminute; //minute we are setting the clock to, 0-59
int outputpin = A1;   //set Analog pin
// Time variable offsets
//** SET ONE TIME WITH EEPROM
int offDay = 0;
int offMonth = 0;
int offYear = 0;
int offHour = 0;
int offMin = 0;
bool morning = true;  //AM when true, PM when false
bool wkED = false;    //enables programmed set points during the week, disabled when false
bool wkVer = false;   //when true, allows enabling of programmed set points for week
bool wkendED = false; //enables programmed set points during the weekend, disabled when false
bool wkendVer = false;//when true, allows enabling of pragrammed set points for weekend
bool cooling = false; //tracks if the AC is on, off when false
bool heating = false; //tracks if the heater is on, off when false
bool hold = true;     //tracks if we are overriding the programmed set points, overriding when true
bool isDim = false;   //tracks if screen dim
time_t t = now();
time_t update = now();
time_t touched = now();
time_t tempRead = now();



void setup(void) {
  Serial.begin(115200); //baud rate to prevent witch craft
  
  //GPS initialization
  GPS.begin(9600);
  
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);   
  GPS.sendCommand(PGCMD_ANTENNA);

  delay(1000);
  
  mySerial.println(PMTK_Q_RELEASE);
  do{
    getTime(&myyear, &mymonth, &myday, &myhour, &myminute);
  } while(myhour == 18 && mymonth == 0); //loop until good data is read
 
  myhour-=6;
  myyear+=2000; 

  //ANALOG PINS FOR TEMP SENSOR
  pinMode(A0, OUTPUT);
  pinMode(A1, INPUT);
  pinMode(A2, OUTPUT);

  digitalWrite(A0, LOW);
  digitalWrite(A2, HIGH);

  // ANALOG PIN FOR BACKLIGHT
  pinMode(A5, OUTPUT);
  digitalWrite(A5, HIGH);

  //ANALOG PINS FOR LEDs
  pinMode(A8, OUTPUT);
  pinMode(A9, OUTPUT);
  pinMode(A10,OUTPUT);
  pinMode(13, OUTPUT);


  //need to change this to where appropriate
  digitalWrite(A8, LOW);
  digitalWrite(A9, LOW);
  digitalWrite(A10, LOW);
  digitalWrite(13, LOW);


  //initialize EEPROM (one time only)
  /*clearEEPROM();
  writeEEPROM();
  signed int start = 0;
  EEPROM.put(32,start);
  EEPROM.put(34, start);
  EEPROM.put(36, start); //account for year offset
  EEPROM.put(38, start);
  EEPROM.put(40, start);*/


  if(myhour>12){
    morning = false;
    myhour-=12;
  }

  //initialize variables from EEPROM
  int index = 0;
  for(int i = 0; i<8; i++){
    wkH[i] = EEPROM.read(index);
    wkM[i] = EEPROM.read(index+1);
    wkA[i] = EEPROM.read(index+2);
    wkT[i] = EEPROM.read(index+3);
    index+=4;
  }
  // EEPROM[32] = day, EEPROM[33] = month, EEPROM[34] = year, EEPROM[35] = hour, EEPROM[36] = min
  signed int value;
  EEPROM.get(40, value);  
  myminute -= value;
  if(myminute<0){
    myminute += 60;
    myhour -= 1;
  } else if(myminute>59){
    myminute -= 60;
    myhour += 1;
  }

  EEPROM.get(38, value);
  myhour -= value;
  if(myhour<0){
    myhour += 24;
    myday -= 1;
  } else if(myhour>23){
    myhour -= 24;
    myday += 1;
  }

  EEPROM.get(34, value);
  mymonth -= value; 
  if(mymonth<1){
    mymonth += 12;
    myyear -= 1;
  } else if(mymonth>12){
    mymonth -= 12;
    myyear += 1;
  } 

  EEPROM.get(32, value); 
  myday -= value;
  if(myday<1){
    mymonth -= 1;
    if((mymonth==4) || (mymonth==6) || (mymonth==9) || (mymonth==11)){
      myday += 30;
    } else if(mymonth == 2){
      myday += 28;
    } else {
      myday += 31;
    }
  } else if(((mymonth==4) || (mymonth==6) || (mymonth==9) || (mymonth==11)) && (myday>30)){
    myday -= 30;
    mymonth += 1;
  } else if((mymonth == 2) && (myday>28)){
    myday -= 28;
    mymonth += 1;
  } else if(((mymonth == 1) || (mymonth == 3) || (mymonth == 5) || (mymonth == 7) || (mymonth == 8) || (mymonth == 10) || (mymonth == 12)) && (myday>31)){
    myday -= 31;
    mymonth += 1;
  }
  
  if(mymonth<1){
    mymonth += 12;
    myyear -= 1;
  } else if(mymonth>12){
    mymonth -= 12;
    myyear += 1;
  }

  EEPROM.get(36, value);
  myyear -= value;

  //initial temperature read
  int rawvoltage= analogRead(outputpin);
  float millivolts= (rawvoltage/1024.0) * 5000;
  
  float fahrenheit= millivolts/10;
  
  float celsius= (fahrenheit - 32) * (5.0/9.0);
  
  temper = fahrenheit;
  delay(1000);
  

  tft.begin();
 
  if (! ctp.begin(40)) {  // pass in 'sensitivity' coefficient
    Serial.println("Couldn't start FT6206 touchscreen controller");
    while (1);
  }
  
  
  yield();

  if (!SD.begin(SD_CS)) {
    Serial.println("failed!");
  }
  mySetTime();
  home_page();
}

void loop() {
  time_t left = now();
  if(left-tempRead>1){
    int rawvoltage= analogRead(outputpin);
    float millivolts= (rawvoltage/1024.0) * 5000;
    float fahrenheit= millivolts/10;
    temper = fahrenheit;
    float celsius= (temper - 32) * (5.0/9.0);
    tempRead = now();
  }
  
  if(view<8){
    time_t nt = now();
    if(nt-update>9){
      if(systemActive(temper2)){
        modeCheck();
      }
      mainViewWriting();
    }
  }
  
  // Wait for a touch or timeout in certain views after 10 seconds
  if ((! ctp.touched()) && (view>7) && (view<14)) {
    time_t nt = now();
    if(nt-t>9){
      modeCheck();
    } else {
        return;
      }
  } else if (! ctp.touched()){    //Wait for a touch
    time_t nt = now();
    if((nt-touched>30) && (!isDim)){
      dimScreen();
      return;
    }
    return;
  }
  
  // Retrieve a point
  TS_Point p = ctp.getPoint();
  TS_Point p2 = ctp.getPoint();
  touched = now();
  brightScreen();
  

  // flip it around to match the screen.
  p.x = map(p2.y, 0, 320, 0, 320);
  p.y = map(p2.x, 0, 240, 240, 0);

  
  //assign instructions based on what locations was touched and what view we are in
  switch(view) {
    case 1:
    case 2:
    case 3:
      if((184<p.x) && (p.x<319) && (0<p.y) && (p.y<72)){
        if(heating){
          heatOn();
        } else {
          heatOff();
        }
      } 
      break;
    case 4:
    case 5:
      if((184<p.x) && (p.x<319) && (0<p.y) && (p.y<72)){
        if(cooling){
          coolOn();
        } else {
          coolOff();
        }
      } 
      break;
    case 6:
    case 7:
      if((184<p.x) && (p.x<319) && (0<p.y) && (p.y<72)){
        home_page();
      } 
      break;
    case 8:
    case 9:
    case 10:
    case 11:
      if((p.x<229) && (p.y<120)){
        progWeekTT();
      } else if((p.x<229) && (121<p.y)){
        progEndTT();
      } else if(230<p.x){
        if((p.y<121) && wkVer){
          wkED = !wkED;
        } else if(wkendVer){
          wkendED = !wkendED;
        }
        if(!wkED && !wkendED){
          progDD();
        } else if(!wkED && wkendVer){
          progDE();
        } else if(!wkendED && wkVer){
          progED();
        } else if(wkVer && wkendVer){
          progEE();
        }
      }
      break;
    case 12:
      if(p.x<29){
        if(p.y<59){
          wkTimeD(0);
        } else if(p.y<119){
          wkTimeD(1);
        } else if(p.y<179){
          wkTimeD(2);
        } else {
          wkTimeD(3);
        }
      } else if(129<p.x && p.x<159){
        if(p.y<59){
          wkTimeU(0);
        } else if(p.y<119){
          wkTimeU(1);
        } else if(p.y<179){
          wkTimeU(2);
        } else {
          wkTimeU(3);
        }
      } else if(160<p.x && p.x<189){
        if(p.y<59){
          wkTempD(0);
        } else if(p.y<119){
          wkTempD(1);
        } else if(p.y<179){
          wkTempD(2);
        } else {
          wkTempD(3);
        }
      } else if(290<p.x){
        if(p.y<59){
          wkTempU(0);
        } else if(p.y<119){
          wkTempU(1);
        } else if(p.y<179){
          wkTempU(2);
        } else {
          wkTempU(3);
        }
      }
      clearEEPROM();
      writeEEPROM();
      delay(300);
      break;
    case 13:
      if(p.x<29){
        if(p.y<59){
          wkTimeD(4);
        } else if(p.y<119){
          wkTimeD(5);
        } else if(p.y<179){
          wkTimeD(6);
        } else {
          wkTimeD(7);
        }
      } else if(129<p.x && p.x<159){
        if(p.y<59){
          wkTimeU(4);
        } else if(p.y<119){
          wkTimeU(5);
        } else if(p.y<179){
          wkTimeU(6);
        } else {
          wkTimeU(7);
        }
      } else if(160<p.x && p.x<189){
        if(p.y<59){
          wkTempD(4);
        } else if(p.y<119){
          wkTempD(5);
        } else if(p.y<179){
          wkTempD(6);
        } else {
          wkTempD(7);
        }
      } else if(290<p.x){
        if(p.y<59){
          wkTempU(4);
        } else if(p.y<119){
          wkTempU(5);
        } else if(p.y<179){
          wkTempU(6);
        } else {
          wkTempU(7);
        }
      }
      clearEEPROM();
      writeEEPROM();
      delay(300);
      break;
   case 14:
      if((60<p.x && p.x<99) && (14<p.y && p.y<39) && mymonth<12){
        mymonth++;
        offMonth++;
      } else if((60<p.x && p.x<99) && (150<p.y && p.y<175) && mymonth>1){
        mymonth--;
        offMonth--;
      } else if((140<p.x && p.x<179) && (14<p.y && p.y<39) && myday<31){
        myday++;
        offDay++;
      } else if((140<p.x && p.x<179) && (150<p.y && p.y<175) && myday>1){
        myday--;
        offDay--;
      } else if((220<p.x && p.x<259) && (14<p.y && p.y<39) && myyear<9999){
        myyear++;
        offYear++;
      } else if((220<p.x && p.x<259) && (150<p.y && p.y<175) && myyear>1){
        myyear--;
        offYear--;
      } else if((219<p.x && p.x<319) && (200<p.y && p.y<239)){
        setTime();
      }
      if((60<p.x && p.x<259) && (14<p.y && p.y<175)){
        tft.fillRect(0, 50, 320, 80, ILI9341_WHITE);
        dateWrite();
      }
      break;
    case 15:
      if((60<p.x && p.x<99) && (14<p.y && p.y<39) && myhour<12){
        myhour++;
        offHour++;
      } else if((60<p.x && p.x<99) && (150<p.y && p.y<175) && myhour>1){
        myhour--;
        offHour--;
      } else if((140<p.x && p.x<179) && (14<p.y && p.y<39) && myminute<59){
        myminute++;
        offMin++;
      } else if((140<p.x && p.x<179) && (150<p.y && p.y<175) && myminute>0){
        myminute--;
        offMin--;
      } else if((220<p.x && p.x<259) && ((14<p.y && p.y<39) || (150<p.y && p.y<175))){
        if(morning){
          offHour += 12;
        }
        else{
          offHour-=12;
        }
        morning = !morning;
        delay(250);
      } else if((219<p.x && p.x<319) && (200<p.y && p.y<239)){
        writeTimeEEPROM();
        mySetTime();
        modeCheck();
      }
      if((60<p.x && p.x<259) && (14<p.y && p.y<175)){
        tft.fillRect(0, 50, 320, 80, ILI9341_WHITE);
        myWrite(myhour, 50, 80, 110, 0, 80, 5, 0, false, true, false, false);
        myWrite(myminute, 135, 165, 0, 0, 80, 5, 0, false, false, false, false);
        myWrite(0, 210, 240, 0, 0, 80, 5, 0, morning, true, true, false);
      }
      break;
    default:
      if((184<p.x) && (p.x<319) && (0<p.y) && (p.y<72)){
        if(heating){
          autoHeat();
        } else if(cooling){
          autoCool();
        } else {
          autoOff();
        }
      }
      break;
  }
  
  if(view<8){
    if((0<p.x) && (p.x<137) && (215<p.y) && (p.y<239)){
      setDate();
    } else if((138<p.x) && (p.x<251) && (215<p.y) && (p.y<239)){
      if(!wkED && !wkendED){
        progDD();
      } else if(!wkED){
        progDE();
      } else if(!wkendED){
        progED();
      } else {
        progEE();
      }
    } else if((252<p.x && p.x<319) && (145<p.y && p.y<191) && (temper2 < 80)){ //inc set point
      temper2++;
      tft.fillRect(139, 146, 111, 67, ILI9341_WHITE);
      myWrite(temper2, 148, 178, 208, 218, 160, 5, 1, true, false, false, false);
      delay(250);
      //redraw or reload page
    } else if((252<p.x && p.x<319) && (192<p.y && p.y<239) && (temper2 > 60)){ //dec set point
      temper2--;
      tft.fillRect(139, 146, 111, 67, ILI9341_WHITE);
      myWrite(temper2, 148, 178, 208, 218, 160, 5, 1, true, false, false, false);
      delay(250);
      //redraw or reload page
    } else if((252<p.x && p.x<319) && (73<p.y && p.y<144)){
      if(hold){
        tft.fillRect(253, 74, 67, 70, ILI9341_RED);
        hold=false;
        if((1<weekday()) && (weekday()<7) && wkED){
          int i = 0;
          bool get = true;
          while(get){
            int hour1 = wkH[i];
            if(!wkA[i]){
              hour1 += 12;
            }
            if((weekday() == 2) && (i == 0) && (hour() < hour1) && (minute() < wkM[0])){
              temper2 = wkT[7];
              get = false;
            } else if((hour() < hour1) && (minute() < wkM[i])){
              temper2 = wkT[i-1];
              get = false;
            } else if(i == 3){
              get = false;
            } else {
              i++;
            }
          }
        }
        if(((weekday() == 1) || (weekday() == 7)) && wkendED){
          int i = 4;
          bool get = true;
          while(get){
            int hour1 = wkH[i];
            if(!wkA[i]){
              hour1 += 12;
            }
            if((weekday() == 7) && (i == 4) && (hour() < hour1) && (minute() < wkM[4])){
              temper2 = wkT[3];
              get = false;
            } else if((hour() < hour1) && (minute() < wkM[i])){
              temper2 = wkT[i-1];
              get = false;
            } else if(i == 7){
              get = false;
            } else {
              i++;
            }
          }
        }
        tft.fillRect(139, 146, 111, 67, ILI9341_WHITE);
        myWrite(temper2, 148, 178, 208, 218, 160, 5, 1, true, false, false, false);
       } else {
        tft.fillRect(253, 74, 67, 70, ILI9341_GREEN);
        hold = true;
       }
       delay(500);
    }
  }
}

// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance.

#define BUFFPIXEL 20

void bmpDraw(char *filename, int16_t x, int16_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col, x2, y2, bx1, by1;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print(F("File not found"));
    return;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        x2 = x + bmpWidth  - 1; // Lower-right corner
        y2 = y + bmpHeight - 1;
        if((x2 >= 0) && (y2 >= 0)) { // On screen?
          w = bmpWidth; // Width/height of section to load/display
          h = bmpHeight;
          bx1 = by1 = 0; // UL coordinate in BMP file
          if(x < 0) { // Clip left
            bx1 = -x;
            x   = 0;
            w   = x2 + 1;
          }
          if(y < 0) { // Clip top
            by1 = -y;
            y   = 0;
            h   = y2 + 1;
          }
          if(x2 >= tft.width())  w = tft.width()  - x; // Clip right
          if(y2 >= tft.height()) h = tft.height() - y; // Clip bottom
  
          // Set TFT address window to clipped image bounds
          tft.startWrite(); // Requires start/end transaction now
          tft.setAddrWindow(x, y, w, h);
  
          for (row=0; row<h; row++) { // For each scanline...
  
            // Seek to start of scan line.  It might seem labor-
            // intensive to be doing this on every line, but this
            // method covers a lot of gritty details like cropping
            // and scanline padding.  Also, the seek only takes
            // place if the file position actually needs to change
            // (avoids a lot of cluster math in SD library).
            if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
              pos = bmpImageoffset + (bmpHeight - 1 - (row + by1)) * rowSize;
            else     // Bitmap is stored top-to-bottom
              pos = bmpImageoffset + (row + by1) * rowSize;
            pos += bx1 * 3; // Factor in starting column (bx1)
            if(bmpFile.position() != pos) { // Need seek?
              tft.endWrite(); // End TFT transaction
              bmpFile.seek(pos);
              buffidx = sizeof(sdbuffer); // Force buffer reload
              tft.startWrite(); // Start new TFT transaction
            }
            for (col=0; col<w; col++) { // For each pixel...
              // Time to read more pixel data?
              if (buffidx >= sizeof(sdbuffer)) { // Indeed
                tft.endWrite(); // End TFT transaction
                bmpFile.read(sdbuffer, sizeof(sdbuffer));
                buffidx = 0; // Set index to beginning
                tft.startWrite(); // Start new TFT transaction
              }
              // Convert pixel from BMP to TFT format, push to display
              b = sdbuffer[buffidx++];
              g = sdbuffer[buffidx++];
              r = sdbuffer[buffidx++];
              tft.writePixel(tft.color565(r,g,b));
            } // end pixel
          } // end scanline
          tft.endWrite(); // End last TFT transaction
        } // end onscreen
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) Serial.println(F("BMP format not recognized."));
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

//draw the base page
void home_page(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSOff.bmp", 0, 0);
  digitalWrite(A10, LOW);
  digitalWrite(13, LOW);//turn off lights
  mainViewWriting();
  view = 0;
  mode = 0;
}

//draw the main page with the HVAC on auto but heating and cooling off
void autoOff(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMA_SO.bmp", 0, 0);
  digitalWrite(A10, LOW);
  digitalWrite(13, LOW);//turn off lights
  mainViewWriting();
  view = 1;
  mode = 1;
}

//draw the main page with the HVAC on auto and cooling on
void autoCool(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMA_SA.bmp", 0, 0);
  mainViewWriting();
  digitalWrite(A10, LOW);
  digitalWrite(13,10); //blue light on
  view = 2;
  mode = 1;
}

//draw the main page with the HVAC on auto and heating on
void autoHeat(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMA_SM.bmp", 0, 0);
  mainViewWriting();
  digitalWrite(A10, HIGH);//red light on
  digitalWrite(13, LOW);
  view = 3;
  mode = 1;
}

//draw the main page with the HVAC on heating but heater turned off
void heatOff(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMH_SO.bmp", 0, 0);
  mainViewWriting();
  digitalWrite(A10, LOW);//turn off lights
  digitalWrite(13, LOW);
  view = 4;
  mode = 2;
}

//draw the main page with the HVAC on heating and heater turned on
void heatOn(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMH_SM.bmp", 0, 0);
  mainViewWriting();
  digitalWrite(A10, HIGH); //red light on
  digitalWrite(13, LOW);
  view = 5;
  mode = 2;
}

//draw the main page with the HVAC on cooling and AC turned off
void coolOff(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMC_SO.bmp", 0, 0);
  digitalWrite(A10, LOW);
  digitalWrite(13, LOW);//turn off lights
  mainViewWriting();
  view = 6;
  mode = 3;
}

//draw the main page with the HVAC on cooling and AC turned on
void coolOn(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMC_SA.bmp", 0, 0);
  mainViewWriting();
  digitalWrite(A10, LOW);
  digitalWrite(13, 10);//blue light on
  view = 7;
  mode = 3;
}

//more code condensing
void mainViewWriting(){
  myWrite(temper, 30, 70, 110, 130, 44, 7, 2, true, false, false, false);
  myWrite(hourFormat12(), 34, 57, 75, 0, 165, 4, 0, false, true, false, false);
  myWrite(minute(), 90, 113, 0, 0, 165, 4, 0, false, false, false, false);
  myWrite(weekday(), 5, 0, 0, 0, 165, 4, 0, false, false, true, false);
  if(hold){
    tft.fillRect(253, 74, 67, 70, ILI9341_GREEN);
  } else {
    tft.fillRect(253, 74, 67, 70, ILI9341_RED);
    if((1<weekday()) && (weekday()<7) && wkED){
      int i = 0;
      bool get = true;
      while(get){
        int hour1 = wkH[i];
        if(!wkA[i]){
          hour1 += 12;
        }
        if((weekday() == 2) && (i == 0) && (hour() < hour1) && (minute() < wkM[0])){
          temper2 = wkT[7];
          get = false;
        } else if((hour() < hour1) && (minute() < wkM[i])){
          temper2 = wkT[i-1];
          get = false;
        } else if(i == 3){
          get = false;
        } else {
          i++;
        }
      }
    }
    if(((weekday() == 1) || (weekday() == 7)) && wkendED){
      int i = 4;
      bool get = true;
      while(get){
        int hour1 = wkH[i];
        if(!wkA[i]){
          hour1 += 12;
        }
        if((weekday() == 7) && (i == 4) && (hour() < hour1) && (minute() < wkM[4])){
          temper2 = wkT[3];
          get = false;
        } else if((hour() < hour1) && (minute() < wkM[i])){
          temper2 = wkT[i-1];
          get = false;
        } else if(i == 7){
          get = false;
        } else {
          i++;
        }
      }
    }
  }
  myWrite(temper2, 148, 178, 208, 218, 160, 5, 1, true, false, false, false);
  update = now();
}

//draw the first page for programming set points with both disabled
void progDD(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("PSPDD.bmp", 0, 0);
  view = 8;
  wkED = false;
  wkendED = false;
  t = now();
}

//draw the first page for programming set points with week disabled
void progDE(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("PSPDE.bmp", 0, 0);
  view = 9;
  wkED = false;
  wkendED = true;
  t = now();
}

//draw the first page for programming set points with weekend disabled
void progED(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("PSPED.bmp", 0, 0);
  view = 10;
  wkED = true;
  wkendED = false;
  t = now();
}

//draw the first page for programming set points with both enabled
void progEE(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("PSPEE.bmp", 0, 0);
  view = 11;
  wkED = true;
  wkendED = true;
  t = now();
}

//draw the page where we actually program the set points for the week
void progWeekTT(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("PSPTT.bmp", 0, 0);
  for(int i=0; i<4; i++){
    myWrite(wkT[i], 210, 230, 250, 260, (20+60*i), 3, 1, true, false, false, false);
    myWrite(wkH[i], 32, 44, 54, 0, (20+60*i), 2, 0, false, true, false, false);
    myWrite(wkM[i], 62, 74, 0, 0, (20+60*i), 2, 0, false, false, false, false);
    myWrite(wkA[i], 85, 98, 0, 0, (20+60*i), 2, 0, wkA[i], true, true, false);
  }
  view = 12;
  t = now();
  wkVer = true;
}

//draw the page where we actually program the set points for the weekend
void progEndTT(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("PSPTT.bmp", 0, 0);
  for(int i=4; i<8; i++){
    myWrite(wkT[i], 210, 230, 250, 260, (20+60*(i%4)), 3, 1, true, false, false, false);
    myWrite(wkH[i], 32, 44, 54, 0, (20+60*(i%4)), 2, 0, false, true, false, false);
    myWrite(wkM[i], 62, 74, 0, 0, (20+60*(i%4)), 2, 0, false, false, false, false);
    myWrite(wkA[i], 85, 98, 0, 0, (20+60*(i%4)), 2, 0, wkA[i], true, true, false);
  }
  view = 13;
  t = now();
  wkendVer = true;
}

//draw the page where we set the date
void setDate(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("SetDate.bmp", 0, 0);
  dateWrite();
  view = 14;
}

//draw the page where we set the time
void setTime(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("SetTime.bmp", 0, 0);
  myWrite(myhour, 50, 80, 0, 0, 80, 5, 0, false, false, false, false);
  myWrite(myminute, 135, 165, 0, 0, 80, 5, 0, false, false, false, false);
  myWrite(0, 210, 240, 0, 0, 80, 5, 0, morning, true, true, false);
  view = 15;
}

//function that writes most characters on the screen
void myWrite(int value, int x1, int x2, int x3, int x4, int y, int size1, int size2, bool myTemp, bool hour, bool day, bool year){
  if(hour && day){
    if(myTemp){
      tft.drawChar(x1, y, 65, ILI9341_BLACK, ILI9341_WHITE, size1);
    } else {
      tft.drawChar(x1, y, 80, ILI9341_BLACK, ILI9341_WHITE, size1);
    }
    tft.drawChar(x2, y, 77, ILI9341_BLACK, ILI9341_WHITE, size1);
  } else if(!day && !year){
    char a[2];
    if(hour && value==0){
      value = 12;
    }
    String(value).toCharArray(a,3);
    if(value<10){
      a[1]=a[0];
      a[0]=79;
    }
    tft.drawChar(x1, y, byte(a[0]), ILI9341_BLACK, ILI9341_WHITE, size1);
    tft.drawChar(x2, y, byte(a[1]), ILI9341_BLACK, ILI9341_WHITE, size1);
    if(myTemp){
      tft.drawChar(x3, y, 79, ILI9341_BLACK, ILI9341_WHITE, size2);
      tft.drawChar(x4, y, 70, ILI9341_BLACK, ILI9341_WHITE, size1);
    }
    if(hour){
      tft.drawChar(x3, y, 58, ILI9341_BLACK, ILI9341_WHITE, size1);
    }
  } else if(!day){
    char b[5];
    String(value).toCharArray(b,6);
    tft.drawChar(x1, y, byte(b[0]), ILI9341_BLACK, ILI9341_WHITE, size1);
    tft.drawChar(x2, y, byte(b[1]), ILI9341_BLACK, ILI9341_WHITE, size1);
    tft.drawChar(x3, y, byte(b[2]), ILI9341_BLACK, ILI9341_WHITE, size1);
    tft.drawChar(x4, y, byte(b[3]), ILI9341_BLACK, ILI9341_WHITE, size1);
  } else {
    //determines day of the week to be printed
    switch(value) {
      case 1:
        tft.drawChar(x1, y, 83, ILI9341_BLACK, ILI9341_WHITE, size1);
        break;
      case 2:
        tft.drawChar(x1, y, 77, ILI9341_BLACK, ILI9341_WHITE, size1);
        break;
      case 3:
        tft.drawChar(x1, y, 84, ILI9341_BLACK, ILI9341_WHITE, size1);
        break;
      case 4:
        tft.drawChar(x1, y, 87, ILI9341_BLACK, ILI9341_WHITE, size1);
        break;
      case 5:
        tft.drawChar(x1, y, 82, ILI9341_BLACK, ILI9341_WHITE, size1);
        break;
      case 6:
        tft.drawChar(x1, y, 70, ILI9341_BLACK, ILI9341_WHITE, size1);
        break;
      default:
        tft.drawChar(x1, y, 65, ILI9341_BLACK, ILI9341_WHITE, size1);
        break;
    }
  }
}

//function that writes the month on the screen, and then calls myWrite()
void dateWrite(){
  switch(mymonth){
    case 1:
      tft.drawChar(10, 80, 74, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(40, 80, 65, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(70, 80, 78, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(95, 85, 46, ILI9341_BLACK, ILI9341_WHITE, 4);
      break;
    case 2:
      tft.drawChar(10, 80, 70, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(40, 80, 69, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(70, 80, 66, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(95, 85, 46, ILI9341_BLACK, ILI9341_WHITE, 4);
      break;
    case 3:
      tft.drawChar(10, 80, 77, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(40, 80, 65, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(70, 80, 82, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(95, 85, 46, ILI9341_BLACK, ILI9341_WHITE, 4);
      break;
    case 4:
      tft.drawChar(10, 80, 65, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(40, 80, 80, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(70, 80, 82, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(95, 85, 46, ILI9341_BLACK, ILI9341_WHITE, 4);
      break;
    case 5:
      tft.drawChar(10, 80, 77, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(40, 80, 65, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(70, 80, 89, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(95, 85, 46, ILI9341_BLACK, ILI9341_WHITE, 4);
      break;
    case 6:
      tft.drawChar(10, 80, 74, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(40, 80, 85, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(70, 80, 78, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(95, 85, 46, ILI9341_BLACK, ILI9341_WHITE, 4);
      break;
    case 7:
      tft.drawChar(10, 80, 74, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(40, 80, 85, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(70, 80, 76, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(95, 85, 46, ILI9341_BLACK, ILI9341_WHITE, 4);
      break;
    case 8:
      tft.drawChar(10, 80, 65, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(40, 80, 85, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(70, 80, 71, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(95, 85, 46, ILI9341_BLACK, ILI9341_WHITE, 4);
      break;
    case 9:
      tft.drawChar(10, 80, 83, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(40, 80, 69, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(70, 80, 80, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(95, 85, 46, ILI9341_BLACK, ILI9341_WHITE, 4);
      break;
    case 10:
      tft.drawChar(10, 80, 79, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(40, 80, 67, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(70, 80, 84, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(95, 85, 46, ILI9341_BLACK, ILI9341_WHITE, 4);
      break;
    case 11:
      tft.drawChar(10, 80, 78, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(40, 80, 79, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(70, 80, 86, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(95, 85, 46, ILI9341_BLACK, ILI9341_WHITE, 4);
      break;
    default:
      tft.drawChar(10, 80, 68, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(40, 80, 69, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(70, 80, 67, ILI9341_BLACK, ILI9341_WHITE, 5);
      tft.drawChar(95, 85, 46, ILI9341_BLACK, ILI9341_WHITE, 4);
      break;
  }
  myWrite(myday, 130, 160, 0, 0, 80, 5, 0, false, false, false, false);
  myWrite(myyear, 200, 230, 260, 290, 80, 5, 0, false, false, false, true);
}

void mySetTime(){
  if(!morning){
      setTime(myhour + 12, myminute, 0, myday, mymonth, myyear);
  }
  else{
      setTime(myhour, myminute, 0, myday, mymonth, myyear);
  }
}

bool systemActive(int temperature){
  bool x = heating;
  bool y = cooling;
  if(temperature-temper>1){
    heating = true;
    cooling = false;
  } else if(temper-temperature>1){
    cooling = true;
    heating = false;
  } else if(temper - temperature == 0){
    cooling = false;
    heating = false;
  }
  if((((x!=heating) && (mode!=3)) || ((y!=cooling) && (mode!=2))) && (mode!=0)){
    return true;
  } else {
    return false;
  }
}

void modeCheck(){
  switch(mode){
    case 1:
      if(heating){
        autoHeat();
      } else if(cooling){
        autoCool();
      } else {
        autoOff();
      }
      break;
    case 2:
      if(heating){
        heatOn();
      } else {
        heatOff();
      }
      break;
    case 3:
      if(cooling){
        coolOn();
      } else {
        coolOff();
      }
      break;
    default:
      home_page();
      break;
  }
}

void wkTimeD(int i){
  if(wkM[i]>0){
    wkM[i] = wkM[i]-5;
  } else {
    if(wkH[i]>0){
      wkH[i]=wkH[i]-1;
      wkM[i]=55;
    } else {
      wkA[i] = !wkA[i];
      wkH[i] = 11;
      wkM[i] = 55;
    }
  }
  myWrite(wkH[i], 32, 44, 54, 0, (20+60*(i%4)), 2, 0, false, true, false, false);
  myWrite(wkM[i], 62, 74, 0, 0, (20+60*(i%4)), 2, 0, false, false, false, false);
  myWrite(wkA[i], 85, 98, 0, 0, (20+60*(i%4)), 2, 0, wkA[i], true, true, false);
  t = now();
}

void wkTimeU(int i){
  if(wkM[i]<55){
    wkM[i] = wkM[i]+5;
  } else {
    if(wkH[i]<11){
      wkH[i]=wkH[i]+1;
      wkM[i]=0;
    } else {
      wkA[i] = !wkA[i];
      wkH[i] = 0;
      wkM[i] = 0;
    }
  }
  myWrite(wkH[i], 32, 44, 54, 0, (20+60*(i%4)), 2, 0, false, true, false, false);
  myWrite(wkM[i], 62, 74, 0, 0, (20+60*(i%4)), 2, 0, false, false, false, false);
  myWrite(wkA[i], 85, 98, 0, 0, (20+60*(i%4)), 2, 0, wkA[i], true, true, false);
  t = now();
}

void wkTempD(int i){
  if(wkT[i]>60){
    wkT[i] = wkT[i]-1;
  }
  myWrite(wkT[i], 210, 230, 250, 260, (20+60*(i%4)), 3, 1, true, false, false, false);
  t = now();
}

void wkTempU(int i){
  if(wkT[i]<80){
    wkT[i] = wkT[i]+1;
  } 
  myWrite(wkT[i], 210, 230, 250, 260, (20+60*(i%4)), 3, 1, true, false, false, false);
  t = now();
}

//from Justin
void getTime(int *YEAR,int *MONTH, int *DAY, int *HOUR, int *MIN)
{
  char c = GPS.read();

  if (GPS.newNMEAreceived()) 
  {
    if (!GPS.parse(GPS.lastNMEA())) 
      return;  // we can fail to parse a sentence in which case we should just wait for another
  }
    *YEAR = GPS.year;
    *MONTH = GPS.month;
    *DAY = GPS.day;
    *HOUR = GPS.hour;
    *MIN = GPS.minute;
    int* SEC = GPS.seconds;

    //Validate data
    if (*SEC > 60)
      *MONTH = 0;
}

void clearEEPROM()
{
  for (int i = 0 ; i < EEPROM.length() ; i++) {
    if(EEPROM.read(i) != 0)                     //skip already "empty" addresses
    {
      EEPROM.write(i, 0);                       //write 0 to address i
    }
  }
}

//writes the values for set points to EEPROM
void writeEEPROM(){
  int index = 0;
  for(int i = 0; i<8; i++){
    EEPROM.write(index, wkH[i]);
    index++;
    EEPROM.write(index, wkM[i]);
    index++;
    EEPROM.write(index, wkA[i]);
    index++;
    EEPROM.write(index, wkT[i]);
    index++;
  }
}

// EEPROM[32] = day, EEPROM[34] = month, EEPROM[36] = year, EEPROM[38] = hour, EEPROM[40] = min
// inverse is stored to match logic to adjust current time when reading from EEPROM
void writeTimeEEPROM(){
  EEPROM.put(32, -offDay);
  EEPROM.put(34, -offMonth);
  EEPROM.put(36, -offYear);
  EEPROM.put(38, -offHour);
  EEPROM.put(40, -offMin);
}

void dimScreen(){  
  digitalWrite(A5, LOW);
  isDim = true; 
} 

void brightScreen(){  
  digitalWrite(A5, HIGH); 
  isDim = false;  
}
