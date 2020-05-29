//
//
//

#include "TTN_esp32.h"

#include "ByteArrayUtils.h"
#include "NVSHandler.h"
#include "esp_log.h"
#include "esp_system.h"
#include "helper.h"

#include "lmic/hal/hal.h"
#include "lmic/lmic/lmic.h"

#define LOOP_TTN_MS 1

/// The last sent sequence number we know, stored in RTC memory
RTC_DATA_ATTR uint32_t sequenceNumberUp = 0;
RTC_DATA_ATTR uint8_t app_session_key[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
RTC_DATA_ATTR uint8_t net_session_key[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
RTC_DATA_ATTR uint8_t dev_adr[4] = {0, 0, 0, 0};

// static osjobcb_t sendMsg;
TTN_esp32* TTN_esp32::_instance = 0;

// --- LMIC callbacks

// This EUI must be in little-endian format, so least-significant-byte first.
// When copying an EUI from ttnctl output, this means to reverse the bytes.
// For TTN issued EUIs the last bytes should be 0xD5, 0xB3, 0x70.
// The order is swapped in provisioning_decode_keys().
void os_getArtEui(u1_t* buf)
{
    TTN_esp32* ttn = TTN_esp32::getInstance();
    std::copy(ttn->app_eui, ttn->app_eui + 8, buf);
}

// This should also be in little endian format, see above.
void os_getDevEui(u1_t* buf)
{
    TTN_esp32* ttn = TTN_esp32::getInstance();
    std::copy(ttn->dev_eui, ttn->dev_eui + 8, buf);
}

// This key should be in big endian format (or, since it is not really a number
// but a block of memory, endianness does not really apply). In practice, a key
// taken from ttnctl can be copied as-is.
void os_getDevKey(u1_t* buf)
{
    TTN_esp32* ttn = TTN_esp32::getInstance();
    std::copy(ttn->app_key, ttn->app_key + 16, buf);
}

/************
 * Public
 ************/

TTN_esp32* TTN_esp32::getInstance()
{
    return _instance;
}

// --- Constructor
TTN_esp32::TTN_esp32() :
	  dev_eui {0, 0, 0, 0, 0, 0, 0, 0},
      app_eui {0, 0, 0, 0, 0, 0, 0, 0},
      app_key {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      joined {false},
      provisioned {false},
      session {false},
      TTN_task_Handle {NULL},
      _message {0},
      _length {1},
      _port {1},
      _confirm {0},
      txInterval {0},
      cyclique {false}
{
    // restoreKeys();
    _instance = this;
}

bool TTN_esp32::begin()
{
    // record self in a static so that we can dispatch events
    // ASSERT(TTN_esp32::pLoRaWAN == this ||TTN_esp32::pLoRaWAN == NULL);
    // pLoRaWAN = this;
    return begin(TTN_esp32_LMIC::GetPinmap_ThisBoard());
}

bool TTN_esp32::begin(uint8_t nss, uint8_t rxtx, uint8_t rst, uint8_t dio0, uint8_t dio1, uint8_t dio2)
{
    static const TTN_esp32_LMIC::HalPinmap_t myPinmap = {
        .nss = nss,
        .rxtx = rxtx,
        .rst = rst,
        .dio = {dio0, dio1, dio2},
    };
    return begin(&myPinmap);
}

bool TTN_esp32::begin(const TTN_esp32_LMIC::HalPinmap_t* pPinmap)
{
    const bool success = os_init_ex(pPinmap);
    if (success)
    {
        // Reset the MAC state. Session and pending data transfers will be discarded.
        LMIC_reset();
    }
    return success;
}

bool TTN_esp32::provision(const char* appEui, const char* appKey)
{
    uint8_t mac[6];
    esp_err_t err = esp_efuse_mac_get_default(mac);
    ESP_ERROR_CHECK(err);
    dev_eui[7] = mac[0];
    dev_eui[6] = mac[1];
    dev_eui[5] = mac[2];
    dev_eui[4] = 0xff;
    dev_eui[3] = 0xfe;
    dev_eui[2] = mac[3];
    dev_eui[1] = mac[4];
    dev_eui[0] = mac[5];
#ifdef DEBUG
    Serial.print("dev EUI: ");
    for (size_t i = 0; i < 8; i++)
    {
        Serial.printf("%02X", dev_eui[7 - i]);
    }
    Serial.println();
#endif // DEBUG

    if (decode(false, nullptr, appEui, appKey))
    {
        return saveKeys();
    }
    return false;
}

bool TTN_esp32::provision(const char* devEui, const char* appEui, const char* appKey)
{
    if (decode(true, devEui, appEui, appKey))
    {
        return saveKeys();
    }
    return false;
}

bool TTN_esp32::provisionABP(const char* devAddr, const char* nwkSKey, const char* appSKey)
{
    ByteArrayUtils::hexStrToBin(nwkSKey, net_session_key, 16);
    ByteArrayUtils::hexStrToBin(appSKey, app_session_key, 16);
    ByteArrayUtils::hexStrToBin(devAddr, dev_adr, 4);
    return saveKeys();
}

bool TTN_esp32::join()
{
    bool success = false;

    if (!provisioned && !session)
    {
        restoreKeys();
    }

    // Check if this is a cold boot
    if (session && sequenceNumberUp != 0)
    {
        Serial.println(F("Using stored session to join"));
        devaddr_t dev_addr = dev_adr[0] << 24 | dev_adr[1] << 16 | dev_adr[2] << 8 | dev_adr[3];
        personalize(0x13, dev_addr, net_session_key, app_session_key);
        success = true;
    }
    else if (provisioned)
    {
        Serial.println(F("Using stored keys to join"));
        LMIC_setClockError(MAX_CLOCK_ERROR * 7 / 100);
        LMIC_unjoin();
        LMIC_startJoining();
        xTaskCreatePinnedToCore(loopStack, "ttnTask", 2048, (void*)1, (5 | portPRIVILEGE_BIT), &TTN_task_Handle, 1);
        success = true;
    }
    else
    {
        ESP_LOGW(TAG, "Device EUI, App EUI and/or App key have not been provided");
        Serial.println(F("Cannot join. No keys provided"));
    }
    return success;
}

bool TTN_esp32::join(const char* appEui, const char* appKey, bool force, int8_t retries, uint32_t retryDelay)
{
    // if(!provision(appEui, appKey)) return false;
    restoreKeys();
    force = force || (std::equal(app_eui, app_eui + 8, appEui) && std::equal(app_key, app_key + 16, appKey));
    if (force || !provisioned)
    {
        provision(appEui, appKey);
    }
    return join();
}

bool TTN_esp32::join( const char* devEui, const char* appEui, const char* appKey, bool force, int8_t retries, uint32_t retryDelay)
{
    restoreKeys();
    force = force
        || (std::equal(dev_eui, dev_eui + 8, devEui) && std::equal(app_eui, app_eui + 8, appEui)
            && std::equal(app_key, app_key + 16, appKey));
    if (force || !provisioned)
    {
        provision(devEui, appEui, appKey);
    }
    return join();
}

bool TTN_esp32::personalize()
{
    bool success;
    // restoreKeys(false);
    if (!ByteArrayUtils::isAllZeros(dev_adr, sizeof(dev_adr))
        && !ByteArrayUtils::isAllZeros(net_session_key, sizeof(net_session_key))
        && !ByteArrayUtils::isAllZeros(app_session_key, sizeof(app_session_key)))
    {
        devaddr_t dev_addr = dev_adr[0] << 24 | dev_adr[1] << 16 | dev_adr[2] << 8 | dev_adr[3];
        personalize(0x13, dev_addr, net_session_key, app_session_key);
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
    ByteArrayUtils::hexStrToBin(nwkSKey, net_session_key, 16);
    ByteArrayUtils::hexStrToBin(appSKey, app_session_key, 16);
    ByteArrayUtils::hexStrToBin(devAddr, dev_adr, 4);
    devaddr_t dev_addr = dev_adr[0] << 24 | dev_adr[1] << 16 | dev_adr[2] << 8 | dev_adr[3];
#ifdef DEBUG
    ESP-LOGI(TAG,"Dev adr str: %s", devAddr);
    ESP_LOGI(TAG,"Dev adr int: %X", dev_addr);
#endif // DEBUG
    personalize(0x13, dev_addr, net_session_key, app_session_key);
    return true;
}

bool TTN_esp32::sendBytes(uint8_t* payload, size_t length, uint8_t port, uint8_t confirm)
{
    cyclique = false;
    return txBytes(payload, length, port, confirm);
}

void TTN_esp32::sendBytesAtInterval(uint8_t* payload, size_t length, uint32_t interval, uint8_t port, uint8_t confirm)
{
    cyclique = (interval != 0) ? true : false;
    txInterval = interval;
    txBytes(payload, length, port, confirm);
}

bool TTN_esp32::poll(uint8_t port, uint8_t confirm)
{
    return sendBytes(0, 1, port, confirm);
}

bool TTN_esp32::stop()
{
    ESP_LOGI(TAG, "ttn_task=%d", TTN_task_Handle);
    if (TTN_task_Handle != NULL)
    {
        ESP_LOGI(TAG, "delete ttn task");
        vTaskDelete(TTN_task_Handle);
        TTN_task_Handle = NULL;
    }
    return true;
}

bool TTN_esp32::isRunning(void)
{
    if (TTN_task_Handle == NULL)
    {
        return false;
    }
    return true;
}

void TTN_esp32::onMessage(void (*callback)(const uint8_t* payload, size_t size, int rssi))
{
    messageCallback = callback;
}

void TTN_esp32::onEvent(void (*callback)(const ev_t event))
{
    eventCallback = callback;
}

bool TTN_esp32::isJoined()
{
    return joined;
}

bool TTN_esp32::isTransceiving()
{
    return LMIC.opmode & OP_TXRXPEND;
}

uint32_t TTN_esp32::waitForPendingTransactions()
{
    uint32_t waited = 0;
    while (isTransceiving())
    {
        waited += 100;
        delay(100);
    }
    return waited;
}

bool TTN_esp32::hasSession()
{
    return session;
}

bool TTN_esp32::storeSession(devaddr_t deviceAddress, u1_t networkSessionKey[16], u1_t applicationRouterSessionKey[16])
{
    bool success = session;
    if (!session)
    {
        uint8_t dev_adr_buf[4];
        dev_adr_buf[0] = (deviceAddress >> 24) & 0xFF;
        dev_adr_buf[1] = (deviceAddress >> 16) & 0xFF;
        dev_adr_buf[2] = (deviceAddress >> 8) & 0xFF;
        dev_adr_buf[3] = deviceAddress & 0xFF;

        uint8_t net_session_key_buf[16];
        uint8_t app_session_key_buf[16];
        std::copy(networkSessionKey, networkSessionKey + 16, net_session_key_buf);
        std::copy(applicationRouterSessionKey, applicationRouterSessionKey + 16, app_session_key_buf);

        HandleCloser handleCloser {};
        if (NVSHandler::openNvsWrite(NVS_FLASH_PARTITION, handleCloser))
        {
            if (NVSHandler::writeNvsValue(handleCloser, NVS_FLASH_KEY_DEV_ADDR, dev_adr_buf, sizeof(dev_adr_buf))
                && NVSHandler::writeNvsValue(
                    handleCloser, NVS_FLASH_KEY_NWK_SESSION_KEY, net_session_key_buf, sizeof(net_session_key_buf))
                && NVSHandler::writeNvsValue(
                    handleCloser, NVS_FLASH_KEY_APP_SESSION_KEY, app_session_key_buf, sizeof(app_session_key_buf)))
            {
                success = session = NVSHandler::commit(handleCloser);
                if (success)
                {
                    Serial.println(F("Successfully stored session"));
                    ESP_LOGI(TAG, "Session saved in NVS storage");
                }
            }
        }
    }
    return success;
}

bool TTN_esp32::getSession(char* deviceAddress, char* networkSessionKey, char* applicationRouterSessionKey)
{
    bool success = false;
    HandleCloser handleCloser {};
    if (NVSHandler::openNvsRead(NVS_FLASH_PARTITION, handleCloser))
    {
        uint8_t buf_dev_adr[4];
        uint8_t buf_net_s_key[16];
        uint8_t buf_app_s_key[16];

        if (NVSHandler::readNvsValue(handleCloser, NVS_FLASH_KEY_DEV_ADDR, buf_dev_adr, sizeof(dev_adr))
            && NVSHandler::readNvsValue(
                handleCloser, NVS_FLASH_KEY_NWK_SESSION_KEY, buf_net_s_key, sizeof(net_session_key))
            && NVSHandler::readNvsValue(
                handleCloser, NVS_FLASH_KEY_APP_SESSION_KEY, buf_app_s_key, sizeof(app_session_key)))
        {
            std::copy(buf_dev_adr, buf_dev_adr + 4, deviceAddress);
            std::copy(buf_net_s_key, buf_net_s_key + 16, networkSessionKey);
            std::copy(buf_app_s_key, buf_app_s_key + 16, applicationRouterSessionKey);
            success = true;
        }
    }
    return success;
}

void TTN_esp32::deleteSession()
{
    memset(dev_adr, 0, sizeof(dev_adr));
    memset(net_session_key, 0, sizeof(net_session_key));
    memset(app_session_key, 0, sizeof(app_session_key));
}

bool TTN_esp32::isProvisioned()
{
    return provisioned;
}

bool TTN_esp32::saveKeys()
{
    bool success = false;

    HandleCloser handleCloser {};
    if (NVSHandler::openNvsWrite(NVS_FLASH_PARTITION, handleCloser))
    {
        if (NVSHandler::writeNvsValue(handleCloser, NVS_FLASH_KEY_DEV_EUI, dev_eui, sizeof(dev_eui))
            && NVSHandler::writeNvsValue(handleCloser, NVS_FLASH_KEY_APP_EUI, app_eui, sizeof(app_eui))
            && NVSHandler::writeNvsValue(handleCloser, NVS_FLASH_KEY_APP_KEY, app_key, sizeof(app_key)))
        {
            success = NVSHandler::commit(handleCloser);
            ESP_LOGI(TAG, "Dev EUI, app EUI and app key saved in NVS storage");
        }
    }

    return success;
}

bool TTN_esp32::restoreKeys(bool silent)
{
    HandleCloser handleCloser {};
    if (NVSHandler::openNvsRead(NVS_FLASH_PARTITION, handleCloser))
    {
        uint8_t buf_dev_eui[8];
        uint8_t buf_app_eui[8];
        uint8_t buf_app_key[16];

        if (NVSHandler::readNvsValue(handleCloser, NVS_FLASH_KEY_DEV_EUI, buf_dev_eui, sizeof(dev_eui), silent)
            && NVSHandler::readNvsValue(handleCloser, NVS_FLASH_KEY_APP_EUI, buf_app_eui, sizeof(app_eui), silent)
            && NVSHandler::readNvsValue(handleCloser, NVS_FLASH_KEY_APP_KEY, buf_app_key, sizeof(app_key), silent))
        {
            std::copy(buf_dev_eui, buf_dev_eui + 8, dev_eui);
            std::copy(buf_app_eui, buf_app_eui + 8, app_eui);
            std::copy(buf_app_key, buf_app_key + 16, app_key);

            checkKeys();

            if (provisioned)
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
            provisioned = false;
        }
    }
    return provisioned;
}

bool TTN_esp32::eraseKeys()
{
    bool success = false;
    uint8_t emptyBuf[16] = {0};
    HandleCloser handleCloser {};
    if (NVSHandler::openNvsWrite(NVS_FLASH_PARTITION, handleCloser))
    {
        if (NVSHandler::writeNvsValue(handleCloser, NVS_FLASH_KEY_DEV_EUI, emptyBuf, sizeof(dev_eui))
            && NVSHandler::writeNvsValue(handleCloser, NVS_FLASH_KEY_APP_EUI, emptyBuf, sizeof(app_eui))
            && NVSHandler::writeNvsValue(handleCloser, NVS_FLASH_KEY_APP_KEY, emptyBuf, sizeof(app_key)))
        {
            success = NVSHandler::commit(handleCloser);
            ESP_LOGI(TAG, "Dev EUI, app EUI and app key erased in NVS storage");
        }
    }
    return success;
}

bool TTN_esp32::storeFrameCounter()
{
    bool success = false;
    HandleCloser handleCloser {};
    if (NVSHandler::openNvsWrite(NVS_FLASH_PARTITION, handleCloser))
    {
        uint8_t buf_seq_num[4];
        buf_seq_num[0] = (sequenceNumberUp >> 24) & 0xFF;
        buf_seq_num[1] = (sequenceNumberUp >> 16) & 0xFF;
        buf_seq_num[2] = (sequenceNumberUp >> 8) & 0xFF;
        buf_seq_num[3] = sequenceNumberUp & 0xFF;
        if (NVSHandler::writeNvsValue(handleCloser, NVS_FLASH_KEY_SEQ_NUM_UP, buf_seq_num, sizeof(buf_seq_num)))
        {
            success = NVSHandler::commit(handleCloser);
            if (success)
            {
                //Serial.println(F("Successfully stored sequence number"));
                ESP_LOGI(TAG, "Frame counter saved in NVS storage");
            }
        }
    }

    return success;
}

uint32_t TTN_esp32::getFrameCounter()
{
    uint32_t number = 0;
    HandleCloser handleCloser {};
    if (NVSHandler::openNvsRead(NVS_FLASH_PARTITION, handleCloser))
    {
        uint8_t buf_seq_num[4];
        if (NVSHandler::readNvsValue(handleCloser, NVS_FLASH_KEY_SEQ_NUM_UP, buf_seq_num, sizeof(buf_seq_num)))
        {
            number = buf_seq_num[0] << 24 | buf_seq_num[1] << 16 | buf_seq_num[2] << 8 | buf_seq_num[3];
        }
    }

    return number;
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

uint8_t TTN_esp32::getDatarate()
{
    return LMIC.datarate;
}

bool TTN_esp32::setDataRate(uint8_t rate)
{
    LMIC.datarate = rate;
    return true;
}

void TTN_esp32::setTXInterval(uint32_t interval = 60)
{
    /*_interval=INTERVAL;*/
    txInterval = interval;
}

size_t TTN_esp32::getAppEui(char* buffer, size_t size)
{
    if (size < 8)
    {
        return 0;
    }
    uint8_t buf[8];
    for (size_t i = 0; i < 8; i++)
    {
        buf[i] = app_eui[i];
    }
    ByteArrayUtils::swapBytes(buf, 8);
    ByteArrayUtils::binToHexStr(buf, 8, buffer);
    buffer[16] = '\0';
    return 8;
}

String TTN_esp32::getAppEui()
{
    char hexbuf[17] = {0};
    for (size_t i = 0; i < 8; i++)
    {
        sprintf(hexbuf + (2 * i), "%02X", app_eui[7 - i]);
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
            buf[i] = dev_eui[i];
        }
    }
    ByteArrayUtils::swapBytes(buf, 8);
    ByteArrayUtils::binToHexStr(buf, 8, buffer);
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
        ByteArrayUtils::binToHexStr(mac, 6, hexbuf);
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
            sprintf(hexbuf + (2 * i), "%02X", dev_eui[7 - i]);
        }
    }
    return String(hexbuf);
}

String TTN_esp32::getMac()
{
    uint8_t mac[6];
    esp_err_t err = esp_efuse_mac_get_default(mac);
    ESP_ERROR_CHECK(err);
    char buf[20];
    for (size_t i = 0; i < 5; i++)
    {
        sprintf(buf + (3 * i), "%02X:", mac[i]);
    }
    sprintf(buf + (3 * 5), "%02X", mac[5]);

    return String(buf);
}

uint8_t TTN_esp32::getPort()
{
    return _port;
}

uint32_t TTN_esp32::getFrequency()
{
    return LMIC.freq;
}

int8_t TTN_esp32::getTXPower()
{
    return LMIC.txpow;
}

bool TTN_esp32::setDevEui(byte* value)
{
    memcpy(dev_eui, value, 8);
    return true;
}

bool TTN_esp32::setAppEui(byte* value)
{
    memcpy(app_eui, value, 8);
    return true;
}

bool TTN_esp32::setAppKey(byte* value)
{
    memcpy(app_key, value, 16);
    return true;
}
size_t TTN_esp32::getDevEui(byte* buf)
{
    memcpy(buf, dev_eui, 8);
    ByteArrayUtils::swapBytes(buf, 8);
    return 8;
}

size_t TTN_esp32::getAppEui(byte* buf)
{
    memcpy(buf, app_eui, 8);
    ByteArrayUtils::swapBytes(buf, 8);
    return 8;
}

size_t TTN_esp32::getAppKey(byte* buf)
{
    memcpy(buf, app_key, 16);
    return 16;
}

/************
 * Private
 ************/

bool TTN_esp32::txBytes(uint8_t* payload, size_t length, uint8_t port, uint8_t confirm)
{
    _message = payload;
    _length = length;
    _port = port;
    _confirm = confirm;

    txMessage(&sendjob);
    if ((LMIC.opmode & OP_TXDATA) == 0)
    {
        return false;
    }

    return true;
}

void TTN_esp32::txMessage(osjob_t* job)
{
    if (joined)
    {
        // Check if there is not a current TX/RX job running
        if (LMIC.opmode & OP_TXRXPEND)
        {
#ifdef DEBUG
            Serial.println(F("Pending transaction, not sending"));
#endif // DEBUG
        }
        else
        {
            // Prepare upstream data transmission at the next possible time.
            LMIC_setTxData2(_port, _message, _length, _confirm);
#ifdef DEBUG
            Serial.println(F("Packet queued"));
#endif // DEBUG
        }
        // Next TX is scheduled after TX_COMPLETE event.
    }
#ifdef DEBUG
    else
    {
        Serial.println(F("Not connected/joined to TTN"));
    }
#endif // DEBUG
}

void TTN_esp32::personalize(u4_t netID, u4_t DevAddr, uint8_t* NwkSKey, uint8_t* AppSKey)
{
    LMIC_setClockError(MAX_CLOCK_ERROR * 7 / 100);
    LMIC_reset();
    LMIC_setSession(netID, DevAddr, NwkSKey, AppSKey);
#if defined(CFG_eu868)
    // Set up the channels used by the Things Network, which corresponds
    // to the defaults of most gateways. Without this, only three base
    // channels from the LoRaWAN specification are used, which certainly
    // works, so it is good for debugging, but can overload those
    // frequencies, so be sure to configure the full frequency range of
    // your network here (unless your network autoconfigures them).
    // Setting up channels should happen after LMIC_setSession, as that
    // configures the minimal channel set.
    // NA-US channels 0-71 are configured automatically
    LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),
        BAND_CENTI); // g-band
    LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B),
        BAND_CENTI); // g-band
    LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),
        BAND_CENTI); // g-band
    LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),
        BAND_CENTI); // g-band
    LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),
        BAND_CENTI); // g-band
    LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),
        BAND_CENTI); // g-band
    LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),
        BAND_CENTI); // g-band
    LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),
        BAND_CENTI); // g-band
    LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK, DR_FSK),
        BAND_MILLI); // g2-band
