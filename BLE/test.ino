#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "abcdefab-1234-1234-1234-abcdefabcdef"

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void setup() {
  Serial.begin(115200);

  BLEDevice::init("ESP32_BLE");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Waiting for a client connection to notify...");

  // 设置BLE数据包长度扩展
  BLEDevice::setMTU(517); // 设置最大传输单元（MTU）

  // 设置连接参数
  pServer->getAdvertising()->setMinInterval(0x20); // 最小连接间隔 20 * 1.25ms = 25ms
  pServer->getAdvertising()->setMaxInterval(0x40); // 最大连接间隔 40 * 1.25ms = 50ms
  pServer->getAdvertising()->setTimeout(0x60);     // 连接超时 60 * 10ms = 600ms
}

void loop() {
  if (deviceConnected) {
    // Example data to send
    String data = "This is a long message that exceeds 20 bytes.";
    int dataLength = data.length();
    int offset = 0;

    // Send data in chunks of 20 bytes
    while (offset < dataLength) {
      int chunkSize = min(20, dataLength - offset);
      String chunk = data.substring(offset, offset + chunkSize);
      pCharacteristic->setValue((uint8_t*)chunk.c_str(), chunkSize);
      pCharacteristic->notify();
      offset += chunkSize;
      delay(50); // Reduce delay to ensure faster transmission
    }
    delay(500); // Send data every half second
  }
}
