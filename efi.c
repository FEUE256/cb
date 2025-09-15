#include "include/efi.h"
#include "include/cb/gen.h"
#include "include/cb/bcb.h"
#include <stdarg.h>
#include <stdbool.h>

// =========================================================
// Global Variables
// =========================================================
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *cout = NULL; // Console output protocol
EFI_SIMPLE_TEXT_INPUT_PROTOCOL *cin = NULL; // Console input protocol
EFI_RUNTIME_SERVICES *rs = NULL; // Runtime services pointer
EFI_BOOT_SERVICES *bs = NULL; // Boot services pointer
EFI_SYSTEM_TABLE *gST = NULL; // System Table
EFI_HANDLE image = NULL;
int _fltused = 1;
EFI_STATUS status = EFI_SUCCESS;

// Mouse cursor buffer 8x8
EFI_GRAPHICS_OUTPUT_BLT_PIXEL cursor_buffer[] = {
    px_LGRAY, px_LGRAY, px_LGRAY, px_LGRAY, px_LGRAY, px_LGRAY, px_LGRAY, px_LGRAY, // Line 1
    px_LGRAY, px_LGRAY, px_LGRAY, px_LGRAY, px_LGRAY, px_LGRAY, px_LGRAY, px_LGRAY, // Line 2
    px_LGRAY, px_LGRAY, px_LGRAY, px_LGRAY, px_BLUE, px_BLUE, px_BLUE, px_BLUE,     // Line 3
    px_LGRAY, px_LGRAY, px_LGRAY, px_LGRAY, px_LGRAY, px_BLUE, px_BLUE, px_BLUE,    // Line 4
    px_LGRAY, px_LGRAY, px_BLUE, px_LGRAY, px_LGRAY, px_LGRAY, px_BLUE, px_BLUE,    // Line 5
    px_LGRAY, px_LGRAY, px_BLUE, px_BLUE, px_LGRAY, px_LGRAY, px_LGRAY, px_BLUE,    // Line 6
    px_LGRAY, px_LGRAY, px_BLUE, px_BLUE, px_BLUE, px_LGRAY, px_LGRAY, px_LGRAY,    // Line 7
    px_LGRAY, px_LGRAY, px_BLUE, px_BLUE, px_BLUE, px_BLUE, px_LGRAY, px_LGRAY,     // Line 8
};

// =========================================================
// Get key function
// =========================================================
EFI_INPUT_KEY get_key(void) {
    EFI_EVENT events[1];
    EFI_INPUT_KEY key;

    key.UnicodeChar = u'\0';
    key.ScanCode = 0;

    events[0] = cin->WaitForKey;
    UINTN index = 0;
    bs->WaitForEvent(1, events, &index);

    if (index == 0) {
        if (cin->ReadKeyStroke(cin, &key) != EFI_SUCCESS) {
            // Optionally handle the error
            key.UnicodeChar = u'\0';
            key.ScanCode = 0;
        }
    }

    return key;
}

// ===== Egna implementationer med originalnamn =====
void CopyMem(void* dest, const void* src, UINTN size) {
    UINT8* d = (UINT8*)dest;
    const UINT8* s = (const UINT8*)src;
    for (UINTN i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

EFI_STATUS AllocatePool(UINTN size, void** buffer) {
    return bs->AllocatePool(EfiLoaderData, size, buffer);
}

EFI_STATUS FreePool(void* buffer) {
    return bs->FreePool(buffer);
}

EFI_STATUS AllocatePages(EFI_MEMORY_TYPE type, UINTN pages, EFI_PHYSICAL_ADDRESS* addr) {
    return bs->AllocatePages(AllocateAnyPages, type, pages, addr);
}

EFI_STATUS LocateProtocol(EFI_GUID* protocolGuid, void** protocol) {
    return bs->LocateProtocol(protocolGuid, NULL, protocol);
}

VOID Print(IN CONST CHAR16 *fmt, ...) {
    static CHAR16 buffer[8094]; // temporär buffer
    UINTN pos = 0;
    va_list args;
    va_start(args, fmt);

    while (*fmt && pos < sizeof(buffer)/sizeof(buffer[0]) - 1) {
        if (*fmt == L'%' && *(fmt+1)) {
            fmt++;
            switch (*fmt) {
                case L'c': {
                    CHAR16 c = (CHAR16)va_arg(args, int);
                    buffer[pos++] = c;
                    break;
                }
                case L's': {
                    CHAR16 *s = va_arg(args, CHAR16*);
                    while (s && *s && pos < sizeof(buffer)/sizeof(buffer[0]) - 1)
                        buffer[pos++] = *s++;
                    break;
                }
                case L'd': {
                    // integer to string
                    UINT64 val = va_arg(args, int);
                    CHAR16 numbuf[21];
                    UINTN i = 0;
                    if (val == 0) {
                        numbuf[i++] = L'0';
                    } else {
                        while (val > 0 && i < sizeof(numbuf)/sizeof(numbuf[0]) - 1) {
                            numbuf[i++] = (CHAR16)(L'0' + (val % 10));
                            val /= 10;
                        }
                        // vänd strängen
                        for (UINTN j = 0; j < i/2; j++) {
                            CHAR16 tmp = numbuf[j];
                            numbuf[j] = numbuf[i-1-j];
                            numbuf[i-1-j] = tmp;
                        }
                    }
                    for (UINTN j = 0; j < i && pos < sizeof(buffer)/sizeof(buffer[0]) - 1; j++)
                        buffer[pos++] = numbuf[j];
                    break;
                }
                case L'x': {
                    UINT64 val = va_arg(args, int);
                    CHAR16 numbuf[17];
                    UINTN i = 0;
                    if (val == 0) {
                        numbuf[i++] = L'0';
                    } else {
                        while (val > 0 && i < sizeof(numbuf)/sizeof(numbuf[0]) - 1) {
                            UINT64 digit = val & 0xF;
                            numbuf[i++] = (digit < 10) ? (CHAR16)(L'0' + digit) : (CHAR16)(L'A' + digit - 10);
                            val >>= 4;
                        }
                        // vänd strängen
                        for (UINTN j = 0; j < i/2; j++) {
                            CHAR16 tmp = numbuf[j];
                            numbuf[j] = numbuf[i-1-j];
                            numbuf[i-1-j] = tmp;
                        }
                    }
                    for (UINTN j = 0; j < i && pos < sizeof(buffer)/sizeof(buffer[0]) - 1; j++)
                        buffer[pos++] = numbuf[j];
                    break;
                }
                default:
                    buffer[pos++] = L'%';
                    buffer[pos++] = *fmt;
                    break;
            }
        } else {
            buffer[pos++] = *fmt;
        }
        fmt++;
    }

    buffer[pos] = L'\0';
    va_end(args);

    extern EFI_SYSTEM_TABLE *gST;
    gST->ConOut->OutputString(gST->ConOut, buffer);
}

// =========================================================
// Globle GUID's
// =========================================================
EFI_GUID EFI_GLOBAL_VARIABLE = {
    0x8BE4DF61,
    0x93CA,
    0x11D2,
    0xAA,
    0x0D,
    {0x00, 0xE0, 0x98, 0x03, 0x2B, 0x8C}
};

// =========================================================
// Set Global Definitions
// =========================================================
void load_variables(EFI_HANDLE handle, EFI_SYSTEM_TABLE *SystemTable) {
    // Retrieve console output and input protocols
    cout = SystemTable->ConOut;
    cin = SystemTable->ConIn;

    // Retrieve runtime and boot services
    rs = SystemTable->RuntimeServices;
    bs = SystemTable->BootServices;

    // System Table
    gST = SystemTable;

    image = handle;
}

UINTN strlen(const char *s) {
    UINTN len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

// Letar efter substring needle i haystack
char* strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char*)haystack; // tom needle hittas direkt

    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;

        while (*h && *n && (*h == *n)) {
            h++;
            n++;
        }

        if (!*n) {
            return (char*)haystack; // needle hittad
        }
    }
    return NULL; // hittades inte
}

int memcmp(const void *s1, const void *s2, UINTN n) {
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;
    for (UINTN i = 0; i < n; i++) {
        if (p1[i] != p2[i])
            return (p1[i] - p2[i]);
    }
    return 0;
}

// Kollar om ett tecken är en siffra (0-9)
BOOLEAN is_digit(char c) {
    return (c >= '0' && c <= '9');
}

// =========================================================
// Clear Screen
// =========================================================
void clear_screen(void) {
    cout->ClearScreen(cout);
}
// =========================================================
// UINT8 strcyp Function
// =========================================================
CHAR16 *strcpy_u16(CHAR16 *dst, CHAR16 *src) {
    if (!dst) return NULL;
    if (!src) return dst;

    CHAR16 *res = dst;
    while (*src) *dst++ = *src++;
    *dst = u'\0';

    return res;
}

// =========================================================
// UINT8 strcmp Function
// =========================================================
int strncmp_u16(CHAR16 *s1, CHAR16 *s2, UINTN len) {
    if (len == 0) return 0;

    while (len > 0 && *s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
        len--;
    }

    if (len == 0) return 0;
    return *s1 - *s2;
}

// =========================================================
// UINT8 strrchr Function
// =========================================================
CHAR16 *strrchr_u16(CHAR16 *s1, CHAR16 c) {
    CHAR16 *res = NULL;

    while (*s1) {
        if (*s1 == c) {
            res = s1; // Update result to last occurrence
        }
        s1++;
    }
    
    return res;
}

// =========================================================
// UINT8 strcat Function
// =========================================================
CHAR16 *strcat_u16(CHAR16 *dst, CHAR16 *src) {
    CHAR16 *s = dst;

    while (*s) {
        s++;
    }

    while (*src) {
        *s++ = *src++;  // korrekt sätt att kopiera och inkrementera
    }

    *s = u'\0'; // Null-terminera strängen
    
    return dst;
}

// =========================================================
// Custom memcpy Function
// =========================================================
void *memcpy(void *dest, const void *src, size_t count) {
    char *d = (char*)dest;
    const char *s = (const char*)src;
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}

// =========================================================
// Custom memset Function
// =========================================================
void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}


// =========================================================
// Custom itoa Function
// =========================================================
char* itoa(int64_t value, char* str, int base) {
    // Handle 0 explicitly, otherwise empty string is printed
    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }

    int i = 0;
    int isNegative = 0;

    // Handle negative numbers only if the base is 10
    if (value < 0 && base == 10) {
        isNegative = 1;
        value = -value;
    }

    // Process individual digits
    while (value != 0) {
        int rem = value % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value = value / base;
    }

    // Append negative sign for negative numbers
    if (isNegative) {
        str[i++] = '-';
    }

    str[i] = '\0'; // Null terminate the string

    // Reverse the string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }

    return str;
}

// ================================
// Print a number to stderr
// ================================
BOOLEAN eprint_number(UINTN number, UINT8 base, BOOLEAN is_signed) {
    const CHAR16 *digits = u"0123456789ABCDEF";
    CHAR16 buffer[24];  // Hopefully enough for UINTN_MAX (UINT64_MAX) + sign character
    UINTN i = 0;
    BOOLEAN negative = FALSE;

    if (base > 16) {
        cout->OutputString(cout, u"Invalid base specified!\r\n");
        return FALSE;    // Invalid base
    }

    // Only use and print negative numbers if decimal and signed True
    if (base == 10 && is_signed && (INTN)number < 0) {
       number = -(INTN)number;  // Get absolute value of correct signed value to get digits to print
       negative = TRUE;
    }

    do {
       buffer[i++] = digits[number % base];
       number /= base;
    } while (number > 0);

    switch (base) {
        case 2:
            // Binary
            buffer[i++] = u'b';
            buffer[i++] = u'0';
            break;

        case 8:
            // Octal
            buffer[i++] = u'o';
            buffer[i++] = u'0';
            break;

        case 10:
            // Decimal
            if (negative) buffer[i++] = u'-';
            break;

        case 16:
            // Hexadecimal
            buffer[i++] = u'x';
            buffer[i++] = u'0';
            break;

        default:
            // Maybe invalid base, but we'll go with it (no special processing)
            break;
    }

    // NULL terminate string
    buffer[i--] = u'\0';

    // Reverse buffer before printing
    for (UINTN j = 0; j < i; j++, i--) {
        // Swap digits
        UINTN temp = buffer[i];
        buffer[i] = buffer[j];
        buffer[j] = temp;
    }

    // Print number string
    cout->OutputString(cout, buffer);

    return TRUE;
}

// =========================================================
// Custom itoa16 Function
// =========================================================
void itoa16(UINTN value, CHAR16 *buffer) {
    int i = 0;
    CHAR16 temp[6];
    if (value == 0) {
        buffer[0] = L'0';
        buffer[1] = L'\0';
        return;
    }

    while (value > 0 && i < 5) {
        temp[i++] = (CHAR16)(L'0' + (value % 10));
        value /= 10;
    }

    for (int j = 0; j < i; j++) {
        buffer[j] = temp[i - j - 1];
    }
    buffer[i] = L'\0';
}

// =========================================================
// Custom itoa2 Function
// =========================================================
void itoa2(UINTN value, CHAR16 *buffer) {
    buffer[0] = (CHAR16)(L'0' + (value / 10));
    buffer[1] = (CHAR16)(L'0' + (value % 10));
    buffer[2] = L'\0';
}

// =========================================================
// Custom utoa16 Function
// =========================================================
void utoa16(unsigned int value, CHAR16 *str) {
    CHAR16 temp[20];
    int i = 0;
    do {
        temp[i++] = L'0' + (value % 10);
        value /= 10;
    } while (value && i < 19);
    temp[i] = L'\0';

    // reverse
    for (int j = 0; j < i; j++) {
        str[j] = temp[i - j - 1];
    }
    str[i] = L'\0';
}