#endif
    // Disable link check validation
    LMIC_setLinkCheckMode(0);

    // TTN uses SF9 for its RX2 window.
    LMIC.dn2Dr = DR_SF9;

    // Set data rate and transmit power for uplink
    LMIC_setDrTxpow(DR_SF7, 14);

    // Set the sequence number of the packet
    LMIC_setSeqnoUp(sequenceNumberUp);

    // Start job
    joined = true;
    xTaskCreatePinnedToCore(loopStack, "ttnTask", 2048, (void*)1, (5 | portPRIVILEGE_BIT), &TTN_task_Handle, 1);
}

bool TTN_esp32::decode(bool includeDevEui, const char* devEui, const char* appEui, const char* appKey)
{
    uint8_t buf_dev_eui[8];
    uint8_t buf_app_eui[8];
    uint8_t buf_app_key[16];

    if (includeDevEui && (strlen(devEui) != 16 || !ByteArrayUtils::hexStrToBin(devEui, buf_dev_eui, 8)))
    {
        ESP_LOGW(TAG, "Invalid device EUI: %s", devEui);
        return false;
    }

    if (includeDevEui)
    {
        ByteArrayUtils::swapBytes(buf_dev_eui, 8);
    }

    if (strlen(appEui) != 16 || !ByteArrayUtils::hexStrToBin(appEui, buf_app_eui, 8))
    {
        ESP_LOGW(TAG, "Invalid application EUI: %s", appEui);
        return false;
    }

    ByteArrayUtils::swapBytes(buf_app_eui, 8);

    if (strlen(appKey) != 32 || !ByteArrayUtils::hexStrToBin(appKey, buf_app_key, 16))
    {
        ESP_LOGW(TAG, "Invalid application key: %s", appKey);
        return false;
    }

    if (includeDevEui)
    {
        std::copy(buf_dev_eui, buf_dev_eui + 8, dev_eui);
    }
    std::copy(buf_app_eui, buf_app_eui + 8, app_eui);
    std::copy(buf_app_key, buf_app_key + 16, app_key);

    checkKeys();

    return true;
}

