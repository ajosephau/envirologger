/***************************************************************************
  This is an environment datalogger 
  BSD license
 ***************************************************************************/

// https://learn.adafruit.com/adafruit-oled-featherwing/usage
// https://learn.adafruit.com/adafruit-feather-m0-adalogger/overview
// https://learn.adafruit.com/adafruit-bme680-humidity-temperature-barometic-pressure-voc-gas
// https://wiki.dfrobot.com/Carbon_Monoxide_Gas_Sensor_MQ7___SKU_SEN0132_
// https://learn.adafruit.com/adafruit-feather-m0-adalogger/using-the-sd-card

#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define BUTTON_A  9
#define BUTTON_B  6
#define BUTTON_C  5

// change this to match your SD shield or module;
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8
// MKRZero SD: SDCARD_SS_PIN
#define cardSelect 4

#define CO_SENSOR  14

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme; // I2C
//Adafruit_BME680 bme(BME_CS); // hardware SPI
//Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO,  BME_SCK);

Adafruit_SSD1306 display = Adafruit_SSD1306();

// set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;

int mode = 0;

File logfile;

void setup() {
  Serial.begin(115200);
  
  //  while (!Serial) {}


  Serial.print("\nInitializing SD card...");

  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  if (!card.init(SPI_HALF_SPEED, cardSelect)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card inserted?");
    Serial.println("* is your wiring correct?");
    Serial.println("* did you change the chipSelect pin to match your shield or module?");
    while (1);
  } else {
    Serial.println("Wiring is correct and a card is present.");
  }

  // print the type of card
  Serial.println();
  Serial.print("Card type:         ");
  switch (card.type()) {
    case SD_CARD_TYPE_SD1:
      Serial.println("SD1");
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println("SD2");
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Unknown");
  }

  // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
  if (!volume.init(card)) {
    Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
    while (1);
  }

  Serial.print("Clusters:          ");
  Serial.println(volume.clusterCount());
  Serial.print("Blocks x Cluster:  ");
  Serial.println(volume.blocksPerCluster());

  Serial.print("Total Blocks:      ");
  Serial.println(volume.blocksPerCluster() * volume.clusterCount());
  Serial.println();

  // print the type and size of the first FAT-type volume
  uint32_t volumesize;
  Serial.print("Volume type is:    FAT");
  Serial.println(volume.fatType(), DEC);

  volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= volume.clusterCount();       // we'll have a lot of clusters
  volumesize /= 2;                           // SD card blocks are always 512 bytes (2 blocks are 1KB)
  Serial.print("Volume size (Kb):  ");
  Serial.println(volumesize);
  Serial.print("Volume size (Mb):  ");
  volumesize /= 1024;
  Serial.println(volumesize);
  Serial.print("Volume size (Gb):  ");
  Serial.println((float)volumesize / 1024.0);

  Serial.println("\nFiles found on the card (name, date and size in bytes): ");
  root.openRoot(volume);

  // list all files in the card with date and size
  root.ls(LS_R | LS_DATE | LS_SIZE);


  Serial.println(F("Datakigger"));

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  // init done
  display.display();
  delay(100);
  display.clearDisplay();
  display.display();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  // see if the card is present and can be initialized:
  if (!SD.begin(cardSelect)) {
    Serial.println("Card init. failed!");
  }
  char filename[15];
  strcpy(filename, "/ANALOG00.TXT");
  for (uint8_t i = 0; i < 100; i++) {
    filename[7] = '0' + i / 10;
    filename[8] = '0' + i % 10;
    // create if does not exist, do not open existing, write, sync after write
    if (! SD.exists(filename)) {
      break;
    }
  }

  logfile = SD.open(filename, FILE_WRITE);
  if ( ! logfile ) {
    Serial.print("Couldnt create ");
    Serial.println(filename);
  }
  Serial.print("Writing to ");
  Serial.println(filename);

  if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
  }

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

  logfile.println("Temp, pressure, gas, humidity, CO");
}

void loop() {
  if (!digitalRead(BUTTON_A)) mode = 0;
  if (!digitalRead(BUTTON_B)) mode = 1;
  if (!digitalRead(BUTTON_C)) mode = 0;

  display.setCursor(0, 0);
  display.clearDisplay();

  if (! bme.performReading()) {
    Serial.println("Failed to perform reading :(");
    return;
  }
  int co_value;
  co_value = analogRead(CO_SENSOR); //Read Gas value from analog 0


  Serial.print("Temperature = "); Serial.print(bme.temperature); Serial.println(" *C");
  display.print("Temperature: "); display.print(bme.temperature); display.println(" *C");
  logfile.print(bme.temperature); logfile.print(",");

  Serial.print("Pressure = "); Serial.print(bme.pressure / 100.0); Serial.println(" hPa");
  display.print("Pressure: "); display.print(bme.pressure / 100.0); display.println(" hPa");
  logfile.print(bme.pressure / 100.0); logfile.print(",");

  Serial.print("Gas = "); Serial.print(bme.gas_resistance / 1000.0); Serial.println(" KOhms");
  display.print("Gas: "); display.print(bme.gas_resistance / 1000.0); display.println(" KOhms");
  logfile.print(bme.gas_resistance / 1000.0); logfile.print(",");

  Serial.print("Humidity = "); Serial.print(bme.humidity); Serial.println(" %");
  logfile.print(bme.humidity); logfile.print(",");

  Serial.print("CO = "); Serial.println(co_value);
  logfile.print(co_value); logfile.println("");

  if ( mode == 0) {
    display.print("CO: "); display.println(co_value);
  }
  else {
    display.print("Humidity: "); display.print(bme.humidity); display.println(" %");
  }

  display.display();
  logfile.flush();


  delay(500);

}
