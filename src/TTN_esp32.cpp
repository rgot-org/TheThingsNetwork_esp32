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
#define DEBUG

static unsigned TX_INTERVAL = 0;
static bool cyclique = false;
static osjob_t sendjob;
// static osjobcb_t sendMsg;
TTN_esp32* TTN_esp32::instance = 0;

// --- LMIC callbacks

// This EUI must be in little-endian format, so least-significant-byte first.
// When copying an EUI from ttnctl output, this means to reverse the bytes.
// For TTN issued EUIs the last bytes should be 0xD5, 0xB3, 0x70.
// The order is swapped in provisioning_decode_keys().
void os_getArtEui(u1_t* buf)
{
    memcpy(buf, TTN_esp32::getInstance()->app_eui, 8);
}

// This should also be in little endian format, see above.
void os_getDevEui(u1_t* buf)
{
    memcpy(buf, TTN_esp32::getInstance()->dev_eui, 8);
}

// This key should be in big endian format (or, since it is not really a number
// but a block of memory, endianness does not really apply). In practice, a key
// taken from ttnctl can be copied as-is.
void os_getDevKey(u1_t* buf)
{
    memcpy(buf, TTN_esp32::getInstance()->app_key, 16);
}

/************
 * Public
 ************/

// --- Constructor
TTN_esp32::TTN_esp32()
    : dev_eui {0, 0, 0, 0, 0, 0, 0, 0},
      app_eui {0, 0, 0, 0, 0, 0, 0, 0},
      app_key {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      app_session_key {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      net_session_key {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      dev_adr {0, 0, 0, 0},
      sequenceNumberUp {0},
      joined {false},
      provisioned {false},
      session {false},
      TTN_task_Handle {NULL},
      _message {0},
      _length {1},
      _port {1},
      _confirm {0}
{
    // restoreKeys();
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
    ByteArrayUtils::hexStrToBin(nwkSKey, net_session_key, 16);
    ByteArrayUtils::hexStrToBin(appSKey, app_session_key, 16);
    ByteArrayUtils::hexStrToBin(devAddr, dev_adr, 4);
    // saveKeys();
    return true;
}

bool TTN_esp32::join()
{
    bool success = false;

    if (!provisioned && !session)
    {
        restoreKeys();
    }

    if (session)
    {
        Serial.println("Using stored session to join");
        devaddr_t dev_addr = dev_adr[0] << 24 | dev_adr[1] << 16 | dev_adr[2] << 8 | dev_adr[3];
        personalize(0x13, dev_addr, net_session_key, app_session_key);
        success = true;
    }
    else if (provisioned)
    {
        Serial.println("Using stored keys to join");
        LMIC_setClockError(MAX_CLOCK_ERROR * 7 / 100);
        LMIC_unjoin();
        LMIC_startJoining();

#ifdef DEBUG
        Serial.println("joined");
#endif // DEBUG

        xTaskCreatePinnedToCore(loopStack, "ttnTask", 2048, (void*)1, (5 | portPRIVILEGE_BIT), TTN_task_Handle, 1);
        success = true;
    }
    else
    {
        ESP_LOGW(TAG, "Device EUI, App EUI and/or App key have not been provided");
        Serial.println("No keys provided");
    }
    return success;
}

bool TTN_esp32::join(const char* appEui, const char* appKey, bool force, int8_t retries, uint32_t retryDelay)
{
    // if(!provision(appEui, appKey)) return false;
    restoreKeys();
    if (force || !provisioned)
    {
        provision(appEui, appKey);
    }
    return join();
}

bool TTN_esp32::join(
    const char* devEui, const char* appEui, const char* appKey, bool force, int8_t retries, uint32_t retryDelay)
{
    restoreKeys();
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
    Serial.printf("str_deva: %s\n", devAddr);
    Serial.printf("int_devaddr: %X\n", dev_addr);
#endif // DEBUG
    personalize(0x13, dev_addr, net_session_key, app_session_key);
    return true;
}

bool TTN_esp32::sendBytes(uint8_t* payload, size_t length, uint8_t port, uint8_t confirm)
{
    cyclique = false;
    return txBytes(payload, length, port, confirm);
}

void TTN_esp32::sendBytesAtInterval(uint8_t* payload, size_t length, unsigned interval, uint8_t port, uint8_t confirm)
{
    cyclique = (interval != 0) ? true : false;
    TX_INTERVAL = interval;
    txBytes(payload, length, port, confirm);
}

bool TTN_esp32::poll(uint8_t port, uint8_t confirm)
{
    return sendBytes(0, 1, port, confirm);
}

bool TTN_esp32::stopTNN()
{
    if (TTN_task_Handle != NULL)
    {
        vTaskDelete(TTN_task_Handle);
    }
    return true;
}

void TTN_esp32::onMessage(void (*callback)(const uint8_t* payload, size_t size, int rssi))
{
    messageCallback = callback;
}

bool TTN_esp32::isJoined()
{
    return joined;
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
        dev_adr[0] = (deviceAddress >> 24) & 0xFF;
        dev_adr[1] = (deviceAddress >> 16) & 0xFF;
        dev_adr[2] = (deviceAddress >> 8) & 0xFF;
        dev_adr[3] = deviceAddress & 0xFF;

        std::copy(networkSessionKey, networkSessionKey + 16, net_session_key);
        std::copy(applicationRouterSessionKey, applicationRouterSessionKey + 16, app_session_key);

        HandleCloser handleCloser {};
        if (NVSHandler::openNvsWrite(NVS_FLASH_PARTITION, handleCloser))
        {
            if (NVSHandler::writeNvsValue(handleCloser, NVS_FLASH_KEY_DEV_ADDR, dev_adr, sizeof(dev_adr))
                && NVSHandler::writeNvsValue(
                    handleCloser, NVS_FLASH_KEY_NWK_SESSION_KEY, net_session_key, sizeof(net_session_key))
                && NVSHandler::writeNvsValue(
                    handleCloser, NVS_FLASH_KEY_APP_SESSION_KEY, app_session_key, sizeof(app_session_key)))
            {
                success = session = NVSHandler::commit(handleCloser);
                if (success)
                {
                    Serial.println("Successfully stored session");
                    ESP_LOGI(TAG, "Session saved in NVS storage");
                }
            }
        }
    }
    return success;
}

void TTN_esp32::deleteSession()
{
    memset(dev_adr, 0, sizeof(dev_adr));
    memset(net_session_key, 0, sizeof(net_session_key));
    memset(app_session_key, 0, sizeof(app_session_key));
    saveKeys();
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
        uint8_t buf_seq_num[4];
        buf_seq_num[0] = (sequenceNumberUp >> 24) & 0xFF;
        buf_seq_num[1] = (sequenceNumberUp >> 16) & 0xFF;
        buf_seq_num[2] = (sequenceNumberUp >> 8) & 0xFF;
        buf_seq_num[3] = sequenceNumberUp & 0xFF;

        if (NVSHandler::writeNvsValue(handleCloser, NVS_FLASH_KEY_DEV_EUI, dev_eui, sizeof(dev_eui))
            && NVSHandler::writeNvsValue(handleCloser, NVS_FLASH_KEY_APP_EUI, app_eui, sizeof(app_eui))
            && NVSHandler::writeNvsValue(handleCloser, NVS_FLASH_KEY_APP_KEY, app_key, sizeof(app_key))
            && NVSHandler::writeNvsValue(handleCloser, NVS_FLASH_KEY_DEV_ADDR, dev_adr, sizeof(dev_adr))
            && NVSHandler::writeNvsValue(
                handleCloser, NVS_FLASH_KEY_NWK_SESSION_KEY, net_session_key, sizeof(net_session_key))
            && NVSHandler::writeNvsValue(
                handleCloser, NVS_FLASH_KEY_APP_SESSION_KEY, app_session_key, sizeof(app_session_key))
            && NVSHandler::writeNvsValue(handleCloser, NVS_FLASH_KEY_SEQ_NUM_UP, buf_seq_num, sizeof(buf_seq_num)))
        {
            success = NVSHandler::commit(handleCloser);
            ESP_LOGI(TAG, "Dev and app EUI and app key saved in NVS storage");
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

        uint8_t buf_dev_adr[4];
        uint8_t buf_net_s_key[16];
        uint8_t buf_app_s_key[16];
        uint8_t buf_seq_num[4];

        if (NVSHandler::readNvsValue(handleCloser, NVS_FLASH_KEY_DEV_EUI, buf_dev_eui, sizeof(dev_eui), silent)
            && NVSHandler::readNvsValue(handleCloser, NVS_FLASH_KEY_APP_EUI, buf_app_eui, sizeof(app_eui), silent)
            && NVSHandler::readNvsValue(handleCloser, NVS_FLASH_KEY_APP_KEY, buf_app_key, sizeof(app_key), silent)
            && NVSHandler::readNvsValue(handleCloser, NVS_FLASH_KEY_DEV_ADDR, buf_dev_adr, sizeof(dev_adr), silent)
            && NVSHandler::readNvsValue(
                handleCloser, NVS_FLASH_KEY_NWK_SESSION_KEY, buf_net_s_key, sizeof(net_session_key), silent)
            && NVSHandler::readNvsValue(
                handleCloser, NVS_FLASH_KEY_APP_SESSION_KEY, buf_app_s_key, sizeof(app_session_key), silent)
            && NVSHandler::readNvsValue(
                handleCloser, NVS_FLASH_KEY_SEQ_NUM_UP, buf_seq_num, sizeof(buf_seq_num), silent))
        {
            memcpy(dev_eui, buf_dev_eui, sizeof(dev_eui));
            memcpy(app_eui, buf_app_eui, sizeof(app_eui));
            memcpy(app_key, buf_app_key, sizeof(app_key));

            memcpy(dev_adr, buf_dev_adr, sizeof(dev_adr));
            memcpy(net_session_key, buf_net_s_key, sizeof(net_session_key));
            memcpy(app_session_key, buf_app_s_key, sizeof(app_session_key));

            sequenceNumberUp = buf_seq_num[0] << 24 | buf_seq_num[1] << 16 | buf_seq_num[2] << 8 | buf_seq_num[3];

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
            Serial.println("[restoreKeys] Could not load keys");
            provisioned = session = false;
        }
    }
    return provisioned;
}

bool TTN_esp32::storeSequenceNumberUp(uint32_t sequenceNumber)
{
    sequenceNumberUp = sequenceNumber;
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
                Serial.println("Successfully stored sequence number");
                ESP_LOGI(TAG, "Sequence number saved in NVS storage");
            }
        }
    }

    return success;
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