// ====================================
// Print formatted strings to stderr
// ====================================
bool eprintf(CHAR16 *fmt, va_list args) {
    bool result = true;
    CHAR16 charstr[2];    // TODO: Replace initializing this with memset and use = { } initializer

    // Initialize buffers
    charstr[0] = u'\0', charstr[1] = u'\0';

    // Print formatted string values
    for (UINTN i = 0; fmt[i] != u'\0'; i++) {
        if (fmt[i] == u'%') {
            i++;

            // Grab next argument type from input args, and print it
            switch (fmt[i]) {
                case u'c': {
                    // Print CHAR16 value; printf("%c", char)
                    charstr[0] = va_arg(args, int); // Compiler warning says to do this
                    cout->OutputString(cout, charstr);
                }
                break;

                case u's': {
                    // Print CHAR16 string; printf("%s", string)
                    CHAR16 *string = va_arg(args, CHAR16*);
                    cout->OutputString(cout, string);
                }
                break;

                case u'd': {
                    // Print INT32; printf("%d", number_int32)
                    INT32 number = va_arg(args, INT32);
                    eprint_number(number, 10, TRUE);
                }
                break;

                case u'u': {
                    // Print UINT32; printf("%u", number_uint32)
                    UINT32 number = va_arg(args, UINT32);
                    eprint_number(number, 10, FALSE);
                }
                break;

                case u'b': {
                    // Print UINTN as binary; printf("%b", number_uintn)
                    UINTN number = va_arg(args, UINTN);
                    eprint_number(number, 2, FALSE);
                }
                break;

                case u'o': {
                    // Print UINTN as octal; printf("%o", number_uintn)
                    UINTN number = va_arg(args, UINTN);
                    eprint_number(number, 8, FALSE);
                }
                break;

                case u'x': {
                    // Print hex UINTN; printf("%x", number_uintn)
                    UINTN number = va_arg(args, UINTN);
                    eprint_number(number, 16, FALSE);
                }
                break;

                default:
                    cout->OutputString(cout, u"Invalid format specifier: %");
                    charstr[0] = fmt[i];
                    cout->OutputString(cout, charstr);
                    cout->OutputString(cout, u"\r\n");
                    result = false;
                    goto end;
                    break;
            }
        } else {
            // Not formatted string, print next character
            charstr[0] = fmt[i];
            cout->OutputString(cout, charstr);
        }
    }

end:
    return result;
}

// =========================================================
// Custom ultoa16 Function
// =========================================================
void ultoa16(unsigned long value, CHAR16 *str) {
    CHAR16 temp[32];
    int i = 0;
    do {
        temp[i++] = L'0' + (value % 10);
        value /= 10;
    } while (value && i < 31);
    temp[i] = L'\0';

    for (int j = 0; j < i; j++) {
        str[j] = temp[i - j - 1];
    }
    str[i] = L'\0';
}

void PrintBufferChunked(const CHAR16 *buf, UINTN size) {
    const UINTN chunk_size = 1024;
    UINTN offset = 0;

    while (offset < size) {
        UINTN len = (size - offset) > chunk_size ? chunk_size : (size - offset);
        for (UINTN i = 0; i < len; i++) {
            gST->ConOut->OutputString(gST->ConOut, (CHAR16[]){ buf[offset + i], L'\0' });
        }
        offset += len;
    }
}

// ================================
// Print a number to stdout
// ================================
BOOLEAN print_number(UINTN number, UINT8 base, BOOLEAN is_signed) {
    const CHAR16 *digits = u"0123456789ABCDEF";
    CHAR16 buffer[24];  // Hopefully enough for UINTN_MAX (UINT64_MAX) + sign character
    UINTN i = 0;
    BOOLEAN negative = FALSE;

    if (base > 16) {
        cout->OutputString(cout, u"Invalid base specified!\r\n");
        return FALSE;    // Invalid base
    }

    // Only use and print negative numbers if decimal and signed True
    if (base == 10 && is_signed && (INTN)number < 0) {
       number = -(INTN)number;  // Get absolute value of correct signed value to get digits to print
       negative = TRUE;
    }

    do {
       buffer[i++] = digits[number % base];
       number /= base;
    } while (number > 0);

    switch (base) {
        case 2:
            // Binary
            buffer[i++] = u'b';
            buffer[i++] = u'0';
            break;

        case 8:
            // Octal
            buffer[i++] = u'o';
            buffer[i++] = u'0';
            break;

        case 10:
            // Decimal
            if (negative) buffer[i++] = u'-';
            break;

        case 16:
            // Hexadecimal
            buffer[i++] = u'x';
            buffer[i++] = u'0';
            break;

        default:
            // Maybe invalid base, but we'll go with it (no special processing)
            break;
    }

    // NULL terminate string
    buffer[i--] = u'\0';

    // Reverse buffer before printing
    for (UINTN j = 0; j < i; j++, i--) {
        // Swap digits
        UINTN temp = buffer[i];
        buffer[i] = buffer[j];
        buffer[j] = temp;
    }

    // Print number string
    cout->OutputString(cout, buffer);

    return TRUE;
}

// =========================================================
// Custom printf Function
// =========================================================
bool printf(const CHAR16 *format, ...) {
    UINTN row;
    row = gST->ConOut->Mode->CursorRow;
    gST->ConOut->SetCursorPosition(gST->ConOut, 0, row);

    va_list args;
    va_start(args, format);

    CHAR16 buffer[1024];
    UINTN buf_index = 0;

    while (*format && buf_index < sizeof(buffer)/sizeof(CHAR16) - 1) {
        if (*format == L'%') {
            format++; // hoppa över '%'

            // Specialfall: "%04x"
            if (*format == L'0' && *(format+1) == L'4' && *(format+2) == L'x') {
                format += 3;
                unsigned int val = va_arg(args, unsigned int);
                CHAR16 numbuf[5];
                numbuf[4] = L'\0';
                static const CHAR16 hexchars[] = L"0123456789abcdef";
                for (int i = 3; i >= 0; i--) {
                    numbuf[i] = hexchars[val & 0xF];
                    val >>= 4;
                }
                for (int i = 0; numbuf[i] && buf_index < sizeof(buffer)/sizeof(CHAR16) - 1; i++) {
                    buffer[buf_index++] = numbuf[i];
                }
                continue;
            }

            // Vanliga format
            switch (*format) {
                case L's': { // Wide string
                    CHAR16 *str = va_arg(args, CHAR16 *);
                    while (*str && buf_index < sizeof(buffer)/sizeof(CHAR16) - 1) {
                        buffer[buf_index++] = *str++;
                    }
                    break;
                }
                case L'a': { // ASCII string
                    char *astr = va_arg(args, char *);
                    while (*astr && buf_index < sizeof(buffer)/sizeof(CHAR16) - 1) {
                        buffer[buf_index++] = (CHAR16)(*astr++);
                    }
                    break;
                }
                case L'd': {
                    int val = va_arg(args, int);
                    CHAR16 numbuf[20];
                    itoa16(val, numbuf);
                    CHAR16 *p = numbuf;
                    while (*p && buf_index < sizeof(buffer)/sizeof(CHAR16) - 1) {
                        buffer[buf_index++] = *p++;
                    }
                    break;
                }
                case L'u': {
                    unsigned int val = va_arg(args, unsigned int);
                    CHAR16 numbuf[20];
                    utoa16(val, numbuf);
                    CHAR16 *p = numbuf;
                    while (*p && buf_index < sizeof(buffer)/sizeof(CHAR16) - 1) {
                        buffer[buf_index++] = *p++;
                    }
                    break;
                }
                case L'n': { // UINTN
                    UINTN val = va_arg(args, UINTN);
                    CHAR16 numbuf[32];

                #if defined(__x86_64__) || defined(_M_X64)
                    // 64-bit, cast till unsigned long long för säkerhet
                    ultoa16((unsigned long long)val, numbuf);
                #else
                    // 32-bit, cast till unsigned int
                    utoa16((unsigned int)val, numbuf);
                #endif

                    CHAR16 *p = numbuf;
                    while (*p && buf_index < sizeof(buffer)/sizeof(CHAR16) - 1) {
                        buffer[buf_index++] = *p++;
                    }
                    break;
                }
                case L'l': {
                    format++;
                    if (*format == L'u') {
                        unsigned long val = va_arg(args, unsigned long);
                        CHAR16 numbuf[32];
                        ultoa16(val, numbuf);
                        CHAR16 *p = numbuf;
                        while (*p && buf_index < sizeof(buffer)/sizeof(CHAR16) - 1) {
                            buffer[buf_index++] = *p++;
                        }
                        format++; // hoppa över 'u'
                    } else {
                        buffer[buf_index++] = L'%';
                        buffer[buf_index++] = L'l';
                    }
                    break;
                }
                case L'c': {
                    CHAR16 ch = (CHAR16)va_arg(args, int);
                    buffer[buf_index++] = ch;
                    break;
                }
                case L'%': {
                    buffer[buf_index++] = L'%';
                    break;
                }
                case L'x': { // Hexadecimal uppercase
                    unsigned int val = va_arg(args, unsigned int);
                    CHAR16 numbuf[9]; // 8 hex digits + '\0'
                    numbuf[8] = L'\0';
                    static const CHAR16 hexchars_lower[] = L"0123456789abcdef";
                    static const CHAR16 hexchars_upper[] = L"0123456789ABCDEF";
                    const CHAR16 *hexchars = (*format == L'x') ? hexchars_lower : hexchars_upper;

                    for (int i = 7; i >= 0; i--) {
                        numbuf[i] = hexchars[val & 0xF];
                        val >>= 4;
                    }

                    for (int i = 0; numbuf[i] && buf_index < sizeof(buffer)/sizeof(CHAR16) - 1; i++) {
                        buffer[buf_index++] = numbuf[i];
                    }
                    break;
                }
                default:
                    buffer[buf_index++] = L'%';
                    buffer[buf_index++] = *format;
                    break;
            }
        } else {
            buffer[buf_index++] = *format;
        }
        format++;
    }

    buffer[buf_index] = L'\0';

    va_end(args);

    cout->OutputString(cout, buffer);

    return true;
}

// =========================================================
// Custom printfc Function
// =========================================================
bool printfc(CHAR16 *fmt, ...) {
    bool result = true;
    CHAR16 charstr[2];    // TODO: Replace initializing this with memset and use = { } initializer
    va_list args;

    va_start(args, fmt);

    // Initialize buffers
    charstr[0] = u'\0', charstr[1] = u'\0';

    // Print formatted string values
    for (UINTN i = 0; fmt[i] != u'\0'; i++) {
        if (fmt[i] == u'%') {
            i++;

            // Grab next argument type from input args, and print it
            switch (fmt[i]) {
                case u'c': {
                    // Print CHAR16 value; printf("%c", char)
                    charstr[0] = va_arg(args, int); // Compiler warning says to do this
                    cout->OutputString(cout, charstr);
                }
                break;

                case u's': {
                    // Print CHAR16 string; printf("%s", string)
                    CHAR16 *string = va_arg(args, CHAR16*);
                    cout->OutputString(cout, string);
                }
                break;

                case u'd': {
                    // Print INT32; printf("%d", number_int32)
                    INT32 number = va_arg(args, INT32);
                    //print_int(number);
                    print_number(number, 10, TRUE);
                }
                break;

                case u'x': {
                    // Print hex UINTN; printf("%x", number_uintn)
                    UINTN number = va_arg(args, UINTN);
                    //print_hex(number);
                    print_number(number, 16, FALSE);
                }
                break;

                case u'u': {
                    // Print UINT32; printf("%u", number_uint32)
                    UINT32 number = va_arg(args, UINT32);
                    print_number(number, 10, FALSE);
                }
                break;

                case u'b': {
                    // Print UINTN as binary; printf("%b", number_uintn)
                    UINTN number = va_arg(args, UINTN);
                    eprint_number(number, 2, FALSE);
                }
                break;

                case u'o': {
                    // Print UINTN as octal; printf("%o", number_uintn)
                    UINTN number = va_arg(args, UINTN);
                    eprint_number(number, 8, FALSE);
                }
                break;

                default:
                    cout->OutputString(cout, u"Invalid format specifier: %");
                    charstr[0] = fmt[i];
                    cout->OutputString(cout, charstr);
                    cout->OutputString(cout, u"\r\n");
                    result = false;
                    goto end;
                    break;
            }
        } else {
            // Not formatted string, print next character
            charstr[0] = fmt[i];
            cout->OutputString(cout, charstr);
        }
    }

end:
    va_end(args);

    return result;
}

// =========================================================
// Getting The Current Time
// =========================================================
void print_current_time() {
    EFI_TIME Time;
    EFI_STATUS status = rs->GetTime(&Time, NULL);
    
    if (EFI_ERROR(status)) {
        cout->OutputString(cout, L"Failed to get time.\r\n");
        return;
    }

    CHAR16 buffer[8];

    itoa16(Time.Year, buffer);         // Ex: 2025
    cout->OutputString(cout, buffer);
    cout->OutputString(cout, L"-");

    itoa2(Time.Month, buffer);         // Ex: 07
    cout->OutputString(cout, buffer);
    cout->OutputString(cout, L"-");

    itoa2(Time.Day, buffer);           // Ex: 27
    cout->OutputString(cout, buffer);
    cout->OutputString(cout, L" ");

    itoa2(Time.Hour, buffer);          // Ex: 14
    cout->OutputString(cout, buffer);
    cout->OutputString(cout, L":");

    itoa2(Time.Minute, buffer);        // Ex: 33
    cout->OutputString(cout, buffer);
    cout->OutputString(cout, L":");

    itoa2(Time.Second, buffer);        // Ex: 08
    cout->OutputString(cout, buffer);
    cout->OutputString(cout, L"\r\n");
}

