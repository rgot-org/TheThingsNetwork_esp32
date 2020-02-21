//
//
//

#include "TTN_esp32.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "lmic/lmic/lmic.h"

static const char* const TAG = "ttn_prov";
static const char* const NVS_FLASH_PARTITION = "ttn";
static const char* const NVS_FLASH_KEY_DEV_EUI = "devEui";
static const char* const NVS_FLASH_KEY_APP_EUI = "appEui";
static const char* const NVS_FLASH_KEY_APP_KEY = "appKey";
static const char* const NVS_FLASH_KEY_APP_SESSION_KEY = "AppSKey";
static const char* const NVS_FLASH_KEY_NWK_SESSION_KEY = "NwkSKey";
static const char* const NVS_FLASH_KEY_DEV_ADDR = "devAddr";

static uint8_t global_dev_eui[8];
static uint8_t global_app_eui[8];
static uint8_t global_app_key[16];
static uint8_t global_appskey[16];
static uint8_t global_nwkskey[16];
static uint8_t global_deva[4];

// --- Helper class
class HandleCloser
{
public:
    explicit HandleCloser(nvs_handle handle) : h(handle) {}
    ~HandleCloser() { nvs_close(h); }

private:
    nvs_handle h;
};

// --- LMIC callbacks

// This EUI must be in little-endian format, so least-significant-byte first.
// When copying an EUI from ttnctl output, this means to reverse the bytes.
// For TTN issued EUIs the last bytes should be 0xD5, 0xB3, 0x70.
// The order is swapped in provisioning_decode_keys().
void os_getArtEui(u1_t* buf)
{
    memcpy(buf, global_app_eui, 8);
}

// This should also be in little endian format, see above.
void os_getDevEui(u1_t* buf)
{
    memcpy(buf, global_dev_eui, 8);
}

// This key should be in big endian format (or, since it is not really a number
// but a block of memory, endianness does not really apply). In practice, a key
// taken from ttnctl can be copied as-is.
void os_getDevKey(u1_t* buf)
{
    memcpy(buf, global_app_key, 16);
}

/************
 * Public
 ************/

// --- Constructor
TTN_esp32::TTN_esp32() : Lorawan_esp32()
{
    restoreKeys();
}

// --- Non-volatile storage
bool TTN_esp32::saveKeys()
{
    bool success = false;

    nvs_handle handle = 0;
    esp_err_t result = nvs_open(NVS_FLASH_PARTITION, NVS_READWRITE, &handle);
    HandleCloser closeMyHandle(handle);

    if (result == ESP_ERR_NVS_NOT_INITIALIZED)
    {
        ESP_LOGW(TAG, "NVS storage is not initialized. Call 'nvs_flash_init()' first.");
        ESP_ERROR_CHECK(result);
    }
    else
    {
        ESP_ERROR_CHECK(result);
        if (result == ESP_OK && writeNvsValue(handle, NVS_FLASH_KEY_DEV_EUI, global_dev_eui, sizeof(global_dev_eui))
            && writeNvsValue(handle, NVS_FLASH_KEY_APP_EUI, global_app_eui, sizeof(global_app_eui))
            && writeNvsValue(handle, NVS_FLASH_KEY_APP_KEY, global_app_key, sizeof(global_app_key))
            && writeNvsValue(handle, NVS_FLASH_KEY_DEV_ADDR, global_deva, sizeof(global_deva))
            && writeNvsValue(handle, NVS_FLASH_KEY_NWK_SESSION_KEY, global_nwkskey, sizeof(global_nwkskey))
            && writeNvsValue(handle, NVS_FLASH_KEY_APP_SESSION_KEY, global_appskey, sizeof(global_appskey)))
        {
            result = nvs_commit(handle);
            ESP_ERROR_CHECK(result);

            success = true;
            ESP_LOGI(TAG, "Dev and app EUI and app key saved in NVS storage");
        }
    }
    return success;
}

