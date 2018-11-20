// Time - Version: Latest 
#include <Time.h>
#include <TimeLib.h>

#include <Adafruit_FT6206.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_ILI9341.h" // Hardware-specific library
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <TouchScreen.h>

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

int wkT[]={70, 70, 70, 70};   //temperatures for the week
int weT[]={70, 70, 70, 70};   //temperatures for the weekend
int wkH[]={12, 6, 12, 6};     //hours for the week
int weH[]={12, 6, 12, 6};     //hours for the weekend
int wkM[]={0, 0, 0, 0};       //minutes for the week
int weM[]={0, 0, 0, 0};       //minutes for the weekend
bool wkA[] = {true, true, false, false};  //AM/PM for the week, AM is true
bool weA[] = {true, true, false, false};  //AM/PM for the weekend, AM is true

int myday = 1;       //the day of the month we are setting the clock to, 1-31
int mymonth = 1;     //the month we are setting the clock to, 1-12
int myyear = 2000;    //the year we are setting the clock to
int myhour = 12;       //the hour we are setting the clock to, 1-12
int myminute = 0;     //minute we are setting the clock to, 0-59
bool morning = true;  //AM when true, PM when false
bool wkED = false;    //enables programmed set points during the week, disabled when false
bool wkendED = false; //enables programmed set points during the weekend, disabled when false
bool cooling = false; //tracks if the AC is on, off when false
bool heating = false; //tracks if the heater is on, off when false
bool hold = true;     //tracks if we are overriding the programmed set points, overriding when true
time_t t = now();
time_t update = now();

void setup(void) {
  Serial.begin(9600);

  tft.begin();
 
  if (! ctp.begin(40)) {  // pass in 'sensitivity' coefficient
    Serial.println("Couldn't start FT6206 touchscreen controller");
    while (1);
  }
  
  Serial.println("Capacitive touchscreen started");
  
  yield();

  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("failed!");
  }
  Serial.println("OK!");
  mySetTime();
  home_page();
  //writeHoldTemp();

}

void loop() {
  /*for(uint8_t r=0; r<4; r++) {
    tft.setRotation(r);
    tft.fillScreen(ILI9341_BLUE);
    for(int8_t i=-2; i<1; i++) {
      bmpDraw("/purple.bmp",
        (tft.width()  / 2) + (i * 120),
        (tft.height() / 2) + (i * 160));
    }
  }*/
  
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
    return;
  }
  
  // Retrieve a point
  TS_Point p = ctp.getPoint();
  TS_Point p2 = ctp.getPoint();
  
 /*
  // Print out raw data from screen touch controller
  Serial.print("X = "); Serial.print(p.x);
  Serial.print("\tY = "); Serial.print(p.y);
  Serial.print(" -> ");
 */

  // flip it around to match the screen.
  p.x = map(p2.y, 0, 320, 0, 320);
  p.y = map(p2.x, 0, 240, 240, 0);

  // Print out the remapped (rotated) coordinates
  Serial.print("("); Serial.print(p.x);
  Serial.print(", "); Serial.print(p.y);
  Serial.println(")");
  
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
        if(p.y<121){
          wkED = !wkED;
        } else {
          wkendED = !wkendED;
        }
        if(!wkED && !wkendED){
          progDD();
        } else if(!wkED){
          progDE();
        } else if(!wkendED){
          progED();
        } else {
          progEE();
        }
      }
      break;
    case 12:
        //myWrite(myhour, 50, 80, 110, 0, 80, 5, 0, false, true, false, false);
        //myWrite(myminute, 135, 165, 0, 0, 80, 5, 0, false, false, false, false);
        //myWrite(0, 210, 240, 0, 0, 80, 5, 0, false, true, true, false);
      break;
    case 13:
    case 14:
      if((60<p.x && p.x<99) && (14<p.y && p.y<39) && mymonth<12){
        mymonth++;
      } else if((60<p.x && p.x<99) && (150<p.y && p.y<175) && mymonth>1){
        mymonth--;
      } else if((140<p.x && p.x<179) && (14<p.y && p.y<39) && myday<31){
        myday++;
      } else if((140<p.x && p.x<179) && (150<p.y && p.y<175) && myday>1){
        myday--;
      } else if((220<p.x && p.x<259) && (14<p.y && p.y<39) && myyear<9999){
        myyear++;
      } else if((220<p.x && p.x<259) && (150<p.y && p.y<175) && myyear>1){
        myyear--;
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
      } else if((60<p.x && p.x<99) && (150<p.y && p.y<175) && myhour>1){
        myhour--;
      } else if((140<p.x && p.x<179) && (14<p.y && p.y<39) && myminute<59){
        myminute++;
      } else if((140<p.x && p.x<179) && (150<p.y && p.y<175) && myminute>0){
        myminute--;
      } else if((220<p.x && p.x<259) && ((14<p.y && p.y<39) || (150<p.y && p.y<175))){
        morning = !morning;
        delay(250);
      } else if((219<p.x && p.x<319) && (200<p.y && p.y<239)){
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
  //trying to condense code
  switch(view) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
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
         } else {
          tft.fillRect(253, 74, 67, 70, ILI9341_GREEN);
          hold = true;
         }
         delay(500);
      }
      break;
    default:
      break;
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
  //tft.fillScreen(ILI9341_WHITE);
  tft.setRotation(3);
  bmpDraw("MSOff.bmp", 0, 0);
  mainViewWriting();
  view = 0;
  mode = 0;
}

