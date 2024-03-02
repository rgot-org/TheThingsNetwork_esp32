#include "TTN_BLE_esp32.h"
#include "ByteArrayUtils.h"
#include "helper.h"
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>


/********************************************
* BLE callback when client connect/disconnect
* reset the esp32 when disconnecting
********************************************/
class MyServerCallbacks : public BLEServerCallbacks, TTN_BLE_esp32
{
    void onConnect(BLEServer* pServer) { 
        ESP_LOGI(TAG,"BLE client connected");
    }

    void onDisconnect(BLEServer* pServer) { 
         ESP_LOGI(TAG, "BLE client disconnected");
      rebootESP32();
     }
};
/********************************
 * BLE callback when BLE client (the phone) sends data to characteristic
 *********************************/
class MyCallbacks : public BLECharacteristicCallbacks, TTN_BLE_esp32
{
    void onWrite(BLECharacteristic* pCharacteristic)
    {
        BLEUUID myUUID = pCharacteristic->getUUID();
        TTN_esp32* ttn = TTN_esp32::getInstance();
        ttn->restoreKeys();

        /*OTAA*/
        if (myUUID.equals(BLEUUID::fromString(CHARACTERISTIC_DEVEUI)))
        {
            ESP_LOGI(TAG,"DevEUI");
            byte* value = pCharacteristic->getData();
            ByteArrayUtils::swapBytes(value, 8);
            ttn->setDevEui(value);
        }
        if (myUUID.equals(BLEUUID::fromString(CHARACTERISTIC_APPEUI)))
        {
            ESP_LOGI(TAG,"AppEUI");
            byte* value = pCharacteristic->getData();
            ByteArrayUtils::swapBytes(value, 8);
            ttn->setAppEui(value);
        }
        if (myUUID.equals(BLEUUID::fromString(CHARACTERISTIC_APPKEY)))
        {
            ESP_LOGI(TAG,"AppKey");
            byte* value = pCharacteristic->getData();
            ttn->setAppKey(value);
        }
        /*ABP*/
        if (myUUID.equals(BLEUUID::fromString(CHARACTERISTIC_DEV_ADDR)))
        {
            ESP_LOGI(TAG,"devADDR");
        }
        if (myUUID.equals(BLEUUID::fromString(CHARACTERISTIC_NWKSKEY)))
        {
            ESP_LOGI(TAG,"NwkSKey");
        }
        if (myUUID.equals(BLEUUID::fromString(CHARACTERISTIC_APP_SKEY)))
        {
            ESP_LOGI(TAG,"AppSKey");
        }

        ttn->saveKeys();
    }
};

bool TTN_BLE_esp32::begin(std::string bt_name)
{
    TTN_esp32* ttn = TTN_esp32::getInstance();
    ttn->restoreKeys();
    if (bt_name == "")
    {
        std::string nameDev(ttn->getDevEui(true).c_str());
        bt_name.append("RGOT_").append(nameDev);
    }
    BLEDevice::init(bt_name);
    ESP_LOGI(TAG, "BLE Begin server: %s", bt_name.c_str());
    BLEServer* pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService* pService = pServer->createService(SERVICE_UUID);
    BLECharacteristic* pCharacteristicAppKey = pService->createCharacteristic(
        CHARACTERISTIC_APPKEY, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    BLECharacteristic* pCharacteristicDevEUI = pService->createCharacteristic(
        CHARACTERISTIC_DEVEUI, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    BLECharacteristic* pCharacteristicAppEUI = pService->createCharacteristic(
        CHARACTERISTIC_APPEUI, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    BLECharacteristic* pCharacteristicDevAddr = pService->createCharacteristic(
        CHARACTERISTIC_DEV_ADDR, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    BLECharacteristic* pCharacteristicNwkSKey = pService->createCharacteristic(
        CHARACTERISTIC_NWKSKEY, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    BLECharacteristic* pCharacteristicAppSKey = pService->createCharacteristic(
        CHARACTERISTIC_APP_SKEY, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

    pCharacteristicDevEUI->setCallbacks(new MyCallbacks());
    pCharacteristicAppEUI->setCallbacks(new MyCallbacks());
    pCharacteristicAppKey->setCallbacks(new MyCallbacks());
    pCharacteristicDevAddr->setCallbacks(new MyCallbacks());
    pCharacteristicNwkSKey->setCallbacks(new MyCallbacks());
    pCharacteristicAppSKey->setCallbacks(new MyCallbacks());
    byte buf[33];

    int len = ttn->getDevEui(buf);
    pCharacteristicDevEUI->setValue(buf, len);

    len = ttn->getAppEui(buf);
    pCharacteristicAppEUI->setValue(buf, len);

    len = ttn->getAppKey(buf);
    for (size_t i = 3; i < len-4; i++)
    {
        buf[i] = 0;
    }
   pCharacteristicAppKey->setValue(buf, len);

    pService->start();
    BLEAdvertising* pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pServer->startAdvertising();
    pAdvertising->start();
    return true;
}

bool TTN_BLE_esp32::stop()
{
    ESP_LOGI(TAG, "stop BLE");
    BLEDevice::deinit();
    return true;
}

void TTN_BLE_esp32::rebootESP32() {
    ESP_LOGI(TAG, "reboot esp32");
    esp_task_wdt_init(1, true); // program task with watchdog to reset the esp32
    esp_task_wdt_add(NULL);
    while (true) ;
}

bool TTN_BLE_esp32::getInitialized()
{
    return BLEDevice::getInitialized();
}
TTN_BLE_esp32::TTN_BLE_esp32() {}
void TTN_BLE_esp32::init() {}
