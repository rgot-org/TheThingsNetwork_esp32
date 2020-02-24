#include "NVSHandler.h"

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"

#else
#include "WProgram.h"

#endif

bool NVSHandler::openNvsRead(const char* partition, HandleCloser& handleCloser)
{
    nvs_handle handle = 0;
    const esp_err_t result = nvs_open(partition, NVS_READONLY, &handle);
    handleCloser.setHandle(handle);

    if (result == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "NVS partition does not exist");
    }
    else if (result == ESP_ERR_NVS_NOT_INITIALIZED)
    {
        ESP_LOGW(TAG, "NVS storage is not initialized. Call 'nvs_flash_init()' first.");
    }
    else
    {
        ESP_ERROR_CHECK(result);
    }
    return result == ESP_OK;
}

bool NVSHandler::openNvsWrite(const char* partition, HandleCloser& handleCloser)
{
    nvs_handle handle = 0;
    const esp_err_t result = nvs_open(partition, NVS_READWRITE, &handle);
    handleCloser.setHandle(handle);

    if (result == ESP_ERR_NVS_NOT_INITIALIZED)
    {
        ESP_LOGW(TAG, "NVS storage is not initialized. Call 'nvs_flash_init()' first.");
    }
    else
    {
        ESP_ERROR_CHECK(result);
    }
    return result == ESP_OK;
}

bool NVSHandler::commit(HandleCloser& handleCloser)
{
    const esp_err_t result = nvs_commit(handleCloser.getHandle());
    ESP_ERROR_CHECK(result);
    return result == ESP_OK;
}

bool NVSHandler::readNvsValue(
    HandleCloser& handleCloser, const char* key, uint8_t* data, size_t expected_length, bool silent)
{
    size_t size = expected_length;
    const esp_err_t result = nvs_get_blob(handleCloser.getHandle(), key, data, &size);
#ifdef DEBUG
    Serial.print("[readNvsValue] Reading " + String(key) + " with value ");
    for (size_t i = 0; i < size; ++i)
    {
        Serial.print(data[i], HEX);
    }
    Serial.println();
#endif

    bool success = result == ESP_OK && size == expected_length;
    if (!success && !silent)
    {
        if (result == ESP_OK && size != expected_length)
        {
            ESP_LOGW(TAG, "Invalid size of NVS data for %s", key);
        }

        if (result == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGW(TAG, "No NVS data found for %s", key);
        }
    }

    if (result != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_ERROR_CHECK(result);
    }
    return success;
}

bool NVSHandler::writeNvsValue(HandleCloser& handleCloser, const char* key, const uint8_t* data, size_t len)
{
    uint8_t buf[16];
    if (!readNvsValue(handleCloser, key, buf, len, true) || memcmp(buf, data, len) != 0)
    {
        const esp_err_t result = nvs_set_blob(handleCloser.getHandle(), key, data, len);
        ESP_ERROR_CHECK(result);
        return result == ESP_OK;
    }

    return true;
}