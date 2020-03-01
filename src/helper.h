#ifndef _HELPER_H
#define _HELPER_H

#include "nvs_flash.h"

static const char* const TAG = "ttn_prov";
static const char* const NVS_FLASH_PARTITION = "ttn";
static const char* const NVS_FLASH_KEY_DEV_EUI = "devEui";
static const char* const NVS_FLASH_KEY_APP_EUI = "appEui";
static const char* const NVS_FLASH_KEY_APP_KEY = "appKey";
static const char* const NVS_FLASH_KEY_APP_SESSION_KEY = "AppSKey";
static const char* const NVS_FLASH_KEY_NWK_SESSION_KEY = "NwkSKey";
static const char* const NVS_FLASH_KEY_DEV_ADDR = "devAddr";
static const char* const NVS_FLASH_KEY_SEQ_NUM_UP = "seqNumUp";

#endif /* _HELPER_H */