void TTN_esp32::checkKeys()
{
    provisioned = !ByteArrayUtils::isAllZeros(dev_eui, sizeof(dev_eui))
        && !ByteArrayUtils::isAllZeros(app_eui, sizeof(app_eui))
        && !ByteArrayUtils::isAllZeros(app_key, sizeof(app_key));
    session = !ByteArrayUtils::isAllZeros(dev_adr, sizeof(dev_adr))
        && !ByteArrayUtils::isAllZeros(app_session_key, sizeof(app_session_key))
        && !ByteArrayUtils::isAllZeros(net_session_key, sizeof(net_session_key)) && sequenceNumberUp != 0x00;

#ifdef DEBUG
    Serial.print(F("[checkKeys] "));
    if (provisioned)
    {
        Serial.print(F("provisioned, "));
    }
    else
    {
        Serial.print(F("unprovisioned, "));
    }
    if (session)
    {
        Serial.println(F("session "));
    }
    else
    {
        Serial.println(F("no session "));
    }
#endif
}

void TTN_esp32::loopStack(void* parameter)
{
    for (;;)
    {
        os_runloop_once();
        vTaskDelay(LOOP_TTN_MS / portTICK_PERIOD_MS);
    }
}

static const char* const eventNames[] = {LMIC_EVENT_NAME_TABLE__INIT};

