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

int view = 0;
int mode = 0;
int temper = 70;
int temper2 = 70;
bool wkED = false;
bool wkendED = false;
bool cooling = false;
bool heating = false;
time_t t = now();

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
  
  // Wait for a touch
  if ((! ctp.touched()) && (view>7) && (view<13)) {
    time_t nt = now();
    if(nt-t>9)
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
      } else {
        return;
      }
  } else if (! ctp.touched()){
    return;
  }
  
  TS_Point p = ctp.getPoint();
  TS_Point p2 = ctp.getPoint();
  // Retrieve a point  
  /*while(ctp.touched()){
    p = ctp.getPoint();
    p2 = ctp.getPoint();
  }*/
  
 /*
  // Print out raw data from screen touch controller
  Serial.print("X = "); Serial.print(p.x);
  Serial.print("\tY = "); Serial.print(p.y);
  Serial.print(" -> ");
 */

  // flip it around to match the screen.
  //p.x = map(p.x, 0, 240, 240, 0);
  //p.y = map(p.y, 0, 320, 320, 0);
  p.x = map(p2.y, 0, 320, 0, 320);
  p.y = map(p2.x, 0, 240, 240, 0);

  // Print out the remapped (rotated) coordinates
  Serial.print("("); Serial.print(p.x);
  Serial.print(", "); Serial.print(p.y);
  Serial.println(")");
  
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
      } else if((0<p.x) && (p.x<137) && (215<p.y) && (p.y<239)){
        setDate();
      } else if((138<p.x) && (p.x<251) && (215<p.y) && (p.y<239)){
        if(!wkED && !wkendED){
          progDD();
        } else if(!wkED && wkendED){
          progDE();
        } else if(wkED && !wkendED){
          progED();
        } else if(wkED && wkendED){
          progEE();
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
      } else if((0<p.x) && (p.x<137) && (215<p.y) && (p.y<239)){
        setDate();
      } else if((138<p.x) && (p.x<251) && (215<p.y) && (p.y<239)){
        if(!wkED && !wkendED){
          progDD();
        } else if(!wkED && wkendED){
          progDE();
        } else if(wkED && !wkendED){
          progED();
        } else if(wkED && wkendED){
          progEE();
        }
      }
      break;
    case 6:
    case 7:
      if((184<p.x) && (p.x<319) && (0<p.y) && (p.y<72)){
        home_page();
      } else if((0<p.x) && (p.x<137) && (215<p.y) && (p.y<239)){
        setDate();
      } else if((138<p.x) && (p.x<251) && (215<p.y) && (p.y<239)){
        if(!wkED && !wkendED){
          progDD();
        } else if(!wkED && wkendED){
          progDE();
        } else if(wkED && !wkendED){
          progED();
        } else if(wkED && wkendED){
          progEE();
        }
      }
      break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    default:
      if((184<p.x) && (p.x<319) && (0<p.y) && (p.y<72)){
        if(heating){
          autoHeat();
        } else if(cooling){
          autoCool();
        } else {
          autoOff();
        }
      } else if((0<p.x) && (p.x<137) && (215<p.y) && (p.y<239)){
        setDate();
      } else if((138<p.x) && (p.x<251) && (215<p.y) && (p.y<239)){
        if(!wkED && !wkendED){
          progDD();
        } else if(!wkED && wkendED){
          progDE();
        } else if(wkED && !wkendED){
          progED();
        } else if(wkED && wkendED){
          progEE();
        }
      }
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

void home_page(){
  tft.fillScreen(ILI9341_BLACK);
  //tft.fillScreen(ILI9341_WHITE);
  tft.setRotation(3);
  bmpDraw("MSOff.bmp", 0, 0);
  writeTemp();
  //writeHoldTemp();
  view = 0;
  mode = 0;
}

void autoOff(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMA_SO.bmp", 0, 0);
  writeTemp();
  view = 1;
  mode = 1;
}

void autoCool(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMA_SA.bmp", 0, 0);
  writeTemp();
  view = 2;
  mode = 1;
}

void autoHeat(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMA_SM.bmp", 0, 0);
  writeTemp();
  view = 3;
  mode = 1;
}

void heatOff(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMH_SO.bmp", 0, 0);
  writeTemp();
  view = 4;
  mode = 2;
}

void heatOn(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMH_SM.bmp", 0, 0);
  writeTemp();
  view = 5;
  mode = 2;
}

void coolOff(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMC_SO.bmp", 0, 0);
  writeTemp();
  view = 6;
  mode = 3;
}

void coolOn(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("MSMC_SA.bmp", 0, 0);
  writeTemp();
  view = 7;
  mode = 3;
}

void progDD(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("PSPDD.bmp", 0, 0);
  view = 8;
  wkED = false;
  wkendED = false;
  t = now();
}

void progDE(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("PSPDE.bmp", 0, 0);
  view = 9;
  wkED = false;
  wkendED = true;
  t = now();
}

void progED(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("PSPED.bmp", 0, 0);
  view = 10;
  wkED = true;
  wkendED = false;
  t = now();
}

void progEE(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("PSPEE.bmp", 0, 0);
  view = 11;
  wkED = true;
  wkendED = true;
  t = now();
}

void progTT(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("PSPTT.bmp", 0, 0);
  view = 12;
  t = now();
}

void setDate(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("SetDate.bmp", 0, 0);
  view = 13;
}

void setTime(){
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  bmpDraw("SetTime.bmp", 0, 0);
  view = 14;
}

void writeTemp(){
  char a[2];
  String(temper).toCharArray(a,3);
  tft.drawChar(30, 44, byte(a[0]), ILI9341_BLACK, ILI9341_WHITE, 7);
  tft.drawChar(70, 44, byte(a[1]), ILI9341_BLACK, ILI9341_WHITE, 7);
  tft.drawChar(110, 44, 79, ILI9341_BLACK, ILI9341_WHITE, 2);
  tft.drawChar(130, 44, 70, ILI9341_BLACK, ILI9341_WHITE, 7);
}

/*void writeHoldTemp(){
  char b[2];
  String(temper2).toCharArray(b,3);
  tft.drawChar(148, 160, byte(b[0]), ILI9341_BLACK, ILI9341_WHITE, 5);
  tft.drawChar(178, 160, byte(b[1]), ILI9341_BLACK, ILI9341_WHITE, 5);
  tft.drawChar(208, 160, 79, ILI9341_BLACK, ILI9341_WHITE, 1);
  tft.drawChar(218, 160, 70, ILI9341_BLACK, ILI9341_WHITE, 5);
}*/