bool TTN_esp32::restoreKeys(bool silent)
{
    nvs_handle handle = 0;
    esp_err_t res = nvs_open(NVS_FLASH_PARTITION, NVS_READONLY, &handle);
    HandleCloser closeMyHandle(handle);

    if (res == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "NVS partition does not exist");
        have_keys = false;
    }
    else if (res == ESP_ERR_NVS_NOT_INITIALIZED)
    {
        ESP_LOGW(TAG, "NVS storage is not initialized. Call 'nvs_flash_init()' first.");
        have_keys = false;
    }
    else
    {
        ESP_ERROR_CHECK(res);

        uint8_t buf_dev_eui[8];
        uint8_t buf_app_eui[8];
        uint8_t buf_app_key[16];

        if (res != ESP_OK && readNvsValue(handle, NVS_FLASH_KEY_DEV_EUI, buf_dev_eui, sizeof(global_dev_eui), silent)
            && readNvsValue(handle, NVS_FLASH_KEY_APP_EUI, buf_app_eui, sizeof(global_app_eui), silent)
            && readNvsValue(handle, NVS_FLASH_KEY_APP_KEY, buf_app_key, sizeof(global_app_key), silent)
            && readNvsValue(handle, NVS_FLASH_KEY_DEV_ADDR, global_deva, sizeof(global_deva), silent)
            && readNvsValue(handle, NVS_FLASH_KEY_NWK_SESSION_KEY, global_nwkskey, sizeof(global_nwkskey), silent)
            && readNvsValue(handle, NVS_FLASH_KEY_APP_SESSION_KEY, global_appskey, sizeof(global_appskey), silent))
        {
            memcpy(global_dev_eui, buf_dev_eui, sizeof(global_dev_eui));
            memcpy(global_app_eui, buf_app_eui, sizeof(global_app_eui));
            memcpy(global_app_key, buf_app_key, sizeof(global_app_key));

            checkKeys();

            if (have_keys)
            {
                ESP_LOGI(TAG, "Dev and app EUI and app key have been restored from NVS storage");
            }
            else
            {
                ESP_LOGW(TAG, "Dev and app EUI and app key are invalid (zeroes only)");
            }
        }
        else
        {
            have_keys = false;
        }
    }

    return have_keys;
}

// --- Key handling
size_t TTN_esp32::getAppEui(char* buffer, size_t size)
{
    if (size < 8)
    {
        return 0;
    }
    uint8_t buf[8];
    for (size_t i = 0; i < 8; i++)
    {
        buf[i] = global_app_eui[i];
    }
    swapBytes(buf, 8);
    binToHexStr(buf, 8, buffer);
    buffer[16] = '\0';
    return 8;
}

String TTN_esp32::getAppEui()
{
    char hexbuf[17] = {0};
    for (size_t i = 0; i < 8; i++)
    {
        sprintf(hexbuf + (2 * i), "%02X", global_app_eui[7 - i]);
    }
    return String(hexbuf);
}

size_t TTN_esp32::getDevEui(char* buffer, size_t size, bool hardwareEUI)
{
    if (size < 8)
    {
        return 0;
    }
    uint8_t buf[8];
    if (hardwareEUI)
    {
        uint8_t mac[6];
        esp_err_t err = esp_efuse_mac_get_default(mac);
        ESP_ERROR_CHECK(err);
        buf[7] = mac[0];
        buf[6] = mac[1];
        buf[5] = mac[2];
        buf[4] = 0xff;
        buf[3] = 0xfe;
        buf[2] = mac[3];
        buf[1] = mac[4];
        buf[0] = mac[5];
    }
    else
    {
        for (size_t i = 0; i < 8; i++)
        {
            buf[i] = global_dev_eui[i];
        }
    }
    swapBytes(buf, 8);
    binToHexStr(buf, 8, buffer);
    buffer[16] = '\0';
    return 8;
}

