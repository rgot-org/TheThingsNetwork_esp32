#ifndef _BYTE_ARRAY_UTILS_H
#define _BYTE_ARRAY_UTILS_H

class ByteArrayUtils
{
public:
    static bool hexStrToBin(const char* hex, uint8_t* buf, int len)
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

    static int hexTupleToByte(const char* hex)
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

    static int hexDigitToVal(char ch)
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

    static void binToHexStr(const uint8_t* buf, int len, char* hex)
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

    static char valToHexDigit(int val) { return "0123456789ABCDEF"[val]; }

    static void swapBytes(uint8_t* buf, int len)
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

    static bool isAllZeros(const uint8_t* buf, int len)
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

private:
    ByteArrayUtils();
    ~ByteArrayUtils();
};

#endif /* _BYTE_ARRAY_UTILS_H */