void TTN_esp32::setTXInterval(unsigned interval = 60)
{
    /*_interval=INTERVAL;*/
    TX_INTERVAL = interval;
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

uint32_t TTN_esp32::getFrequency()
{
    return LMIC.freq;
}

int8_t TTN_esp32::getTXPower()
{
    return LMIC.txpow;
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

void TTN_esp32::txMessage(osjob_t* j)
{
    if (joined)
    {
        // Check if there is not a current TX/RX job running

        if (LMIC.opmode & OP_TXRXPEND)
        {
#ifdef DEBUG
            Serial.println(F("OP_TXRXPEND, not sending"));
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
        Serial.println("Not connected/joined to TTN");
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
    xTaskCreatePinnedToCore(loopStack, "ttnTask", 2048, (void*)1, (5 | portPRIVILEGE_BIT), TTN_task_Handle, 1);
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
        memcpy(dev_eui, buf_dev_eui, sizeof(dev_eui));
    }
    memcpy(app_eui, buf_app_eui, sizeof(app_eui));
    memcpy(app_key, buf_app_key, sizeof(app_key));

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

    Serial.println("[checkKeys] " + String(provisioned ? "provisioned " : "unprovisioned ")
        + String(session ? "session" : "no session"));
}

void TTN_esp32::loopStack(void* parameter)
{
    for (;;)
    {
        os_runloop_once();
        vTaskDelay(LOOP_TTN_MS / portTICK_PERIOD_MS);
    }
}

void onEvent(ev_t ev)
{
    TTN_esp32* ttn = TTN_esp32::getInstance();
#ifdef DEBUG
    Serial.print(os_getTime());
    Serial.print(": ");
#endif // DEBUG

    switch (ev)
    {
    case EV_SCAN_TIMEOUT:
#ifdef DEBUG
        Serial.println(F("EV_SCAN_TIMEOUT"));
#endif // DEBUG
        break;
    case EV_BEACON_FOUND:
#ifdef DEBUG
        Serial.println(F("EV_BEACON_FOUND"));
#endif // DEBUG
        break;
    case EV_BEACON_MISSED:
#ifdef DEBUG
        Serial.println(F("EV_BEACON_MISSED"));
#endif // DEBUG
        break;
    case EV_BEACON_TRACKED:
#ifdef DEBUG
        Serial.println(F("EV_BEACON_TRACKED"));
#endif // DEBUG
        break;
    case EV_JOINING:
        ttn->joined = false;
#ifdef DEBUG
        Serial.println(F("EV_JOINING"));
#endif // DEBUG
        break;
    case EV_JOINED:
    {
        u4_t netid = 0;
        devaddr_t devaddr = 0;
        u1_t nwkKey[16];
        u1_t artKey[16];
        LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
        ttn->storeSession(devaddr, nwkKey, artKey);
#ifdef DEBUG
        Serial.println(F("EV_JOINED"));
        Serial.print("netid: ");
        Serial.println(netid, DEC);
        Serial.print("devaddr: ");
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
#endif // DEBUG
    }
        ttn->joined = true;
        // Disable link check validation (automatically enabled
        // during join, but because slow data rates change max TX
        // size, we don't use it in this example.

        LMIC_setLinkCheckMode(0);
        break;
        /*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_RFU1:
        ||     Serial.println(F("EV_RFU1"));
        ||     break;
        */
    case EV_JOIN_FAILED:
#ifdef DEBUG
        Serial.println(F("EV_JOIN_FAILED"));
#endif // DEBUG
        ttn->joined = false;
        break;
    case EV_REJOIN_FAILED:
#ifdef DEBUG
        Serial.println(F("EV_REJOIN_FAILED"));
#endif // DEBUG
        break;
    case EV_TXCOMPLETE:
        ttn->storeSequenceNumberUp(LMIC.seqnoUp);
#ifdef DEBUG
        Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
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

            uint8_t downlink[LMIC.dataLen];
            for (byte i = LMIC.dataBeg; i < LMIC.dataBeg + LMIC.dataLen; i++)
            {
                downlink[i - LMIC.dataBeg] = LMIC.frame[i];
            }
            if (ttn->messageCallback)
            {
                ttn->messageCallback(downlink, LMIC.dataLen, LMIC.rssi);
            }
        }
        // Schedule next transmission
        /*	if (cyclique)
                {
                        os_setTimedCallback(&sendjob, os_getTime() +
           sec2osticks(TX_INTERVAL), ttn->txMessage);
                }*/
        break;
    case EV_LOST_TSYNC:
#ifdef DEBUG
        Serial.println(F("EV_LOST_TSYNC"));
#endif // DEBUG
        break;
    case EV_RESET:
#ifdef DEBUG
        Serial.println(F("EV_RESET"));
#endif // DEBUG
        ttn->joined = false;
        break;
    case EV_RXCOMPLETE:
#ifdef DEBUG
        // data received in ping slot
        Serial.println(F("EV_RXCOMPLETE"));
#endif // DEBUG
        break;
    case EV_LINK_DEAD:
        ttn->joined = false;
#ifdef DEBUG
        Serial.println(F("EV_LINK_DEAD"));
#endif // DEBUG
        break;
    case EV_LINK_ALIVE:
#ifdef DEBUG
        Serial.println(F("EV_LINK_ALIVE"));
#endif // DEBUG
        break;
    /*
    || This event is defined but not used in the code. No
    || point in wasting codespace on it.
    ||
    || case EV_SCAN_FOUND:
    ||    Serial.println(F("EV_SCAN_FOUND"));
    ||    break;
    */
    case EV_TXSTART:
#ifdef DEBUG
        Serial.println(F("EV_TXSTART"));
#endif // DEBUG
        break;

    default:
#ifdef DEBUG
        Serial.print(F("Unknown event: "));
        Serial.println((unsigned)ev);
#endif // DEBUG
        break;
    }
}