String TTN_esp32::getDevEui(bool hardwareEUI)
{
    char hexbuf[17] = {0};
    if (hardwareEUI)
    {
        uint8_t mac[6];

        esp_err_t err = esp_efuse_mac_get_default(mac);
        ESP_ERROR_CHECK(err);
        binToHexStr(mac, 6, hexbuf);
        for (size_t i = 0; i < 6; i++)
        {
            hexbuf[15 - i] = hexbuf[11 - i];
        }
        hexbuf[9] = 'E';
        hexbuf[8] = 'F';
        hexbuf[7] = 'F';
        hexbuf[6] = 'F';
    }
    else
    {
        for (size_t i = 0; i < 8; i++)
        {
            sprintf(hexbuf + (2 * i), "%02X", global_dev_eui[7 - i]);
        }
    }
    return String(hexbuf);
}

bool TTN_esp32::provision(const char* appEui, const char* appKey)
{
    uint8_t mac[6];
    esp_err_t err = esp_efuse_mac_get_default(mac);
    ESP_ERROR_CHECK(err);
    global_dev_eui[7] = mac[0];
    global_dev_eui[6] = mac[1];
    global_dev_eui[5] = mac[2];
    global_dev_eui[4] = 0xff;
    global_dev_eui[3] = 0xfe;
    global_dev_eui[2] = mac[3];
    global_dev_eui[1] = mac[4];
    global_dev_eui[0] = mac[5];
#ifdef DEBUG
    Serial.print("dev EUI: ");
    for (size_t i = 0; i < 8; i++)
    {
        Serial.printf("%02X", global_dev_eui[7 - i]);
    }
    Serial.println();
#endif // DEBUG

    return decode(false, nullptr, appEui, appKey);
    // saveKeys();
}

bool TTN_esp32::provision(const char* devEui, const char* appEui, const char* appKey)
{
    return decode(true, devEui, appEui, appKey);
    // saveKeys();
}

bool TTN_esp32::provisionABP(const char* devAddr, const char* nwkSKey, const char* appSKey)
{
    hexStrToBin(nwkSKey, global_nwkskey, 16);
    hexStrToBin(appSKey, global_appskey, 16);
    hexStrToBin(devAddr, global_deva, 4);
    // saveKeys();
    return true;
}

bool TTN_esp32::join()
{
    bool success = have_keys;
    if (!success)
    {
        success = restoreKeys(false);
    }
    if (success)
    {
        success = Lorawan_esp32::join();
    }
    else
    {
        ESP_LOGW(TAG, "Device EUI, App EUI and/or App key have not been provided");
        Serial.println("No keys provided");
    }
    return success;
}

bool TTN_esp32::join(const char* appEui, const char* appKey, int8_t retries, uint32_t retryDelay)
{
    // if(!provision(appEui, appKey)) return false;
    provision(appEui, appKey);
    return Lorawan_esp32::join();
}

bool TTN_esp32::join(const char* devEui, const char* appEui, const char* appKey, int8_t retries, uint32_t retryDelay)
{
    provision(devEui, appEui, appKey);
    return Lorawan_esp32::join();
}

bool TTN_esp32::personalize()
{
    bool success;
    // restoreKeys(false);
    if (!isAllZeros(global_deva, sizeof(global_deva)) && !isAllZeros(global_nwkskey, sizeof(global_nwkskey))
        && !isAllZeros(global_appskey, sizeof(global_appskey)))
    {
        devaddr_t dev_addr = global_deva[0] << 24 | global_deva[1] << 16 | global_deva[2] << 8 | global_deva[3];
        Lorawan_esp32::personalize(0x13, dev_addr, global_nwkskey, global_appskey);
        success = true;
    }
    else
    {
        success = false;
    }
    return success;
}