// =========================================================
// SPrint Function
// =========================================================
void SPrint(CHAR16 *buffer, UINTN bufferSize, const CHAR16 *format, ...) {
    va_list args;
    va_start(args, format);

    UINTN i = 0;

    while (*format && i < bufferSize - 1) {
        if (*format == L'%') {
            format++; // Skip '%'

            if (*format == L'%') {
                // Literal '%'
                buffer[i++] = L'%';

            } else if (*format == L'x') {
                // Hexadecimal formatting for UINTN
                UINTN value = va_arg(args, UINTN);
                for (INTN shift = (sizeof(UINTN) * 8) - 4; shift >= 0 && i < bufferSize - 1; shift -= 4) {
                    UINTN digit = (value >> shift) & 0xF;
                    buffer[i++] = (digit < 10) ? (CHAR16)(L'0' + digit) : (CHAR16)(L'A' + digit - 10);
                }

            } else if (*format == L'r') {
                // EFI_STATUS formatting as hex with 0x prefix
                EFI_STATUS status = va_arg(args, EFI_STATUS);
                buffer[i++] = L'0';
                if (i < bufferSize - 1) buffer[i++] = L'x';
                for (INTN shift = (sizeof(EFI_STATUS) * 8) - 4; shift >= 0 && i < bufferSize - 1; shift -= 4) {
                    UINTN digit = (status >> shift) & 0xF;
                    buffer[i++] = (digit < 10) ? (CHAR16)(L'0' + digit) : (CHAR16)(L'A' + digit - 10);
                }

            } else if (*format == L's') {
                // String formatting
                CHAR16 *str = va_arg(args, CHAR16 *);
                while (*str && i < bufferSize - 1) {
                    buffer[i++] = *str++;
                }

            } else {
                // Unsupported format specifier: print as literal
                buffer[i++] = L'%';
                if (i < bufferSize - 1) buffer[i++] = *format;
            }
        } else {
            // Regular character
            buffer[i++] = *format;
        }
        format++;
    }

    buffer[i] = L'\0'; // Null-terminate string
    va_end(args);
}