//draw the main page with the HVAC on auto but heating and cooling off
void autoOff(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMA_SO.bmp", 0, 0);
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
  view = 2;
  mode = 1;
}

//draw the main page with the HVAC on auto and heating on
void autoHeat(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMA_SM.bmp", 0, 0);
  mainViewWriting();
  view = 3;
  mode = 1;
}

//draw the main page with the HVAC on heating but heater turned off
void heatOff(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMH_SO.bmp", 0, 0);
  mainViewWriting();
  view = 4;
  mode = 2;
}

//draw the main page with the HVAC on heating and heater turned on
void heatOn(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMH_SM.bmp", 0, 0);
  mainViewWriting();
  view = 5;
  mode = 2;
}

//draw the main page with the HVAC on cooling and AC turned off
void coolOff(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMC_SO.bmp", 0, 0);
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
  view = 7;
  mode = 3;
}

//more code condensing
void mainViewWriting(){
  myWrite(temper, 30, 70, 110, 130, 44, 7, 2, true, false, false, false);
  myWrite(temper2, 148, 178, 208, 218, 160, 5, 1, true, false, false, false);
  myWrite(hourFormat12(), 34, 57, 75, 0, 165, 4, 0, false, true, false, false);
  myWrite(minute(), 90, 113, 0, 0, 165, 4, 0, false, false, false, false);
  myWrite(weekday(), 5, 0, 0, 0, 165, 4, 0, false, false, true, false);
  if(hold){
    tft.fillRect(253, 74, 67, 70, ILI9341_GREEN);
  } else {
    tft.fillRect(253, 74, 67, 70, ILI9341_RED);
  }
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
}

//draw the page where we actually program the set points for the weekend
void progEndTT(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("PSPTT.bmp", 0, 0);
  for(int i=0; i<4; i++){
    myWrite(weT[i], 210, 230, 250, 260, (20+60*i), 3, 1, true, false, false, false);
    myWrite(weH[i], 32, 44, 54, 0, (20+60*i), 2, 0, false, true, false, false);
    myWrite(weM[i], 62, 74, 0, 0, (20+60*i), 2, 0, false, false, false, false);
    myWrite(weA[i], 85, 98, 0, 0, (20+60*i), 2, 0, weA[i], true, true, false);
  }
  view = 13;
  t = now();
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

//THIS ONE WORKS
/*void writeTemp(int temperature, int x1, int x2, int x3, int x4, int y, int size1, int size2){
  char a[2];
  String(temperature).toCharArray(a,3);
  tft.drawChar(x1, y, byte(a[0]), ILI9341_BLACK, ILI9341_WHITE, size1);
  tft.drawChar(x2, y, byte(a[1]), ILI9341_BLACK, ILI9341_WHITE, size1);
  tft.drawChar(x3, y, 79, ILI9341_BLACK, ILI9341_WHITE, size2);
  tft.drawChar(x4, y, 70, ILI9341_BLACK, ILI9341_WHITE, size1);
}*/

void mySetTime(){
  if(!morning){
    myhour = myhour + 12;
  }
  setTime(myhour, myminute, 0, myday, mymonth, myyear);
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