bool TTN_esp32::personalize(const char* devAddr, const char* nwkSKey, const char* appSKey)
{
    hexStrToBin(nwkSKey, global_nwkskey, 16);
    hexStrToBin(appSKey, global_appskey, 16);
    hexStrToBin(devAddr, global_deva, 4);
    devaddr_t dev_addr = global_deva[0] << 24 | global_deva[1] << 16 | global_deva[2] << 8 | global_deva[3];
#ifdef DEBUG
    Serial.printf("str_deva: %s\n", devAddr);
    Serial.printf("int_devaddr: %X\n", dev_addr);
#endif // DEBUG
    Lorawan_esp32::personalize(0x13, dev_addr, global_nwkskey, global_appskey);
    return true;
}

void TTN_esp32::showStatus()
{
    if (poll())
    {
        char buffer[64];
        Serial.println("---------------Status--------------");

        getDevEui(buffer, sizeof(buffer));
        Serial.printf("Device EUI: %s\n", buffer);
        getAppEui(buffer, sizeof(buffer));
        Serial.printf("Application EUI: %s\n", buffer);
        LMIC_getSessionKeys(&LMIC.netid, &LMIC.devaddr, LMIC.nwkKey, LMIC.artKey);
        Serial.print("netid: ");
        Serial.println(LMIC.netid, HEX);
        Serial.print("devaddr: ");
        Serial.println(LMIC.devaddr, HEX);
        Serial.print("NwkSKey: ");
        for (size_t i = 0; i < sizeof(LMIC.nwkKey); ++i)
        {
            Serial.print(LMIC.nwkKey[i], HEX);
        }
        Serial.println("");
        Serial.print("AppSKey: ");
        for (size_t i = 0; i < sizeof(LMIC.artKey); ++i)
        {
            Serial.print(LMIC.artKey[i], HEX);
        }
        Serial.println("");

        Serial.printf("data rate: %d\n", LMIC.datarate);
        Serial.printf("tx power: %ddB\n", LMIC.txpow);
        // Serial.printf("rssi: %ddB\n", LMIC.rssi -157);
        // Serial.printf("snr: %ddB\n", LMIC.snr / 4);
        Serial.printf("freq: %dHz\n", LMIC.freq);
        Serial.println("-----------------------------------");
    }
}

/************
 * Private
 ************/

bool TTN_esp32::decode(bool incl_dev_eui, const char* dev_eui, const char* app_eui, const char* app_key)
{
    uint8_t buf_dev_eui[8];
    uint8_t buf_app_eui[8];
    uint8_t buf_app_key[16];

    if (incl_dev_eui && (strlen(dev_eui) != 16 || !hexStrToBin(dev_eui, buf_dev_eui, 8)))
    {
        ESP_LOGW(TAG, "Invalid device EUI: %s", dev_eui);
        return false;
    }

    if (incl_dev_eui)
    {
        swapBytes(buf_dev_eui, 8);
    }

    if (strlen(app_eui) != 16 || !hexStrToBin(app_eui, buf_app_eui, 8))
    {
        ESP_LOGW(TAG, "Invalid application EUI: %s", app_eui);
        return false;
    }

    swapBytes(buf_app_eui, 8);

    if (strlen(app_key) != 32 || !hexStrToBin(app_key, buf_app_key, 16))
    {
        ESP_LOGW(TAG, "Invalid application key: %s", app_key);
        return false;
    }

    if (incl_dev_eui)
    {
        memcpy(global_dev_eui, buf_dev_eui, sizeof(global_dev_eui));
    }
    memcpy(global_app_eui, buf_app_eui, sizeof(global_app_eui));
    memcpy(global_app_key, buf_app_key, sizeof(global_app_key));

    checkKeys();

    return true;
}

// bool TTN_esp32::decodeKeys(const char *dev_eui, const char *app_eui, const
// char *app_key)
//{
//	return decode(true, dev_eui, app_eui, app_key);
//}

// bool TTN_esp32::fromMAC(const char *app_eui, const char *app_key)
//{
//
//}

