#ifndef _NVS_HANDLER_H
#define _NVS_HANDLER_H

#include "nvs_flash.h"

class HandleCloser
{
    friend class NVSHandler;

public:
    explicit HandleCloser(nvs_handle handle = 0) : h(handle) {}
    ~HandleCloser() { nvs_close(h); }

protected:
    void setHandle(nvs_handle handle) { this->h = handle; }
    nvs_handle getHandle() { return h; }

private:
    nvs_handle h;
};

class NVSHandler
{
public:
    static bool openNvsRead(const char* partition, HandleCloser& handleCloser);
    static bool openNvsWrite(const char* partition, HandleCloser& handleCloser);
    static bool commit(HandleCloser& HandleCloser);
    static bool readNvsValue(
        HandleCloser& handleCloser, const char* key, uint8_t* data, size_t expected_length, bool silent = true);
    static bool writeNvsValue(HandleCloser& handleCloser, const char* key, const uint8_t* data, size_t len);

private:
    NVSHandler();
    ~NVSHandler();
};

#endif