#include "DHTesp.h"
#include "pthread.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"



DHTesp dht;
pthread_t threads[5];
int motorPin = 22;
bool listen = true;

bool stopTemp = false;
bool stopHumid = false;
bool stopMotor = false;

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
    float temperature = dht.getTemperature();
    Serial.print("[");
    Serial.print((int)i);
    Serial.print("] ");
    
    Serial.print("Temperature: ");
    Serial.println(temperature);
    delay(10000);
  }
  stopTemp = true;
}

void *readHumid(void *i) {
  while(listen) {
    float humid = dht.getHumidity();
    Serial.print("[");
    Serial.print((int)i);
    Serial.print("] ");
    
    Serial.print("Humidity ");
    Serial.println(humid);
    delay(20000);
  }
  stopHumid = true;
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

void startThreads() {
  startThreadTemp();
  startThreadHumid();
  startThreadMotor();
}

void setup()
{
  Serial.begin(115200);
  pinMode(motorPin, OUTPUT);
  dht.setup(14, DHTesp::DHT22);

  startThreads();
  initBLE();
 
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