// bool TTN_esp32::sendBytes(uint8_t *payload, size_t length, uint8_t port ,
// uint8_t confirm ) { 	Lorawan_esp32::sendBytes(payload, length, port,
// confirm);
//}

// void TTN_esp32::onMessage(void(*cb)(const uint8_t *payload, size_t size,
// uint8_t port))
//{
//	Lorawan_esp32::onMessage(cb);
//}

bool TTN_esp32::readNvsValue(nvs_handle handle, const char* key, uint8_t* data, size_t expected_length, bool silent)
{
    size_t size = expected_length;
    esp_err_t res = nvs_get_blob(handle, key, data, &size);
    if (res == ESP_OK && size == expected_length)
    {
        return true;
    }

    if (res == ESP_OK && size != expected_length)
    {
        if (!silent)
        {
            ESP_LOGW(TAG, "Invalid size of NVS data for %s", key);
        }
        return false;
    }

    if (res == ESP_ERR_NVS_NOT_FOUND)
    {
        if (!silent)
        {
            ESP_LOGW(TAG, "No NVS data found for %s", key);
        }
        return false;
    }

    ESP_ERROR_CHECK(res);
    return false;
}

bool TTN_esp32::writeNvsValue(nvs_handle handle, const char* key, const uint8_t* data, size_t len)
{
    uint8_t buf[16];
    if (readNvsValue(handle, key, buf, len, true) && memcmp(buf, data, len) == 0)
    {
        return true; // unchanged
    }

    esp_err_t res = nvs_set_blob(handle, key, data, len);
    ESP_ERROR_CHECK(res);

    return res == ESP_OK;
}

// --- Helper functions ---

void TTN_esp32::checkKeys()
{
    have_keys = !isAllZeros(global_dev_eui, sizeof(global_dev_eui))
        && !isAllZeros(global_app_eui, sizeof(global_app_eui)) && !isAllZeros(global_app_key, sizeof(global_app_key));
}

bool TTN_esp32::hexStrToBin(const char* hex, uint8_t* buf, int len)
{
    const char* ptr = hex;
    for (int i = 0; i < len; i++)
    {
        int val = hexTupleToByte(ptr);
        if (val < 0)
        {
            return false;
        }
        buf[i] = val;
        ptr += 2;
    }
    return true;
}

int TTN_esp32::hexTupleToByte(const char* hex)
{
    int nibble1 = hexDigitToVal(hex[0]);
    if (nibble1 < 0)
    {
        return -1;
    }
    int nibble2 = hexDigitToVal(hex[1]);
    if (nibble2 < 0)
    {
        return -1;
    }
    return (nibble1 << 4) | nibble2;
}

int TTN_esp32::hexDigitToVal(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    if (ch >= 'A' && ch <= 'F')
    {
        return ch + 10 - 'A';
    }
    if (ch >= 'a' && ch <= 'f')
    {
        return ch + 10 - 'a';
    }
    return -1;
}

void TTN_esp32::binToHexStr(const uint8_t* buf, int len, char* hex)
{
    for (int i = 0; i < len; i++)
    {
        uint8_t b = buf[i];
        *hex = valToHexDigit((b & 0xf0) >> 4);
        hex++;
        *hex = valToHexDigit(b & 0x0f);
        hex++;
    }
}

char TTN_esp32::valToHexDigit(int val)
{
    return "0123456789ABCDEF"[val];
}

void TTN_esp32::swapBytes(uint8_t* buf, int len)
{
    uint8_t* p1 = buf;
    uint8_t* p2 = buf + len - 1;
    while (p1 < p2)
    {
        uint8_t t = *p1;
        *p1 = *p2;
        *p2 = t;
        p1++;
        p2--;
    }
}

bool TTN_esp32::isAllZeros(const uint8_t* buf, int len)
{
    for (int i = 0; i < len; i++)
    {
        if (buf[i] != 0)
        {
            return false;
        }
    }
    return true;
}
