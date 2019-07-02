#include "DHTesp.h"
#include "pthread.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <string.h>
#include <GxEPD.h>
#include <GxGDEH029A1/GxGDEH029A1.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include "image.h"


#define SCREEN_WIDTH 296 // epaper display width, in pixels
#define SCREEN_HEIGHT 128 // epaper display height, in pixels

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"


DHTesp dht;
pthread_t threads[5];
int motorPin = 32;
bool listen = true;

bool stopTemp = false;
bool stopHumid = false;
bool stopMotor = false;

float temperature = 0;
float humid = 0;

GxIO_Class io(SPI, SS, 22, 21);
GxEPD_Class display(io, 16, 4);

// todo
/**
 * 1 thread untuk update data dari DHT untuk update value dari temperature & humidity:
 * thread ini juga berurusan dengan server update untuk mengirim data ke server mengenai status suhu dan kelembapan dalam cycle waktu tertentu.
 *
 * 1 thread untuk trigger Servo untuk Humidity
 * 1 thread untuk trigger servo untuk pemanas
 * 
 * 1 thread untuk berurusan dengan pemutar balik telur. thread ini sekalian mendata untuk kapan terakhir dibalik telur
 * 
 * 1 thread untuk mengupdate display.
 * 1 thread bluetooth untuk listen bluetooth.
 * bluetooth untuk retrieve dan store configs.
 * config yg d simpan:
 * - jenis telur (nama)
 * - lama penetasan
 * - konfigurasi kelembapan: array ex: [{day: 0, val: 80}, {day: 25, val: 90}]
 * - konfigurasi suhu (konstant): ex: 38
 * - konfigurasi pembalikan: { times: 5}
 * - wifi config: {ssid: "SSID WIFI", password: "123456"}
 * - firebase database: {id: "id_firebase_project", "path"}
 * - update cycle: { temp: sec, humid: sec, database: sec, }
 */


class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      if (value == "true") {
        listen = true;
      } else if (value == "stop") {
        listen = false;
      }

      if (value.length() > 0) {
        Serial.println("*********");
        Serial.print("New value: ");
        for (int i = 0; i < value.length(); i++)
          Serial.print(value[i]);

        Serial.println();
        Serial.println("*********");
      }
    }
};


void *readTemp(void *i) {
  while(listen) {
    float t = dht.getTemperature();
    if (!isnan(t)) temperature = t;
    delay(1000);
  }
  stopTemp = true;
}

void *readHumid(void *i) {
  while(listen) {
    float h = dht.getHumidity();
    if(!isnan(h)) humid = h;
    delay(1000);
  }
  stopHumid = true;
}


void *renderOled(void *i) {
  int ct = 0;
  while(listen) {
    String temperatureString = String(temperature,1);
    String humidString = String(humid,1);
    const char* name = "FreeSansBold24pt7b";
    const GFXfont* f = &FreeSansBold24pt7b;
  
    uint16_t box_x = 5;
    uint16_t box_y = 5;
    uint16_t box_w = SCREEN_WIDTH-10;
    uint16_t box_h = SCREEN_HEIGHT-10;
    uint16_t cursor_y = box_y + 16;

    display.setRotation(45);
    display.setFont(f);
    display.setTextColor(GxEPD_BLACK);
    display.fillScreen(GxEPD_WHITE);
    display.drawExampleBitmap(bgImg, 5, 5, box_w, box_h, GxEPD_BLACK);
    if (ct ==0) {
      display.drawExampleBitmap(telur1, box_w-50, 5, 60, 60, GxEPD_BLACK);
      ct++;
    } else if(ct == 1) {
      display.drawExampleBitmap(telur2, box_w-50, 5, 60, 60, GxEPD_BLACK);
      ct++;
    } else {
      display.drawExampleBitmap(telur3, box_w-50, 5, 60, 60, GxEPD_BLACK);
      ct = 0;
    }
    display.setCursor(box_x+40, box_h -13);
    display.print(temperatureString); 
    display.setCursor(box_x+145, box_h -13);
    display.print(humidString); 
    display.updateWindow(box_x, box_y, box_w, box_h, true);
//    display.update();
    delay(700);
  }
}
bool motorOn = false;

void *triggerMotor(void *i) {
  while(listen) {
    Serial.print("[");
    Serial.print((int)i);
    Serial.print("] ");
    
    if(motorOn) {
      Serial.println("MOTOR ON");
      digitalWrite(motorPin, HIGH);
    } else {
      Serial.println("MOTOR OFF");
      digitalWrite(motorPin, LOW);
    }
    motorOn = !motorOn;
    
    delay(20000);
  }
  stopMotor = true;
}

void initBLE() {
  BLEDevice::init("MyESP32");
  BLEServer *pServer = BLEDevice::createServer();

  BLEService *pService = pServer->createService(SERVICE_UUID);

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());

  pCharacteristic->setValue("Hello World");
  pService->start();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
}

void startThreadTemp() {
  int i = 0;
  pthread_create(&threads[i], NULL, readTemp, (void *)i);
}

void startThreadHumid() {
  int i =1;
  pthread_create(&threads[i], NULL, readHumid, (void *)i);
}

void startThreadMotor() {
  int i = 2;
  pthread_create(&threads[i], NULL, triggerMotor, (void *)i);
}

void startThreadOled() {
  int i = 3;
  pthread_create(&threads[i], NULL, renderOled, (void *)i);
}

void startThreads() {
  startThreadTemp();
  startThreadHumid();
  startThreadMotor();
  startThreadOled();
}

void setup()
{
  Serial.begin(115200);
  pinMode(motorPin, OUTPUT);
  dht.setup(14, DHTesp::DHT22);
  initBLE();

  display.init();
  display.fillScreen(GxEPD_WHITE);
  display.setRotation(45);
  display.drawExampleBitmap(linov, 5, 5, SCREEN_WIDTH-10, SCREEN_HEIGHT-10, GxEPD_BLACK);
  display.update();
  delay(3000);

  startThreads();
}

void loop()
{
  
  if(stopTemp && listen) {
    stopTemp = false;
    startThreadTemp();
  }
  if(stopHumid && listen) {
    stopHumid = false;
    startThreadHumid();
  }

  if(stopMotor && listen) {
    stopMotor = false;
    startThreadMotor();
  }

  delay(2000);
 
}