void onEvent(ev_t event)
{
    TTN_esp32* ttn = TTN_esp32::getInstance();
#ifdef DEBUG
    Serial.print(os_getTime());
    Serial.print(": ");
    // get event message
    if (event < sizeof(eventNames) / sizeof(eventNames[0]))
    {
        Serial.print("[Event] ");
        Serial.println((eventNames[event] + 3)); // +3 to strip "EV_"
    }
    else
    {
        Serial.print("[Event] Unknown: ");
        Serial.println(event);
    }
#endif // DEBUG

    switch (event)
    {
    case EV_JOINING:
        ttn->joined = false;
        break;
    case EV_JOINED:
    {
        u4_t netid = 0;
        devaddr_t devaddr = 0;
        u1_t nwkKey[16];
        u1_t artKey[16];
        LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
        std::copy(nwkKey, nwkKey + 16, net_session_key);
        std::copy(artKey, artKey + 16, app_session_key);
        dev_adr[0] = (devaddr >> 24) & 0xFF;
        dev_adr[1] = (devaddr >> 16) & 0xFF;
        dev_adr[2] = (devaddr >> 8) & 0xFF;
        dev_adr[3] = devaddr & 0xFF;
#ifdef DEBUG
        Serial.println(F("EV_JOINED"));
        Serial.print("netid: ");
        Serial.println(netid, DEC);
        Serial.print("devAdr: ");
        Serial.println(devaddr, HEX);
        Serial.print("artKey: ");
        for (size_t i = 0; i < sizeof(artKey); ++i)
        {
            Serial.print(artKey[i], HEX);
        }
        Serial.println("");
        Serial.print("nwkKey: ");
        for (size_t i = 0; i < sizeof(nwkKey); ++i)
        {
            Serial.print(nwkKey[i], HEX);
        }
        Serial.println();
#endif // DEBUG
    }
        ttn->joined = true;
        // Disable link check validation (automatically enabled
        // during join, but because slow data rates change max TX
        // size, we don't use it in this example.)
        LMIC_setLinkCheckMode(0);
        break;
    case EV_JOIN_FAILED:
        ttn->joined = false;
        break;
    case EV_TXCOMPLETE:
        sequenceNumberUp = LMIC.seqnoUp;
#ifdef DEBUG
        if (LMIC.txrxFlags & TXRX_ACK)
        {
            Serial.println(F("Received ack"));
        }
#endif // DEBUG
        if (LMIC.dataLen)
        {
#ifdef DEBUG
            Serial.print(F("Received "));
            Serial.print(LMIC.dataLen);
            Serial.println(F(" bytes of payload"));
            String JSONMessage = "";
            for (byte i = LMIC.dataBeg; i < LMIC.dataBeg + LMIC.dataLen; i++)
            {
                JSONMessage += (char)LMIC.frame[i];
            }
            Serial.println(JSONMessage);
#endif // DEBUG

            if (ttn->messageCallback)
            {
                uint8_t downlink[LMIC.dataLen];
				uint8_t offset = 9;// offset to get data.
                std::copy(LMIC.frame+offset, LMIC.frame+offset + LMIC.dataLen, downlink);
                ttn->messageCallback(downlink, LMIC.dataLen, LMIC.rssi);
            }
        }
        // Schedule next transmission
        // if (cyclique)
        // {
        //     os_setTimedCallback(&ttn.sendjob, os_getTime() + sec2osticks(txInterval), ttn.txMessage);
        // }
        break;
    case EV_RESET:
        ttn->joined = false;
        break;
    case EV_LINK_DEAD:
        ttn->joined = false;
        break;

    default:
        break;
    }

    if (ttn->eventCallback)
    {
        ttn->eventCallback(event);
    }
}