// =========================================================
// Hexadecimal Print Function
// =========================================================
void PrintHex64(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *cout, UINT64 val) {
    CHAR16 hexDigits[] = L"0123456789ABCDEF";
    CHAR16 buffer[17];
    for (int i = 0; i < 16; i++) {
        buffer[15 - i] = hexDigits[val & 0xF];
        val >>= 4;
    }
    buffer[16] = L'\0';
    cout->OutputString(cout, buffer);
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int isspace(int c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

int sscanf(const char *src, const char *fmt, void *out) {
    if (strcmp(fmt, "%63s") == 0) {
        // Läs sträng (max 63 tecken)
        char *sout = (char*)out;
        int i = 0;
        while (*src && !isspace((unsigned char)*src) && i < 63) {
            sout[i++] = *src++;
        }
        sout[i] = '\0';
        return 1;
    }
    else if (strcmp(fmt, "%d") == 0) {
        // Läs heltal (decimal)
        int *iout = (int*)out;
        int sign = 1;
        int val = 0;

        // Hoppa över whitespace
        while (isspace((unsigned char)*src)) src++;

        // Tecken?
        if (*src == '-') { sign = -1; src++; }
        else if (*src == '+') { src++; }

        // Bygg tal
        while (*src >= '0' && *src <= '9') {
            val = val * 10 + (*src - '0');
            src++;
        }

        *iout = val * sign;
        return 1;
    }
    else if (strcmp(fmt, "%x") == 0) {
        // Läs hexadecimalt tal
        int *iout = (int*)out;
        int val = 0;

        // Hoppa över whitespace
        while (isspace((unsigned char)*src)) src++;

        // Tillåt "0x" prefix
        if (src[0] == '0' && (src[1] == 'x' || src[1] == 'X')) {
            src += 2;
        }

        while ((*src >= '0' && *src <= '9') ||
               (*src >= 'a' && *src <= 'f') ||
               (*src >= 'A' && *src <= 'F')) {
            char c = *src++;
            int digit;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = 10 + (c - 'a');
            else digit = 10 + (c - 'A');
            val = (val << 4) | digit;
        }

        *iout = val;
        return 1;
    }

    // Om formatet inte stöds
    return 0;
}

// =========================================================
// OS info
// =========================================================
EFI_STATUS print_os_info(void) {
    clear_screen();

    cout->OutputString(cout, L"Operating System Information:\r\n");

    cout->OutputString(cout, L"Name: ");
    cout->OutputString(cout, OS_NAME);

    cout->OutputString(cout, L"\r\nType: ");
    cout->OutputString(cout, OS_TYPE);

    cout->OutputString(cout, L"\r\nVersion: ");
    cout->OutputString(cout, OS_VERSION);

    cout->OutputString(cout, L"\r\nLanguage: ");
    cout->OutputString(cout, L"UEFI c");

    cout->OutputString(cout, L"\r\nArchitecture: ");
    cout->OutputString(cout, OS_ARCH);

    cout->OutputString(cout, L"\r\nCurrent time and date: ");
    print_current_time();
    cout->OutputString(cout, L"\r\n\r\n");

    printf(L"Press any key to return to the main menu...\r\n");
    get_key();

    return EFI_SUCCESS;
}

// =========================================================
// Read ESP Files Function
// =========================================================
EFI_STATUS read_esp_files(EFI_HANDLE image) {
    EFI_GUID lip_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *lip = NULL;
    EFI_STATUS status = EFI_SUCCESS;

    // === Open Loaded Image Protocol ===
    status = bs->OpenProtocol(
        image,
        &lip_guid,
        (void**)&lip,
        image,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );

    if (EFI_ERROR(status)) {
        CHAR16 buffer[64];
        SPrint(buffer, sizeof(buffer), L"OpenProtocol (LIP) failed: %r\r\n", status);
        cout->OutputString(cout, buffer);
        return status;
    }

    // === Open Simple File System Protocol ===
    EFI_GUID sfsp_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfsp = NULL;

    status = bs->OpenProtocol(
        lip->DeviceHandle,
        &sfsp_guid,
        (void**)&sfsp,
        image,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );

    if (EFI_ERROR(status)) {
        CHAR16 buffer[64];
        SPrint(buffer, sizeof(buffer), L"OpenProtocol (SFSP) failed: %r\r\n", status);
        cout->OutputString(cout, buffer);

        // Cleanup: Close previously opened LIP
        bs->CloseProtocol(image, &lip_guid, image, NULL);
        return status;
    }

    // === Open root dir via OpenVolume() ===
    EFI_FILE_PROTOCOL *dirp = NULL;
    status = sfsp->OpenVolume(sfsp, &dirp);

    if (EFI_ERROR(status)) {
        CHAR16 buffer[64];
        SPrint(buffer, sizeof(buffer), L"Can't open volume for root dir: %r\r\n", status);
        cout->OutputString(cout, buffer);

        // Cleanup: Close previously opened LIP
        bs->CloseProtocol(image, &lip_guid, image, NULL);
        return status;
    }
    
    // TEMP:
    CHAR16 current_dir[256];
    strcpy_u16(current_dir, u"/");

    INT32 csr_row = 1;

    while (true) {
        clear_screen(); // Clear the screen before outputting files
        cout->OutputString(cout, L"To exit click Q/q\r\n");
        cout->OutputString(cout, L"To navigate use W/S/w/s arrows\r\n");
        cout->OutputString(cout, L"To open a file or directory press ENTER\r\n\n");

        printf(u"%s\r\n", current_dir);

        EFI_FILE_INFO file_info;
        UINTN buffer_size = sizeof(file_info);
        dirp->Read(dirp, &buffer_size, &file_info); // Prime first read

        UINTN current_row = 1;
        UINTN total_rows = 0;

        // Rewind directory to beginning before reading
        dirp->SetPosition(dirp, 0);

        // Läsa och visa filerna, räkna total_rows
        while (true) {
            buffer_size = sizeof(file_info);
            EFI_STATUS status = dirp->Read(dirp, &buffer_size, &file_info);
            if (EFI_ERROR(status) || buffer_size == 0) break;

            CHAR16 line[256];
            SPrint(line, sizeof(line), L"%s %s %s\r\n",
                (current_row == (UINTN)csr_row) ? L">" : L" ",
                (file_info.Attribute & EFI_FILE_DIRECTORY) ? L"[DIR]" : L"[FILE]",
                file_info.FileName);

            cout->OutputString(cout, line);
            current_row++;
        }
        total_rows = current_row - 1;

        // Vänta på tangenttryck
        EFI_INPUT_KEY key = get_key();

        switch (key.UnicodeChar) {
            case BIG_SCAN_ESC:
            case LOW_SCAN_ESC:
                dirp->Close(dirp);
                bs->CloseProtocol(lip->DeviceHandle, &sfsp_guid, image, NULL);
                bs->CloseProtocol(image, &lip_guid, image, NULL);
                // Gå tillbaka till huvudmeny eller avsluta funktionen
                return status; // eller anropa main_menu() om du har en sådan
            case BIG_SCAN_UP:
            case LOW_SCAN_UP:
                if (csr_row > 1) csr_row--;
                break;
            case BIG_SCAN_DOWN:
            case LOW_SCAN_DOWN:
                if (csr_row < (INT32)total_rows) csr_row++;
                break;
            default:
                if (key.UnicodeChar == u'\r') {
                    dirp->SetPosition(dirp, 0); // Reset position to start
                    UINTN buf_size;
                    INT32 i = 0;

                    do {
                        buf_size = sizeof file_info;
                        dirp->Read(dirp, &buf_size, &file_info);
                        i++;
                        
                        // OLD:
                        // while (cout->Mode->CursorRow < csr_row);
                    } while (i < csr_row);

                    if (file_info.Attribute & EFI_FILE_DIRECTORY) {
                        EFI_FILE_PROTOCOL *new_dir;

                        status = dirp->Open(dirp,
                                            &new_dir,
                                            file_info.FileName,
                                            EFI_FILE_MODE_READ,
                                            0);

                        if (EFI_ERROR(status)) {
                            printf(L"Failed to open directory: %s\r\n", file_info.FileName);
                        }

                        dirp->Close(dirp); // Close current directory
                        dirp = new_dir; // Switch to new directory
                        csr_row = 1; // Reset cursor row after opening a file/directory

                        if (!strncmp_u16(file_info.FileName, u".", 2)) {
                            // Do nothing, just stay in the same directory
                        } else if (!strncmp_u16(file_info.FileName, u"..", 3)) {
                            // Go back to parent directory
                            CHAR16 *pos = strrchr_u16(current_dir, u'/');
                            if (pos == current_dir) pos++;
                            *pos = u'\0';
                        } else {
                            // Go into nested directory
                            CHAR16 *pos = current_dir;

                            // Kontrollera att current_dir slutar med '/'
                            size_t len = 0;
                            while (pos[len] != u'\0') len++;

                            if (len == 0 || pos[len - 1] != u'/') {
                                strcat_u16(pos, u"/");
                            }

                            strcat_u16(pos, file_info.FileName);
                        }
                        continue; // Continue to read files in the new directory
                    }

                    VOID *buf = NULL;
                    buf_size = file_info.FileSize;
                    status = bs->AllocatePool(EfiLoaderData, buf_size, &buf);
                    if (EFI_ERROR(status)) {
                        cout->OutputString(cout, u"Allocate error");
                        get_key();
                    }

                    EFI_FILE_PROTOCOL *file = NULL;
                    status = dirp->Open(
                                        dirp,
                                        &file,
                                        file_info.FileName,
                                        EFI_FILE_MODE_READ,
                                        0);

                    if (EFI_ERROR(status)) {
                        cout->OutputString(cout, u"Can't open file");
                        get_key();
                    }

                    status = dirp->Read(file, &buf_size, buf);
                    if (EFI_ERROR(status)) {
                        cout->OutputString(cout, u"Read file error");
                        get_key();
                    }

                    if (buf_size != file_info.FileSize) {
                        cout->OutputString(cout, u"Buffer error");
                        get_key();
                    }

                    printf(u"File Contents:\r\n");
                    char *pos = (char *)buf;

                    for (UINTN bytes = buf_size; bytes > 0; bytes--) {
                        CHAR16 str[2];        
                        str[0] = (CHAR16)(unsigned char)(*pos);
                        str[1] = L'\0';

                        if (*pos == '\n') {
                            printfc(u"\r\n", str);
                        } else {
                            printfc(u"%s", str);
                        }

                        pos++;
                    }

                    printf(u"\r\n\r\nPress any key to continue...\r\n");

                    bs->FreePool(buf);
                    dirp->Close(file);
                    
                    get_key();
                }
                break;
        }
    }
    
    // END TEMP

    // === Cleanup ===
    dirp->Close(dirp); // Close the root directory
    bs->CloseProtocol(lip->DeviceHandle, &sfsp_guid, image, NULL); // Close Simple File System Protocol
    bs->CloseProtocol(image, &lip_guid, image, NULL); // Close Loaded Image Protocol

    return status;
}

// =========================================================
// Print Block IO
// =========================================================
EFI_STATUS print_block_io_partitions(void) {
    EFI_STATUS status = EFI_SUCCESS;

    cout->ClearScreen(cout);

    EFI_GUID bio_guid = EFI_BLOCK_IO_PROTOCOL_GUID;
    EFI_BLOCK_IO_PROTOCOL *biop;
    UINTN num_handles = 0;
    EFI_HANDLE *handle_buffer = NULL;

    // Get media ID for this disk image first, to compare to others in output
    EFI_GUID lip_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *lip = NULL;
    status = bs->OpenProtocol(image,
                              &lip_guid,
                              (VOID **)&lip,
                              image,
                              NULL,
                              EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(status)) {
        printf(u"Error %x; Could not open Loaded Image Protocol\r\n", status);
        return status;
    }

    status = bs->OpenProtocol(lip->DeviceHandle,
                              &bio_guid,
                              (VOID **)&biop,
                              image,
                              NULL,
                              EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(status)) {
        printf(u"\r\nERROR: %x; Could not open Block IO Protocol for this loaded image.\r\n", status);
        return status;
    }

    UINT32 this_image_media_id = biop->Media->MediaId;  // Media ID for this running disk image itself

    // Close open protocols when done
    bs->CloseProtocol(lip->DeviceHandle,
                      &bio_guid,
                      image,
                      NULL);
    bs->CloseProtocol(image,
                      &lip_guid,
                      image,
                      NULL);

    // Loop through and print all partition information found
    status = bs->LocateHandleBuffer(ByProtocol, &bio_guid, NULL, &num_handles, &handle_buffer);
    if (EFI_ERROR(status)) {
        printf(u"\r\nERROR: %x; Could not locate any Block IO Protocols.\r\n", status);
        return status;
    }

    UINT32 last_media_id = -1;  // Keep track of currently opened Media info
    for (UINTN i = 0; i < num_handles; i++) {
        status = bs->OpenProtocol(handle_buffer[i], 
                                  &bio_guid,
                                  (VOID **)&biop,
                                  image,
                                  NULL,
                                  EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

        if (EFI_ERROR(status)) {
            printf(u"\r\nERROR: %x; Could not Open Block IO protocol on handle %u.\r\n", status, i);
            continue;
        }

        // Print Block IO Media Info for this Disk/partition
        if (last_media_id != biop->Media->MediaId) {
            last_media_id = biop->Media->MediaId;   
            printf(u"Media ID: %u %s\r\n", 
                   last_media_id, 
                   (last_media_id == this_image_media_id ? u"(Disk Image)" : u""));
        }

        if (biop->Media->LastBlock == 0) {
            // Only really care about partitions/disks above 1 block in size
            bs->CloseProtocol(handle_buffer[i],
                              &bio_guid,
                              image,
                              NULL);
            continue;
        }

        printf(u"Rmv: %s, Pr: %s, LglPrt: %s, RdOnly: %s, Wrt$: %s\r\n"
               u"BlkSz: %u, IoAln: %u, LstBlk: %u, LwLBA: %u, LglBlkPerPhys: %u\r\n"
               u"OptTrnLenGran: %u\r\n",
               biop->Media->RemovableMedia   ? u"Y" : u"N",
               biop->Media->MediaPresent     ? u"Y" : u"N",
               biop->Media->LogicalPartition ? u"Y" : u"N",
               biop->Media->ReadOnly         ? u"Y" : u"N",
               biop->Media->WriteCaching     ? u"Y" : u"N",

               biop->Media->BlockSize,
               biop->Media->IoAlign,
               biop->Media->LastBlock,
               biop->Media->LowestAlignedLba,                   
               biop->Media->LogicalBlocksPerPhysicalBlock,     
               biop->Media->OptimalTransferLengthGranularity);

        // Print type of partition e.g. ESP or Data or Other
        if (!biop->Media->LogicalPartition) printf(u"<Entire Disk>\r\n");
        else {
            // Get partition info protocol for this partition
            EFI_GUID pi_guid = EFI_PARTITION_INFO_PROTOCOL_GUID;
            EFI_PARTITION_INFO_PROTOCOL *pip = NULL;
            status = bs->OpenProtocol(handle_buffer[i], 
                                      &pi_guid,
                                      (VOID **)&pip,
                                      image,
                                      NULL,
                                      EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

            if (EFI_ERROR(status)) {
                printf(u"\r\nERROR: %x; Could not Open Partition Info protocol on handle %u.\r\n", 
                      status, i);
            } else {
                if      (pip->Type == PARTITION_TYPE_OTHER) printf(u"<Other Type>\r\n");
                else if (pip->Type == PARTITION_TYPE_MBR)   printf(u"<MBR>\r\n");
                else if (pip->Type == PARTITION_TYPE_GPT) {
                    if (pip->System == 1) printf(u"<EFI System Partition>\r\n");
                    else {
                        // Compare Gpt.PartitionTypeGUID with known values
                        EFI_GUID data_guid = BASIC_DATA_GUID;
                        if (!memcmp(&pip->Info.Gpt.PartitionTypeGUID, &data_guid, sizeof(EFI_GUID))) 
                            printf(u"<Basic Data>\r\n");
                        else
                            printf(u"<Other GPT Type>\r\n");
                    }
                }
            }
        }

        // Close open protocol when done
        bs->CloseProtocol(handle_buffer[i],
                          &bio_guid,
                          image,
                          NULL);

        printf(u"\r\n");    // Separate each block of text visually 
    }

    printf(u"Press any key to go back..\r\n");
    get_key();
    return EFI_SUCCESS;
}

// =========================================================
// Read ESP file to buffer
// =========================================================
VOID *read_esp_file_tb(CHAR16 *path, UINTN *file_size) {
    VOID *file_buf = NULL;
    EFI_STATUS status;
    EFI_GUID lip_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *lip = NULL;
    EFI_GUID sfsp_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfsp = NULL;


    status = bs->OpenProtocol(
        image,
        &lip_guid,
        (VOID **)&lip,
        image,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );

    if (EFI_ERROR(status)) {
        printf(L"Error #01 read_esp_file_tb efi.c");
        get_key();
        goto clean;
    }

    status = bs->OpenProtocol(
        lip->DeviceHandle,
        &sfsp_guid,
        (void**)&sfsp,
        image,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );

    if (EFI_ERROR(status)) {
        CHAR16 buffer[64];
        SPrint(buffer, sizeof(buffer), L"OpenProtocol (SFSP) failed ESP: %r\r\n", status);
        cout->OutputString(cout, buffer);

        // Cleanup: Close previously opened LIP
        bs->CloseProtocol(image, &lip_guid, image, NULL);
        goto clean;
    }

    // === Open root dir via OpenVolume() ===
    EFI_FILE_PROTOCOL *root = NULL;
    status = sfsp->OpenVolume(sfsp, &root);

    if (EFI_ERROR(status)) {
        CHAR16 buffer[64];
        SPrint(buffer, sizeof(buffer), L"Can't open volume for root dir ESP: %r\r\n", status);
        cout->OutputString(cout, buffer);

        // Cleanup: Close previously opened LIP
        bs->CloseProtocol(image, &lip_guid, image, NULL);
        goto clean;
    }

    EFI_FILE_PROTOCOL *file = NULL;
    status = root->Open(root,
                        &file,
                        path,
                        EFI_FILE_MODE_READ,
                        0);

    if (EFI_ERROR(status)) {
        printf(L"Failed to open file ESP: %s\r\n", path);
        goto clean;
    }

    EFI_FILE_INFO file_info;
    EFI_GUID fi_guid = EFI_FILE_INFO_ID;
    UINTN buf_size = sizeof(EFI_FILE_INFO);
    status = file->GetInfo(file, &fi_guid, &buf_size, &file_info);

    if (EFI_ERROR(status)) {
        printf(L"Failed to open file info ESP: %s\r\n", path);
        goto file_cleanup;
    }

    if (file_info.FileSize == 0) {
        printf(L"File empty: %s\n", path);
        goto file_cleanup;
    }
    buf_size = file_info.FileSize;


    status = bs->AllocatePool(EfiLoaderData, buf_size, &file_buf);

    if (EFI_ERROR(status) || buf_size != file_info.FileSize) {
        printf(L"Failed to allocate memory: %s\r\n", path);
        goto file_cleanup;
    }

    status = file->Read(file, &buf_size, file_buf);
    if (EFI_ERROR(status) || buf_size != file_info.FileSize) {
        printf(L"Failed to read file: %s\r\n", path);
        goto file_cleanup;
    }

    *file_size = buf_size;

    file_cleanup:
    file->Close(file);
    root->Close(root);

    clean:
    bs->CloseProtocol(
        lip->DeviceHandle,
        &sfsp_guid,
        image,
        NULL
    );

    bs->CloseProtocol(
        image,
        &lip_guid,
        image,
        NULL
    );

    return file_buf;
}

void PrintBuffer(void *buf, UINTN size, BOOLEAN isChar16) {
    if (!buf || size == 0) return;

    if (isChar16) {
        // CHAR16-buffer
        CHAR16 *c16 = (CHAR16*)buf;
        for (UINTN i = 0; i < size; i++) {
            CHAR16 c = c16[i];
            if (c == L'\n') {
                printf(L"\r\n");  // ny rad
            } else {
                printf(L"%c", c); // korrekt utskrift av CHAR16
            }
        }
    } else {
        // ASCII buffer
        UINT8 *c8 = (UINT8*)buf;
        for (UINTN i = 0; i < size; i++) {
            CHAR16 c = (CHAR16)c8[i];  // konvertera byte → CHAR16
            if (c == '\n') {
                printf(L"\r\n");
            } else {
                printf(L"%c", c); // korrekt utskrift av CHAR16
            }
        }
    }
}

// Hjälpfunktioner för strängar
void PrintStr(const char *str) {
    if (!str) return;
    UINTN len = 0;
    while (str[len]) len++;
    PrintBuffer((void*)str, len, FALSE);
}

void PrintStr16(const CHAR16 *str16) {
    if (!str16) return;
    UINTN len = 0;
    while (str16[len]) len++;
    PrintBuffer((void*)str16, len, TRUE);
}

// =========================================================
// Getting a Image Media ID for this running image
// =========================================================
EFI_STATUS get_image_id(UINT32 *image_mid) {
    if (!image_mid) return EFI_INVALID_PARAMETER;

    EFI_STATUS status = EFI_SUCCESS;
    EFI_GUID bio_guid = EFI_BLOCK_IO_PROTOCOL_GUID;
    EFI_BLOCK_IO_PROTOCOL *biop = NULL;

    EFI_GUID lip_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *lip = NULL;

    status = bs->OpenProtocol(image,
                              &lip_guid,
                              (VOID **)&lip,
                              image,
                              NULL,
                              EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(status)) {
        printf(L"Error %x; Could not open Loaded Image Protocol\r\n", status);
        return status;
    }

    status = bs->OpenProtocol(lip->DeviceHandle,
                              &bio_guid,
                              (VOID **)&biop,
                              image,
                              NULL,
                              EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
    if (EFI_ERROR(status)) {
        printf(L"Error %x; Could not open Block IO Protocol\r\n", status);
        return status;
    }

    *image_mid = biop->Media->MediaId; // nu korrekt, image_mid är pekare

    bs->CloseProtocol(lip->DeviceHandle, &bio_guid, image, NULL);
    bs->CloseProtocol(image, &lip_guid, image, NULL);

    return status;
}

// =========================================================
// Read LBAs to buffer
// =========================================================
VOID *read_disk_lba_tb(EFI_LBA disk_lba, UINTN data_size, UINT32 disk_mediaID) {
    // (void)disk_lba;
    // (void)data_size;
    // (void)disk_mediaID;

    EFI_GUID bio_guid = EFI_BLOCK_IO_PROTOCOL_GUID;
    VOID *buf = NULL;
    EFI_STATUS status;
    EFI_BLOCK_IO_PROTOCOL *biop;
    UINTN num_handles = 0;
    EFI_HANDLE *handle_buffer = NULL;
    BOOLEAN found = false;

    // Loop through and print all partition information found
    status = bs->LocateHandleBuffer(ByProtocol, &bio_guid, NULL, &num_handles, &handle_buffer);
    if (EFI_ERROR(status)) {
        printf(u"\r\nERROR: %x; Could not locate any Block IO Protocols.\r\n", status);
        return NULL;
    }

    UINTN i = 0;
    for (i = 0; i < num_handles; i++) {
        status = bs->OpenProtocol(handle_buffer[i], 
                                  &bio_guid,
                                  (VOID **)&biop,
                                  image,
                                  NULL,
                                  EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

        if (EFI_ERROR(status)) {
            printf(u"\r\nERROR: %x; Could not Open Block IO protocol on handle %u.\r\n", status, i);
            // Close open protocol when done
            bs->CloseProtocol(handle_buffer[i],
                                &bio_guid,
                                image,
                                NULL);
            continue;
        }

        if (biop->Media->MediaId == disk_mediaID && !biop->Media->LogicalPartition) {
            found = true;
            break;
        }
    }

    if (found == false) {
        printf(L"Error: %x; Could not fins biop for disk with ID: %u", status, disk_mediaID);
        get_key();
        return buf;
    }

    EFI_GUID dio_guid = EFI_DISK_IO_PROTOCOL_GUID;
    EFI_DISK_IO_PROTOCOL *diop;

    status = bs->OpenProtocol(handle_buffer[i],
                              &dio_guid,
                              (VOID **)&diop,
                              image,
                              NULL,
                              EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );

    if (EFI_ERROR(status)) {
        printf(L"Could not open Disk IO Protocol UEFI c");

        get_key();

        // Close open Disk IO Protocol
        bs->CloseProtocol(handle_buffer[i],
                        &dio_guid,
                        image,
                        NULL);

        // Close open protocol when done
        bs->CloseProtocol(handle_buffer[i],
                        &bio_guid,
                        image,
                        NULL);

        return NULL;
    }

    status = bs->AllocatePool(EfiLoaderData, data_size, &buf);

    if (EFI_ERROR(status))  {
        printf(L"Cant allocate buffer data");
        get_key();

        // Close open protocol when done
        bs->CloseProtocol(handle_buffer[i],
                            &bio_guid,
                            image,
                            NULL);

        // Close open Disk IO Protocol
        bs->CloseProtocol(handle_buffer[i],
                            &dio_guid,
                            image,
                            NULL);

        return NULL;
    }

    // Reading data into allocated pool
    status = diop->ReadDisk(diop, disk_mediaID, disk_lba * biop->Media->BlockSize, data_size, buf);

        if (EFI_ERROR(status))  {
        printf(L"Could not read UDP data");
        get_key();

        // Close open protocol when done
        bs->CloseProtocol(handle_buffer[i],
                            &bio_guid,
                            image,
                            NULL);

        // Close open Disk IO Protocol
        bs->CloseProtocol(handle_buffer[i],
                            &dio_guid,
                            image,
                            NULL);

        return NULL;
    }

    // Close open protocol when done
    bs->CloseProtocol(handle_buffer[i],
                        &bio_guid,
                        image,
                        NULL);

    // Close open Disk IO Protocol
    bs->CloseProtocol(handle_buffer[i],
                        &dio_guid,
                        image,
                        NULL);

    return buf;
}

int is_valid_utf8(const unsigned char *buf, size_t len) {
    size_t i = 0;
    while (i < len) {
        if (buf[i] <= 0x7F) {
            i++;
        } else if ((buf[i] & 0xE0) == 0xC0) {
            if (i+1 >= len || (buf[i+1] & 0xC0) != 0x80) return 0;
            i += 2;
        } else if ((buf[i] & 0xF0) == 0xE0) {
            if (i+2 >= len || (buf[i+1] & 0xC0) != 0x80 || (buf[i+2] & 0xC0) != 0x80) return 0;
            i += 3;
        } else if ((buf[i] & 0xF8) == 0xF0) {
            if (i+3 >= len || (buf[i+1] & 0xC0) != 0x80 || (buf[i+2] & 0xC0) != 0x80 || (buf[i+3] & 0xC0) != 0x80) return 0;
            i += 4;
        } else {
            return 0; // ogiltig UTF-8
        }
    }
    return 1; // giltig UTF-8
}

int is_ascii(const unsigned char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (buf[i] > 127)
            return 0; // ej ASCII
    }
    return 1; // ASCII
}

// =========================================================
// Read UDP File
// =========================================================
EFI_STATUS read_udp_file(const char* target_name) {
    clear_screen();
    EFI_STATUS status = EFI_SUCCESS;

    CHAR16 *file_name = L"\\EFI\\BOOT\\FILE.TXT";
    UINTN buf_size = 0;
    VOID *file_buf = read_esp_file_tb(file_name, &buf_size);
    if (!file_buf) {
        printf(L"Could not find or read FILE.TXT in ESP\r\n");
        get_key();
        return EFI_NOT_FOUND;
    }

    // Iterera över alla FILE_NAME poster
    char *pos = (char*)file_buf;
    while ((pos = strstr(pos, "FILE_NAME=")) != NULL) {
        pos += strlen("FILE_NAME=");
        char filename[64] = {0};
        sscanf(pos, "%63s", filename);

        // Hitta FILE_SIZE
        char *size_pos = strstr(pos, "FILE_SIZE=");
        UINTN file_size = 0;
        if (size_pos) {
            size_pos += strlen("FILE_SIZE=");
            while (is_digit(*size_pos)) {
                file_size = file_size * 10 + (*size_pos - '0');
                size_pos++;
            }
        }

        // Hitta DISK_LBA
        char *lba_pos = strstr(pos, "DISK_LBA=");
        UINTN disk_lba = 0;
        if (lba_pos) {
            lba_pos += strlen("DISK_LBA=");
            while (is_digit(*lba_pos)) {
                disk_lba = disk_lba * 10 + (*lba_pos - '0');
                lba_pos++;
            }
        }

        if (strcmp(filename, target_name) == 0) {
            UINT32 image_id = 0;
            status = get_image_id(&image_id);
            if (EFI_ERROR(status)) {
                printf(L"Could not get Image Media ID\r\n");
                break;
            }

            VOID *disk_buf = read_disk_lba_tb(disk_lba, file_size, image_id);
            if (!disk_buf) {
                printf(L"Could not read %a from disk\r\n", filename);
                break;
            }

            printf(L"Contents of %a:\r\n\n", filename);
            for (UINTN i = 0; i < file_size; i++) {
                Print(L"%c", ((char*)disk_buf)[i]);
            }

            Print(L"\r\n");
            bs->FreePool(disk_buf);
            break;
        }

        pos = lba_pos; // fortsätt till nästa fil
    }

    bs->FreePool(file_buf);
    printf(L"\r\nPress any key to continue\r\n");
    get_key();
    return EFI_SUCCESS;
}

void error(EFI_STATUS status, const CHAR16 *msg) {
    if (status != 0) {
        printf(L"Error: %x: %s\r\n", status, msg);
    } else {
        printf(L"%s\r\n", msg);
    }
    get_key();  // Vänta på tangenttryck
}

// ==========================
// Get Memory Map from UEFI
// ==========================
EFI_STATUS get_memory_map(Memory_Map_Info *mmap) { 
    memset(mmap, 0, sizeof *mmap);  // Ensure input parm is initialized

    // Get initial memory map size (send 0 for map size)
    EFI_STATUS status = EFI_SUCCESS;
    status = bs->GetMemoryMap(&mmap->size,
                              mmap->map,
                              &mmap->key,
                              &mmap->desc_size,
                              &mmap->desc_version);

    if (EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL) {
        error(0, u"Could not get initial memory map size.\r\n");
        return status;
    }

    // Allocate buffer for actual memory map for size in mmap->size;
    //   need to allocate enough space for an additional memory descriptor or 2 in the map due to
    //   this allocation itself.
    mmap->size += mmap->desc_size * 2;  
    status = bs->AllocatePool(EfiLoaderData, mmap->size,(VOID **)&mmap->map);
    if (EFI_ERROR(status)) {
        error(status, u"Could not allocate buffer for memory map '%s'\r\n");
        return status;
    }

    // Call get memory map again to get the actual memory map now that the buffer is the correct
    //   size
    status = bs->GetMemoryMap(&mmap->size,
                              mmap->map,
                              &mmap->key,
                              &mmap->desc_size,
                              &mmap->desc_version);
    if (EFI_ERROR(status)) {
        error(status, u"Could not get UEFI memory map! :(\r\n");
        return status;
    }

    return EFI_SUCCESS;
}
// ====================
// Print Memory Map
// ====================
EFI_STATUS print_memory_map(void) { 
    cout->ClearScreen(cout);

    Memory_Map_Info mmap = {0};
    get_memory_map(&mmap);

    // Print memory map descriptor values
    printf(u"Memory map: Size %u, Descriptor size: %u, # of descriptors: %u, key: %x\r\n",
            mmap.size, mmap.desc_size, mmap.size / mmap.desc_size, mmap.key);

    UINTN usable_bytes = 0; // "Usable" memory for an OS or similar, not firmware/device reserved
    for (UINTN i = 0; i < mmap.size / mmap.desc_size; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = 
            (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)mmap.map + (i * mmap.desc_size));

        printf(u"%u: Typ: %u, Phy: %x, Vrt: %x, Pgs: %u, Att: %x\r\n",
                i,
                desc->Type, 
                desc->PhysicalStart, 
                desc->VirtualStart, 
                desc->NumberOfPages, 
                desc->Attribute);

        // Add to usable memory count depending on type
        if (desc->Type == EfiLoaderCode         || 
            desc->Type == EfiLoaderData         || 
            desc->Type == EfiBootServicesCode   || 
            desc->Type == EfiBootServicesData   || 
            desc->Type == EfiConventionalMemory || 
            desc->Type == EfiPersistentMemory) {

            usable_bytes += desc->NumberOfPages * 4096;
        }

        // Pause if reached bottom of screen
        if (cout->Mode->CursorRow >= 23) {
            printf(u"Press any key to continue...\r\n");
            get_key();
            cout->ClearScreen(cout);
        }
    }

    printf(u"\r\nUsable memory: %u / %u MiB / %u GiB\r\n",
            usable_bytes, usable_bytes / (1024 * 1024), usable_bytes / (1024 * 1024 * 1024));

    // Free allocated buffer for memory map
    bs->FreePool(mmap.map);

    printf(u"\r\nPress any key to go back...\r\n");
    get_key();
    return EFI_SUCCESS;
}

// =========================================================
// Load and run KERNEL.BIN from UDP
// =========================================================
EFI_STATUS load_kernel(void) {
    clear_screen();
    EFI_STATUS status = EFI_SUCCESS;

    // Läs FILE.TXT från ESP
    CHAR16 *file_name = L"\\EFI\\BOOT\\FILE.TXT";
    UINTN buf_size = 0;
    VOID *file_buf = read_esp_file_tb(file_name, &buf_size);
    if (!file_buf) {
        printf(L"Could not find or read FILE.TXT in ESP\r\n");
        get_key();
        return EFI_NOT_FOUND;
    }

    // Leta efter KERNEL.BIN
    char *pos = (char*)file_buf;
    while ((pos = strstr(pos, "FILE_NAME=")) != NULL) {
        pos += strlen("FILE_NAME=");
        char filename[64] = {0};
        sscanf(pos, "%63s", filename);

        if (strcmp(filename, "KERNEL.BIN") == 0) {
            // Läs storlek
            char *size_pos = strstr(pos, "FILE_SIZE=");
            UINTN file_size = 0;
            if (size_pos) {
                size_pos += strlen("FILE_SIZE=");
                while (is_digit(*size_pos)) {
                    file_size = file_size * 10 + (*size_pos - '0');
                    size_pos++;
                }
            }

            // Läs LBA
            char *lba_pos = strstr(pos, "DISK_LBA=");
            UINTN disk_lba = 0;
            if (lba_pos) {
                lba_pos += strlen("DISK_LBA=");
                while (is_digit(*lba_pos)) {
                    disk_lba = disk_lba * 10 + (*lba_pos - '0');
                    lba_pos++;
                }
            }

            // Hämta Image ID
            UINT32 image_id = 0;
            status = get_image_id(&image_id);
            if (EFI_ERROR(status)) {
                printf(L"Could not get Image Media ID\r\n");
                break;
            }

            // Läs kernel från disk
            VOID *kernel_buf = read_disk_lba_tb(disk_lba, file_size, image_id);
            if (!kernel_buf) {
                printf(L"Could not read KERNEL.BIN from disk\r\n");
                break;
            }

            // Lokalisera GOP
            EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
            EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
            status = bs->LocateProtocol(&gop_guid, NULL, (VOID**)&gop);
            if (EFI_ERROR(status)) {
                printf(L"Could not locate GOP! Status: %x\r\n", status);
                bs->FreePool(kernel_buf);
                get_key();
                return status;
            }

            // Ladda kernel till 0x100000
            EFI_PHYSICAL_ADDRESS addr = 0x100000;
            CopyMem((VOID*)(UINTN)addr, kernel_buf, file_size);

            // Debug info innan ExitBootServices
            printf(L"KERNEL.BIN loaded at 0x100000 (%u bytes)\r\n", file_size);
            printf(L"File Size: %u bytes, Disk LBA: %u\r\n", file_size, disk_lba);
            printf(L"Image ID: %u\r\n", image_id);
            printf(L"Header Bytes: [0x%04x] [0x%04x] [0x%04x] [0x%04x]\r\n\n",
                   ((UINT8*)addr)[0], ((UINT8*)addr)[1], ((UINT8*)addr)[2], ((UINT8*)addr)[3]);

            // Setup kp struct för kernel
            kp ctx = {0};
            ctx.ST = gST;
            ctx.gop = gop;

            // Debug innan exit
            printf(L"Press any key to jump to kernel\r\n");
            get_key();

            if (EFI_ERROR(get_memory_map(&ctx.mmap))) {
                bs->FreePool(kernel_buf);
                bs->FreePool(file_buf);
            }

            // Hoppa till kernel
            typedef void (*kernel_entry_t)(kp*);
            kernel_entry_t kernel_entry = (kernel_entry_t)(UINTN)addr;
            kernel_entry(&ctx);

            break;
        }
    }

    bs->FreePool(file_buf);
    return status;
}

// =========================================================
// Show UDP Menu
// =========================================================
void ShowUDPmenu(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *cout, UINTN selected) {
    clear_screen();
    cout->OutputString(cout, (CHAR16 *)L"Use W/S to navigate, ENTER to confirm.\r\n\r\n");

    const CHAR16* options[] = {
        L"README",
        L"KERNEL.BIN",
        L"Exit to Main Menu"
    };

    const UINTN optionCount = sizeof(options) / sizeof(options[0]);

    for (UINTN i = 0; i < optionCount; i++) {
        if (i == selected) {
            cout->SetAttribute(cout, EFI_TEXT_ATTR(EFI_BLACK, EFI_WHITE));
            cout->OutputString(cout, (CHAR16 *)L"> ");
            cout->OutputString(cout, (CHAR16 *)options[i]);
            cout->OutputString(cout, (CHAR16 *)L"\r\n");
            cout->SetAttribute(cout, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLACK));
        } else {
            cout->OutputString(cout, (CHAR16 *)L"  ");
            cout->OutputString(cout, (CHAR16 *)options[i]);
            cout->OutputString(cout, (CHAR16 *)L"\r\n");
        }
    }
}

// =========================================================
// UDP Main Menu
// =========================================================
EFI_STATUS EFIAPI udp_main(void) {
    gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

    UINTN selected = 1;
    EFI_INPUT_KEY key;
    const UINTN optionCount = 3;

    ShowUDPmenu(cout, selected);

    while (1) {
        if (cin->ReadKeyStroke(cin, &key) == EFI_SUCCESS) {
            if (key.UnicodeChar == L'w' || key.UnicodeChar == L'W') {
                selected = (selected == 0) ? optionCount - 1 : selected - 1;
                ShowUDPmenu(cout, selected);
            } else if (key.UnicodeChar == L's' || key.UnicodeChar == L'S') {
                selected = (selected + 1) % optionCount;
                ShowUDPmenu(cout, selected);
            } else if (key.UnicodeChar == L'\r' || key.UnicodeChar == 0x0D) { // Enter
                switch (selected) {
                    case 0:
                        read_udp_file("README");
                        ShowUDPmenu(cout, selected);
                        break;
                    case 1:
                        read_udp_file("KERNEL.BIN");
                        ShowUDPmenu(cout, selected);
                        break;
                    case 2:
                        ShowMenu(cout, 0);
                        return EFI_SUCCESS;
                }
            }
        }
    }

    return EFI_SUCCESS; // Kommer aldrig nås, men krävs
}

// =========================================================
// Mouse test
// =========================================================
EFI_STATUS test_mouse(void) {
    // Get SPP protocol via LocateHandleBuffer()
    EFI_GUID spp_guid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
    EFI_SIMPLE_POINTER_PROTOCOL *spp[5];
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL save_buffer[CURSOR_SIZE * CURSOR_SIZE];
    UINTN num_handles = 0;
    EFI_HANDLE *handle_buffer = NULL;
    EFI_STATUS status = 0;
    INTN cursor_size = 8;               // Size in pixels    
    INTN cursor_x = 0, cursor_y = 0;    // Mouse cursor position

    // Get APP protocol via LocateHandleBuffer()
    EFI_GUID app_guid = EFI_ABSOLUTE_POINTER_PROTOCOL_GUID;
    EFI_ABSOLUTE_POINTER_PROTOCOL *app[5];

    typedef enum {
        CIN = 0,    // ConIn (keyboard)
        SPP = 1,    // Simple Pointer Protocol (mouse/touchpad)
        APP = 2,    // Absolute Pointer Protocol (touchscreen/digitizer)
    } INPUT_TYPE;

    typedef struct {
        EFI_EVENT wait_event;   // This will be used in WaitForEvent()
        INPUT_TYPE type;
        union {
            EFI_SIMPLE_POINTER_PROTOCOL   *spp;
            EFI_ABSOLUTE_POINTER_PROTOCOL *app;
        };
    } INPUT_PROTOCOL;

    INPUT_PROTOCOL input_protocols[11]; // 11 = Max of 5 spp + 5 app + 1 conin
    UINTN num_protocols = 0;

    // First input will be ConIn
    input_protocols[num_protocols++] = (INPUT_PROTOCOL){ 
        .wait_event = cin->WaitForKey,
        .type = CIN,
        .spp = NULL,
    };

    // Get GOP protocol via LocateProtocol()
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID; 
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info = NULL;
    UINTN mode_info_size = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    UINTN mode_index = 0;   // Current mode within entire menu of GOP mode choices;

    status = bs->LocateProtocol(&gop_guid, NULL, (VOID **)&gop);
    if (EFI_ERROR(status)) {
        printf(u"\r\nERROR: %x; Could not locate GOP! :(\r\n", status);
        get_key();
        return status;
    }

    gop->QueryMode(gop, mode_index, &mode_info_size, &mode_info);

    // Use LocateHandleBuffer() to find all SPPs 
    status = bs->LocateHandleBuffer(ByProtocol, &spp_guid, NULL, &num_handles, &handle_buffer);
    if (EFI_ERROR(status)) {
        printf(u"\r\nERROR: %x; Could not locate Simple Pointer Protocol handle buffer.\r\n", status);
        get_key();
    }

    cout->ClearScreen(cout);

    BOOLEAN found_mode = FALSE;

    // Open all SPP protocols for each handle
    for (UINTN i = 0; i < num_handles; i++) {
        status = bs->OpenProtocol(handle_buffer[i], 
                                  &spp_guid,
                                  (VOID **)&spp[i],
                                  image,
                                  NULL,
                                  EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

        if (EFI_ERROR(status)) {
            printf(u"\r\nERROR: %x; Could not Open Simple Pointer Protocol on handle.\r\n", status);
            get_key();
            continue;
        }

        // Reset device
        spp[i]->Reset(spp[i], TRUE);

        // Print initial SPP mode info
        printf(u"SPP %u; Resolution X: %u, Y: %u, Z: %u, LButton: %u, RButton: %u\r\n",
               i,
               spp[i]->Mode->ResolutionX, 
               spp[i]->Mode->ResolutionY,
               spp[i]->Mode->ResolutionZ,
               spp[i]->Mode->LeftButton, 
               spp[i]->Mode->RightButton);

        if (spp[i]->Mode->ResolutionX < 65536) {
            found_mode = TRUE;
            // Add valid protocol to array
            input_protocols[num_protocols++] = (INPUT_PROTOCOL){ 
                .wait_event = spp[i]->WaitForInput,
                .type = SPP,
                .spp = spp[i]
            };
        }
    }
    
    if (!found_mode) {
        printf(u"\r\nERROR: Could not find any valid SPP Mode.\r\n");
        get_key();
    }

    // Free memory pool allocated by LocateHandleBuffer()
    bs->FreePool(handle_buffer);

    // Use LocateHandleBuffer() to find all APPs 
    num_handles = 0;
    handle_buffer = NULL;
    found_mode = FALSE;

    status = bs->LocateHandleBuffer(ByProtocol, &app_guid, NULL, &num_handles, &handle_buffer);
    if (EFI_ERROR(status)) {
        printf(u"\r\nERROR: %x; Could not locate Absolute Pointer Protocol handle buffer.\r\n", status); 
        get_key();
    }

    printf(u"\r\n");    // Separate SPP and APP info visually

    // Open all APP protocols for each handle
    for (UINTN i = 0; i < num_handles; i++) {
        status = bs->OpenProtocol(handle_buffer[i], 
                                  &app_guid,
                                  (VOID **)&app[i],
                                  image,
                                  NULL,
                                  EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

        if (EFI_ERROR(status)) {
            printf(u"\r\nERROR: %x; Could not Open Simple Pointer Protocol on handle.\r\n", status);
            continue;
        }

        // Reset device
        app[i]->Reset(app[i], TRUE);

        // Print initial APP mode info
        printf(u"APP %u; Min X: %u, Y: %u, Z: %u, Max X: %u, Y: %u, Z: %u, Attributes: %b\r\n",
               i,
               app[i]->Mode->AbsoluteMinX, 
               app[i]->Mode->AbsoluteMinY,
               app[i]->Mode->AbsoluteMinZ,
               app[i]->Mode->AbsoluteMaxX, 
               app[i]->Mode->AbsoluteMaxY,
               app[i]->Mode->AbsoluteMaxZ,
               app[i]->Mode->Attributes);

        if (app[i]->Mode->AbsoluteMaxX < 65536) {
            found_mode = TRUE;
            // Add valid protocol to array
            input_protocols[num_protocols++] = (INPUT_PROTOCOL){ 
                .wait_event = app[i]->WaitForInput,
                .type = APP,
                .app = app[i]
            };
        }
    }
    
    if (!found_mode) {
        printf(u"\r\nERROR: Could not find any valid APP Mode.\r\n");
        get_key();
    }

    if (num_protocols == 0) {
        printf(u"\r\nERROR: Could not find any Simple or Absolute Pointer Protocols.\r\n");
        get_key();
        return 1;
    }

    // Found valid SPP mode, get mouse input
    // Start off in middle of screen
    INT32 xres = mode_info->HorizontalResolution, yres = mode_info->VerticalResolution;
    cursor_x = (xres / 2) - (cursor_size / 2);
    cursor_y = (yres / 2) - (cursor_size / 2);

    // Print initial mouse state & draw initial cursor
    printf(u"\r\nMouse Xpos: %d, Ypos: %d, Xmm: %d, Ymm: %d, LB: %u, RB: %u\r",
           cursor_x, cursor_y, 0, 0, 0);

    // Draw mouse cursor, and also save underlying FB data first
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *fb = 
        (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)gop->Mode->FrameBufferBase;

    for (INTN y = 0; y < cursor_size; y++) {
        for (INTN x = 0; x < cursor_size; x++) {
            save_buffer[(y * cursor_size) + x] = 
                fb[(mode_info->PixelsPerScanLine * (cursor_y + y)) + (cursor_x + x)];

            EFI_GRAPHICS_OUTPUT_BLT_PIXEL csr_px = cursor_buffer[(y * cursor_size) + x];
            fb[(mode_info->PixelsPerScanLine * (cursor_y + y)) + (cursor_x + x)] = csr_px;
        }
    }

    // Input loop
    // Fill out event queue first
    EFI_EVENT events[11]; // Same max # of elems as input_protocols
    for (UINTN i = 0; i < num_protocols; i++) events[i] = input_protocols[i].wait_event;

    while (TRUE) {
        UINTN index = 0;

        bs->WaitForEvent(num_protocols, events, &index);
        if (input_protocols[index].type == CIN) {
            // Keypress
            EFI_INPUT_KEY key;
            cin->ReadKeyStroke(cin, &key);

            if (key.ScanCode == SCANCODE_ESC) {
                // ESC Key, leave and go back to main menu
                break;
            }

        } else if (input_protocols[index].type == SPP) {
            // Simple Pointer Protocol; Mouse event
            // Get mouse state
            EFI_SIMPLE_POINTER_STATE state;
            EFI_SIMPLE_POINTER_PROTOCOL *active_spp = input_protocols[index].spp;
            active_spp->GetState(active_spp, &state);

            // Print current info
            // Movement is spp state's RelativeMovement / spp mode's Resolution
            //   movement amount is in mm; 1mm = 2% of horizontal or vertical resolution
            float xmm_float = (float)state.RelativeMovementX / (float)active_spp->Mode->ResolutionX;
            float ymm_float = (float)state.RelativeMovementY / (float)active_spp->Mode->ResolutionY;

            // If moved a tiny bit, show that on screen for a small minimum amount
            if (state.RelativeMovementX > 0 && xmm_float == 0.0) xmm_float = 1.0;
            if (state.RelativeMovementY > 0 && ymm_float == 0.0) ymm_float = 1.0;

            // Erase text first before reprinting
            printf(u"                                                                      \r");
            printf(u"Mouse Xpos: %d, Ypos: %d, Xmm: %d, Ymm: %d, LB: %u, RB: %u\r",
                  cursor_x, cursor_y, (INTN)xmm_float, (INTN)ymm_float, 
                  state.LeftButton, state.RightButton);

            // Draw cursor: Get pixel amount to move per mm
            float xres_mm_px = mode_info->HorizontalResolution * 0.02;
            float yres_mm_px = mode_info->VerticalResolution   * 0.02;

            // Save framebuffer data at mouse position first, then redraw that data
            //   instead of just overwriting with background color e.g. with a blt buffer and
            //   EfiVideoToBltBuffer and EfiBltBufferToVideo
            for (INTN y = 0; y < cursor_size; y++) {
                for (INTN x = 0; x < cursor_size; x++) {
                    fb[(mode_info->PixelsPerScanLine * (cursor_y + y)) + (cursor_x + x)] = 
                        save_buffer[(y * cursor_size) + x];
                }
            }

            cursor_x += (INTN)(xres_mm_px * xmm_float);
            cursor_y += (INTN)(yres_mm_px * ymm_float);

            // Keep cursor in screen bounds
            if (cursor_x < 0) cursor_x = 0;
            if (cursor_x > xres - cursor_size) cursor_x = xres - cursor_size;
            if (cursor_y < 0) cursor_y = 0;
            if (cursor_y > yres - cursor_size) cursor_y = yres - cursor_size;

            // Save FB data at new cursor position before drawing over it
            for (INTN y = 0; y < cursor_size; y++) {
                for (INTN x = 0; x < cursor_size; x++) {
                    save_buffer[(y * cursor_size) + x] = 
                        fb[(mode_info->PixelsPerScanLine * (cursor_y + y)) + (cursor_x + x)];

                    EFI_GRAPHICS_OUTPUT_BLT_PIXEL csr_px = cursor_buffer[(y * cursor_size) + x];
                    fb[(mode_info->PixelsPerScanLine * (cursor_y + y)) + (cursor_x + x)] = csr_px;
                }
            }

        } else if (input_protocols[index].type == APP) {
            // Handle absolute pointer protocol
            // Get state
            EFI_ABSOLUTE_POINTER_STATE state;
            EFI_ABSOLUTE_POINTER_PROTOCOL *active_app = input_protocols[index].app;
            active_app->GetState(active_app, &state);

            // Print state values
            // Erase text first before reprinting
            printf(u"                                                                      \r");
            printf(u"Ptr Xpos: %u, Ypos: %u, Zpos: %u, Buttons: %b\r",
                  state.CurrentX, state.CurrentY, state.CurrentZ,
                  state.ActiveButtons);

            // Save framebuffer data at mouse position first, then redraw that data
            //   instead of just overwriting with background color e.g. with a blt buffer and
            //   EfiVideoToBltBuffer and EfiBltBufferToVideo
            for (INTN y = 0; y < cursor_size; y++) {
                for (INTN x = 0; x < cursor_size; x++) {
                    fb[(mode_info->PixelsPerScanLine * (cursor_y + y)) + (cursor_x + x)] = 
                        save_buffer[(y * cursor_size) + x];
                }
            }

            // Get ratio of GOP screen resolution to APP max values, to translate the APP
            //   position to the correct on screen GOP position
            float x_app_ratio = (float)mode_info->HorizontalResolution / 
                                (float)active_app->Mode->AbsoluteMaxX;

            float y_app_ratio = (float)mode_info->VerticalResolution / 
                                (float)active_app->Mode->AbsoluteMaxY;

            cursor_x = (INTN)((float)state.CurrentX * x_app_ratio);
            cursor_y = (INTN)((float)state.CurrentY * y_app_ratio);

            // Keep cursor in screen bounds
            if (cursor_x < 0) cursor_x = 0;
            if (cursor_x > xres - cursor_size) cursor_x = xres - cursor_size;
            if (cursor_y < 0) cursor_y = 0;
            if (cursor_y > yres - cursor_size) cursor_y = yres - cursor_size;

            // Save FB data at new cursor position before drawing over it
            for (INTN y = 0; y < cursor_size; y++) {
                for (INTN x = 0; x < cursor_size; x++) {
                    save_buffer[(y * cursor_size) + x] = 
                        fb[(mode_info->PixelsPerScanLine * (cursor_y + y)) + (cursor_x + x)];

                    EFI_GRAPHICS_OUTPUT_BLT_PIXEL csr_px = cursor_buffer[(y * cursor_size) + x];
                    fb[(mode_info->PixelsPerScanLine * (cursor_y + y)) + (cursor_x + x)] = csr_px;
                }
            }
        }
    }

    return EFI_SUCCESS;
}

// =========================================================
// Help funtion for reading UEFI settings variables
// =========================================================
void FormatBootVarName(UINT16 BootOptionNumber, CHAR16* buffer, UINTN bufferSize)
{
    if (bufferSize < 9) return; // "Boot" + 4 tecken + null = 9

    buffer[0] = L'B';
    buffer[1] = L'o';
    buffer[2] = L'o';
    buffer[3] = L't';

    static const CHAR16 hexchars[] = L"0123456789ABCDEF";

    for (int i = 0; i < 4; i++) {
        buffer[7 - i] = hexchars[BootOptionNumber & 0xF];
        BootOptionNumber >>= 4;
    }

    buffer[8] = L'\0';
}

// =========================================================
// Help funtion for reading UEFI settings variables
// =========================================================
EFI_STATUS PrintBootOptionName(UINT16 BootOptionNumber)
{
    CHAR16 VarName[9];
    FormatBootVarName(BootOptionNumber, VarName, sizeof(VarName) / sizeof(CHAR16));

    UINT8 Data[1024];
    UINTN DataSize = sizeof(Data);
    UINT32 Attributes;
    EFI_STATUS Status;

    Status = rs->GetVariable(
        VarName,
        &EFI_GLOBAL_VARIABLE,
        &Attributes,
        &DataSize,
        Data
    );

    if (EFI_ERROR(Status)) {
        printf(L"Failed to get variable %s: 0x%llx\n", VarName, (unsigned long long)Status);
        return Status;
    }

    UINT16 FilePathListLength = *(UINT16 *)(Data + 4);
    CHAR16 *Description = (CHAR16 *)(Data + 6 + FilePathListLength);

    printf(L"BootOption 0x%04x Name: %s\n", BootOptionNumber, Description);

    return EFI_SUCCESS;
}

// =========================================================
// EFI reboot (cold)
// =========================================================
void reboot(void) {
    cout->OutputString(cout, L"\r\nRebooting...\r\n");
    rs->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
}

// =========================================================
// EFI shutdown
// =========================================================
void shutdown(void) {
    cout->OutputString(cout, L"\r\nShutting down...\r\n");
    rs->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);
}

// =========================================================
// Reading UEFI settings variables
// =========================================================
EFI_STATUS UEFI_Boot_Order()
{
    clear_screen();

    UINT8 Data[1024];
    UINTN DataSize = sizeof(Data);
    UINT32 Attributes;
    EFI_STATUS Status;

    Status = rs->GetVariable(
        L"BootOrder",
        &EFI_GLOBAL_VARIABLE,
        &Attributes,
        &DataSize,
        Data
    );

    if (EFI_ERROR(Status)) {
        printf(L"Failed to get BootOrder variable: %d\n", Status);
        return Status;
    }

    UINT16* BootOrder = (UINT16*)Data;
    UINTN Count = DataSize / sizeof(UINT16);

    printf(L"BootOrder (%d entries):\n", (int)Count);
    for (UINTN i = 0; i < Count; i++) {
        // Skriv ut hex-värdet
        printf(L"  BootOption 0x%04x\n", BootOrder[i]);

        // Skriv ut namnet
        PrintBootOptionName(BootOrder[i]);
    }

    printf(u"\r\n\r\nPress any key to continue...\r\n");
    get_key();

    return EFI_SUCCESS;
}

// =========================================================
// Time Function
// =========================================================
EFI_STATUS Time(VOID) {
    EFI_STATUS Status;
    EFI_INPUT_KEY Key;

    printf(L"Press Enter to update time and q/Q to exit.\n");

    while (TRUE) {
        clear_screen();           // Clear the screen before printing time
        printf(L"Press Enter to update time and q/Q to exit.\n");
        print_current_time();     // Print current time and date

        // Wait until a key is pressed
        do {
            Status = cin->ReadKeyStroke(cin, &Key);
        } while (Status == EFI_NOT_READY);

        if (Key.UnicodeChar == BIG_SCAN_ESC || Key.UnicodeChar == LOW_SCAN_ESC) {
            // Exit the function if 'q' or 'Q' is pressed
            return EFI_SUCCESS;
        } else if (Key.UnicodeChar == SCAN_ENTER) {
            // If Enter is pressed, continue the loop and update the time display
            continue;
        }
        // For other keys, ignore and wait for the next key press
    }

    return EFI_SUCCESS; // Will never reach here, added for completeness
}

// ====================
// Set Text Mode
// ====================
EFI_STATUS set_text_mode(void) {
    // Store found Text mode info
    typedef struct {
        UINTN cols;
        UINTN rows;
    } Text_Mode_Info;

    Text_Mode_Info text_modes[20];

    UINTN mode_index = 0;   // Current mode within entire menu of GOP mode choices;

    // Overall screen loop
    while (true) {
        cout->ClearScreen(cout);

        // Write String
        cout->OutputString(cout, u"Text mode information:\r\n");
        UINTN max_cols = 0, max_rows = 0;

        // Get current text mode's column and row counts
        cout->QueryMode(cout, cout->Mode->Mode, &max_cols, &max_rows);

        printf(u"Max Mode: %d\r\n"
               u"Current Mode: %d\r\n"
               u"Attribute: %x\r\n" 
               u"CursorColumn: %d\r\n"
               u"CursorRow: %d\r\n"
               u"CursorVisible: %d\r\n"
               u"Columns: %d\r\n"
               u"Rows: %d\r\n\r\n",
               cout->Mode->MaxMode,
               cout->Mode->Mode,
               cout->Mode->Attribute,
               cout->Mode->CursorColumn,
               cout->Mode->CursorRow,
               cout->Mode->CursorVisible,
               max_cols,
               max_rows);

        cout->OutputString(cout, u"Available text modes:\r\n");

        UINTN menu_top = cout->Mode->CursorRow, menu_bottom = max_rows;

        // Print keybinds at bottom of screen
        cout->SetCursorPosition(cout, 0, menu_bottom-3);
        printf(u"Up/Down Arrow = Move Cursor\r\n"
               u"Enter = Select\r\n"
               u"Escape = Go Back");

        cout->SetCursorPosition(cout, 0, menu_top);
        menu_bottom -= 5;   // Bottom of menu will be 2 rows above keybinds
        UINTN menu_len = menu_bottom - menu_top;

        // Get all available Text modes' info
        const UINT32 max = cout->Mode->MaxMode;
        if (max < menu_len) {
            // Bound menu by actual # of available modes
            menu_bottom = menu_top + max-1;
            menu_len = menu_bottom - menu_top;  // Limit # of modes in menu to max mode - 1
        }

        for (UINT32 i = 0; i < ARRAY_SIZE(text_modes) && i < max; i++) 
            cout->QueryMode(cout, i, &text_modes[i].cols, &text_modes[i].rows);

        // Highlight top menu row to start off
        cout->SetAttribute(cout, EFI_TEXT_ATTR(HIGHLIGHT_FG_COLOR, HIGHLIGHT_BG_COLOR));
        printf(u"Mode %d: %dx%d", 0, text_modes[0].cols, text_modes[0].rows);

        // Print other text mode infos
        cout->SetAttribute(cout, EFI_TEXT_ATTR(DEFAULT_FG_COLOR, DEFAULT_BG_COLOR));
        for (UINT32 i = 1; i < menu_len + 1; i++) 
            printf(u"\r\nMode %d: %dx%d", i, text_modes[i].cols, text_modes[i].rows);

        // Get input from user
        cout->SetCursorPosition(cout, 0, menu_top);
        bool getting_input = true;
        while (getting_input) {
            UINTN current_row = cout->Mode->CursorRow;

            EFI_INPUT_KEY key = get_key();
            switch (key.ScanCode) {
                case SCANCODE_ESC: return EFI_SUCCESS;  // ESC Key: Go back to main menu

                case SCANCODE_UP_ARROW:
                    if (current_row == menu_top && mode_index > 0) {
                        // Scroll menu up by decrementing all modes by 1
                        printf(u"                    \r");  // Blank out mode text first

                        cout->SetAttribute(cout, EFI_TEXT_ATTR(HIGHLIGHT_FG_COLOR, HIGHLIGHT_BG_COLOR));
                        mode_index--;
                        printf(u"Mode %d: %dx%d", 
                               mode_index, text_modes[mode_index].cols, text_modes[mode_index].rows);

                        cout->SetAttribute(cout, EFI_TEXT_ATTR(DEFAULT_FG_COLOR, DEFAULT_BG_COLOR));
                        UINTN temp_mode = mode_index + 1;
                        for (UINT32 i = 0; i < menu_len; i++, temp_mode++) {
                            printf(u"\r\n                    \r"  // Blank out mode text first
                                   u"Mode %d: %dx%d\r", 
                                   temp_mode, text_modes[temp_mode].cols, text_modes[temp_mode].rows);
                        }

                        // Reset cursor to top of menu
                        cout->SetCursorPosition(cout, 0, menu_top);

                    } else if (current_row-1 >= menu_top) {
                        // De-highlight current row, move up 1 row, highlight new row
                        printf(u"                    \r"    // Blank out mode text first
                               u"Mode %d: %dx%d\r", 
                               mode_index, text_modes[mode_index].cols, text_modes[mode_index].rows);

                        mode_index--;
                        current_row--;
                        cout->SetCursorPosition(cout, 0, current_row);
                        cout->SetAttribute(cout, EFI_TEXT_ATTR(HIGHLIGHT_FG_COLOR, HIGHLIGHT_BG_COLOR));
                        printf(u"                    \r"    // Blank out mode text first
                               u"Mode %d: %dx%d\r", 
                               mode_index, text_modes[mode_index].cols, text_modes[mode_index].rows);
                    }

                    // Reset colors
                    cout->SetAttribute(cout, EFI_TEXT_ATTR(DEFAULT_FG_COLOR, DEFAULT_BG_COLOR));
                    break;

                case SCANCODE_DOWN_ARROW:
                    // NOTE: Max valid GOP mode is ModeMax-1 per UEFI spec
                    if (current_row == menu_bottom && mode_index < max-1) {
                        // Scroll menu down by incrementing all modes by 1
                        mode_index -= menu_len - 1;

                        // Reset cursor to top of menu
                        cout->SetCursorPosition(cout, 0, menu_top);

                        // Print modes up until the last menu row
                        for (UINT32 i = 0; i < menu_len; i++, mode_index++) {
                            printf(u"                    \r"    // Blank out mode text first
                                   u"Mode %d: %dx%d\r\n", 
                                   mode_index, text_modes[mode_index].cols, text_modes[mode_index].rows);
                        }

                        // Highlight last row
                        cout->SetAttribute(cout, EFI_TEXT_ATTR(HIGHLIGHT_FG_COLOR, HIGHLIGHT_BG_COLOR));
                        printf(u"                    \r"    // Blank out mode text first
                               u"Mode %d: %dx%d\r", 
                               mode_index, text_modes[mode_index].cols, text_modes[mode_index].rows);

                    } else if (current_row+1 <= menu_bottom) {
                        // De-highlight current row, move down 1 row, highlight new row
                        printf(u"                    \r"    // Blank out mode text first
                               u"Mode %d: %dx%d\r\n", 
                               mode_index, text_modes[mode_index].cols, text_modes[mode_index].rows);

                        mode_index++;
                        current_row++;
                        cout->SetAttribute(cout, EFI_TEXT_ATTR(HIGHLIGHT_FG_COLOR, HIGHLIGHT_BG_COLOR));
                        printf(u"                    \r"    // Blank out mode text first
                               u"Mode %d: %dx%d\r", 
                               mode_index, text_modes[mode_index].cols, text_modes[mode_index].rows);
                    }

                    // Reset colors
                    cout->SetAttribute(cout, EFI_TEXT_ATTR(DEFAULT_FG_COLOR, DEFAULT_BG_COLOR));
                    break;

                default:
                    if (key.UnicodeChar == u'\r' && text_modes[mode_index].cols != 0) {	// Qemu can have invalid text modes
                        // Enter key, set Text mode
                        cout->SetMode(cout, mode_index);
                        cout->QueryMode(cout, mode_index, &text_modes[mode_index].cols, &text_modes[mode_index].rows);

                        // Clear text screen
			cout->ClearScreen(cout);

                        getting_input = false;  // Will leave input loop and redraw screen
                        mode_index = 0;         // Reset last selected mode in menu
                    }
                    break;
            }
        }
    }

    return EFI_SUCCESS;
}

// ====================
// Set Graphics Mode
// ====================
EFI_STATUS set_graphics_mode(void) {
    // Get GOP protocol via LocateProtocol()
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID; 
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info = NULL;
    UINTN mode_info_size = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    EFI_STATUS status = 0;
    UINTN mode_index = 0;   // Current mode within entire menu of GOP mode choices;

    // Store found GOP mode info
    typedef struct {
        UINT32 width;
        UINT32 height;
    } Gop_Mode_Info;

    Gop_Mode_Info gop_modes[50];

    status = bs->LocateProtocol(&gop_guid, NULL, (VOID **)&gop);
    if (EFI_ERROR(status)) {
        printf(u"\r\nERROR: %x; Could not locate GOP! :(\r\n", status);
        get_key();
        return status;
    }

    // Overall screen loop
    while (true) {
        cout->ClearScreen(cout);

        // Write String
        printf(u"Graphics mode information:\r\n");

        // Get current GOP mode information
        status = gop->QueryMode(gop, 
                                gop->Mode->Mode, 
                                &mode_info_size, 
                                &mode_info);

        if (EFI_ERROR(status)) {
            printf(u"\r\nERROR: %x; Could not Query GOP Mode %u\r\n", status, gop->Mode->Mode);
            get_key();
            return status;
        }

        // Print info
        printf(u"Max Mode: %d\r\n"
               u"Current Mode: %d\r\n"
               u"WidthxHeight: %ux%u\r\n"
               u"Framebuffer address: %x\r\n"
               u"Framebuffer size: %x\r\n"
               u"PixelFormat: %d\r\n"
               u"PixelsPerScanLine: %u\r\n",
               gop->Mode->MaxMode,
               gop->Mode->Mode,
               mode_info->HorizontalResolution, mode_info->VerticalResolution,
               gop->Mode->FrameBufferBase,
               gop->Mode->FrameBufferSize,
               mode_info->PixelFormat,
               mode_info->PixelsPerScanLine);

        cout->OutputString(cout, u"\r\nAvailable GOP modes:\r\n");

        // Get current text mode ColsxRows values
        UINTN menu_top = cout->Mode->CursorRow, menu_bottom = 0, max_cols;
        cout->QueryMode(cout, cout->Mode->Mode, &max_cols, &menu_bottom);

        // Print keybinds at bottom of screen
        cout->SetCursorPosition(cout, 0, menu_bottom-3);
        printf(u"Up/Down Arrow = Move Cursor\r\n"
               u"Enter = Select\r\n"
               u"Escape = Go Back");

        cout->SetCursorPosition(cout, 0, menu_top);
        menu_bottom -= 5;   // Bottom of menu will be 2 rows above keybinds
        UINTN menu_len = menu_bottom - menu_top;

        // Get all available GOP modes' info
        const UINT32 max = gop->Mode->MaxMode;
        if (max < menu_len) {
            // Bound menu by actual # of available modes
            menu_bottom = menu_top + max-1;
            menu_len = menu_bottom - menu_top;  // Limit # of modes in menu to max mode - 1
        }

        for (UINT32 i = 0; i < ARRAY_SIZE(gop_modes) && i < max; i++) {
            gop->QueryMode(gop, i, &mode_info_size, &mode_info);

            gop_modes[i].width = mode_info->HorizontalResolution;
            gop_modes[i].height = mode_info->VerticalResolution;
        }

        // Highlight top menu row to start off
        cout->SetAttribute(cout, EFI_TEXT_ATTR(HIGHLIGHT_FG_COLOR, HIGHLIGHT_BG_COLOR));
        printf(u"Mode %d: %dx%d", 0, gop_modes[0].width, gop_modes[0].height);

        // Print other text mode infos
        cout->SetAttribute(cout, EFI_TEXT_ATTR(DEFAULT_FG_COLOR, DEFAULT_BG_COLOR));
        for (UINT32 i = 1; i < menu_len + 1; i++) 
            printf(u"\r\nMode %d: %dx%d", i, gop_modes[i].width, gop_modes[i].height);

        // Get input from user 
        cout->SetCursorPosition(cout, 0, menu_top);
        bool getting_input = true;
        while (getting_input) {
            UINTN current_row = cout->Mode->CursorRow;

            EFI_INPUT_KEY key = get_key();
            switch (key.ScanCode) {
                case SCANCODE_ESC: return EFI_SUCCESS;  // ESC Key: Go back to main menu

                case SCANCODE_UP_ARROW:
                    if (current_row == menu_top && mode_index > 0) {
                        // Scroll menu up by decrementing all modes by 1
                        printf(u"                    \r");  // Blank out mode text first

                        cout->SetAttribute(cout, EFI_TEXT_ATTR(HIGHLIGHT_FG_COLOR, HIGHLIGHT_BG_COLOR));
                        mode_index--;
                        printf(u"Mode %d: %dx%d", 
                               mode_index, gop_modes[mode_index].width, gop_modes[mode_index].height);

                        cout->SetAttribute(cout, EFI_TEXT_ATTR(DEFAULT_FG_COLOR, DEFAULT_BG_COLOR));
                        UINTN temp_mode = mode_index + 1;
                        for (UINT32 i = 0; i < menu_len; i++, temp_mode++) {
                            printf(u"\r\n                    \r"  // Blank out mode text first
                                   u"Mode %d: %dx%d\r", 
                                   temp_mode, gop_modes[temp_mode].width, gop_modes[temp_mode].height);
                        }

                        // Reset cursor to top of menu
                        cout->SetCursorPosition(cout, 0, menu_top);

                    } else if (current_row-1 >= menu_top) {
                        // De-highlight current row, move up 1 row, highlight new row
                        printf(u"                    \r"    // Blank out mode text first
                               u"Mode %d: %dx%d\r", 
                               mode_index, gop_modes[mode_index].width, gop_modes[mode_index].height);

                        mode_index--;
                        current_row--;
                        cout->SetCursorPosition(cout, 0, current_row);
                        cout->SetAttribute(cout, EFI_TEXT_ATTR(HIGHLIGHT_FG_COLOR, HIGHLIGHT_BG_COLOR));
                        printf(u"                    \r"    // Blank out mode text first
                               u"Mode %d: %dx%d\r", 
                               mode_index, gop_modes[mode_index].width, gop_modes[mode_index].height);
                    }

                    // Reset colors
                    cout->SetAttribute(cout, EFI_TEXT_ATTR(DEFAULT_FG_COLOR, DEFAULT_BG_COLOR));
                    break;

                case SCANCODE_DOWN_ARROW:
                    // NOTE: Max valid GOP mode is ModeMax-1 per UEFI spec
                    if (current_row == menu_bottom && mode_index < max-1) {
                        // Scroll menu down by incrementing all modes by 1
                        mode_index -= menu_len - 1;

                        // Reset cursor to top of menu
                        cout->SetCursorPosition(cout, 0, menu_top);

                        // Print modes up until the last menu row
                        for (UINT32 i = 0; i < menu_len; i++, mode_index++) {
                            printf(u"                    \r"    // Blank out mode text first
                                   u"Mode %d: %dx%d\r\n", 
                                   mode_index, gop_modes[mode_index].width, gop_modes[mode_index].height);
                        }

                        // Highlight last row
                        cout->SetAttribute(cout, EFI_TEXT_ATTR(HIGHLIGHT_FG_COLOR, HIGHLIGHT_BG_COLOR));
                        printf(u"                    \r"    // Blank out mode text first
                               u"Mode %d: %dx%d\r", 
                               mode_index, gop_modes[mode_index].width, gop_modes[mode_index].height);

                    } else if (current_row+1 <= menu_bottom) {
                        // De-highlight current row, move down 1 row, highlight new row
                        printf(u"                    \r"    // Blank out mode text first
                               u"Mode %d: %dx%d\r\n", 
                               mode_index, gop_modes[mode_index].width, gop_modes[mode_index].height);

                        mode_index++;
                        current_row++;
                        cout->SetAttribute(cout, EFI_TEXT_ATTR(HIGHLIGHT_FG_COLOR, HIGHLIGHT_BG_COLOR));
                        printf(u"                    \r"    // Blank out mode text first
                               u"Mode %d: %dx%d\r", 
                               mode_index, gop_modes[mode_index].width, gop_modes[mode_index].height);
                    }

                    // Reset colors
                    cout->SetAttribute(cout, EFI_TEXT_ATTR(DEFAULT_FG_COLOR, DEFAULT_BG_COLOR));
                    break;

                default:
                    if (key.UnicodeChar == u'\r') {
                        // Enter key, set GOP mode
                        gop->SetMode(gop, mode_index);
                        gop->QueryMode(gop, mode_index, &mode_info_size, &mode_info);

                        // Clear GOP screen - EFI_BLUE seems to have a hex value of 0x98
                        EFI_GRAPHICS_OUTPUT_BLT_PIXEL px = { 0x98, 0x00, 0x00, 0x00 };  // BGR_8888
                        gop->Blt(gop, &px, EfiBltVideoFill, 
                                 0, 0,  // Origin BLT BUFFER X,Y
                                 0, 0,  // Destination screen X,Y
                                 mode_info->HorizontalResolution, mode_info->VerticalResolution,
                                 0);

                        getting_input = false;  // Will leave input loop and redraw screen
                        mode_index = 0;         // Reset last selected mode in menu
                    }
                    break;
            }
        }
    }

    return EFI_SUCCESS;
}

// =========================================================
// Show Menu Function
// =========================================================
void ShowMenu(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *cout, UINTN selected) {
    clear_screen();

    cout->OutputString(cout, (CHAR16 *)OS_NAME L" " OS_CP L" Loaded in UEFI\r\n\r\n");
    cout->OutputString(cout, (CHAR16 *)L"Use W/S to navigate, ENTER to confirm.\r\n\r\n");
    cout->OutputString(cout, (CHAR16 *)L"Press q/q/ESC to escape a under menu or program.\r\n\r\n");

    const CHAR16* options[] = {
        L"Shutdown",
        L"Reboot",
        L"Recovery Mode",
        L"Change Text Mode",
        L"Read ESP Files",
        L"Print Block IO Partition's Info",
        L"Read UDP Files",
        L"UEFI Boot Order",
        L"Print Current Time and Date",
        L"Mouse Test",
        L"Set Grafics Mode",
        L"Load Kernel",
        L"Print OS Info",
        L"Print Memory Map"
    };
    const UINTN optionCount = sizeof(options) / sizeof(options[0]);

    for (UINTN i = 0; i < optionCount; i++) {
        if (i == selected) {
            cout->SetAttribute(cout, EFI_TEXT_ATTR(EFI_BLACK, EFI_WHITE));
            cout->OutputString(cout, (CHAR16 *)L"> ");
            cout->OutputString(cout, (CHAR16 *)options[i]);
            cout->OutputString(cout, (CHAR16 *)L"\r\n");
            cout->SetAttribute(cout, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLACK));
        } else {
            cout->OutputString(cout, (CHAR16 *)L"  ");
            cout->OutputString(cout, (CHAR16 *)options[i]);
            cout->OutputString(cout, (CHAR16 *)L"\r\n");
        }
    }
}

// =========================================================
// EFI Main Function
// =========================================================
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle __attribute__((unused)), EFI_SYSTEM_TABLE *SystemTable) {
    load_variables(ImageHandle, SystemTable);
    clear_screen();

    UINTN selected = 11;
    EFI_INPUT_KEY key;
    const UINTN optionCount = 14;

    ShowMenu(cout, selected);

    while (1) {
        if (cin->ReadKeyStroke(cin, &key) == EFI_SUCCESS) {
            if (key.UnicodeChar == L'w' || key.UnicodeChar == L'W') {
                selected = (selected == 0) ? optionCount - 1 : selected - 1;
                ShowMenu(cout, selected);
            } else if (key.UnicodeChar == L's' || key.UnicodeChar == L'S') {
                selected = (selected + 1) % optionCount;
                ShowMenu(cout, selected);
            } else if (key.UnicodeChar == L'\r' || key.UnicodeChar == 0x0D) { // Enter
                switch (selected) {
                    case 0:
                        shutdown();
                        break;
                    case 1:
                        reboot();
                        break;
                    case 2:
                        cout->OutputString(cout, L"\r\nEntering Recovery Mode...\r\n");
                        // TODO: Lägg in recovery mode-logik
                        break;
                    case 3:
                    {
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
                        
                        set_text_mode();
                        ShowMenu(cout, selected);

                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
                        break;
                    }
                    case 4:
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

                        read_esp_files(ImageHandle);
                        ShowMenu(cout, selected); 

                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
                        break;
                    case 5:
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

                        print_block_io_partitions();
                        ShowMenu(cout, selected);

                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
                        break;
                    case 6:
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

                        udp_main();
                        ShowMenu(cout, selected);

                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
                        break;
                    case 7:
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

                        UEFI_Boot_Order();
                        ShowMenu(cout, selected);

                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
                        break;
                    case 8:
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

                        Time();
                        ShowMenu(cout, selected);

                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
                        break;
                    case 9:
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

                        test_mouse();
                        ShowMenu(cout, selected);
                        
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
                        break;
                    case 10:
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

                        set_graphics_mode();
                        ShowMenu(cout, selected);
                        
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
                        break;
                    case 11:
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

                        load_kernel();
                        ShowMenu(cout, selected);
                        
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
                        break;
                    case 12:
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

                        print_os_info();
                        ShowMenu(cout, selected);
                        
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
                        break;
                    case 13:
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);

                        print_memory_map();
                        ShowMenu(cout, selected);
                        
                        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 0);
                        break;
                }
            }
        }
    }
    return EFI_SUCCESS; // Kommer aldrig nås, men krävs
}
