#include "include/efi.h"
#include "include/cb/gen.h"
#include "include/cb/kcb.h"
#include <stdarg.h>
#include <stdbool.h>

// =========================================================
// Globala variabler
// =========================================================
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *cout = NULL;
EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *cin  = NULL;
EFI_RUNTIME_SERVICES            *rs   = NULL;
EFI_BOOT_SERVICES               *bs   = NULL;

// =========================================================
// Kernel Entry
// =========================================================
void EFIAPI kmain(kp *kargs) {
    init(kargs);
    cout->SetCursorPosition(cout, 0, 0);

    UINTN selected = 0;
    EFI_INPUT_KEY key;
    const UINTN optionCount = 3;

    MainMenu(selected, kargs);

    while (1) {
        if (cin->ReadKeyStroke(cin, &key) == EFI_SUCCESS) {
            if (key.UnicodeChar == u'w' || key.UnicodeChar == u'W') {
                selected = (selected == 0) ? optionCount - 1 : selected - 1;
                MainMenu(selected, kargs);
            } else if (key.UnicodeChar == u's' || key.UnicodeChar == u'S') {
                selected = (selected + 1) % optionCount;
                MainMenu(selected, kargs);
            } else if (key.UnicodeChar == u'\r' || key.UnicodeChar == 0x0D) {
                switch (selected) {
                    case 0:
                        screen_info(kargs);
                        MainMenu(selected, kargs);
                        break;
                    case 1:
                        shutdown(kargs);
                        break;
                    case 2:
                        cout->OutputString(cout, u"Exiting to UEFI Shell...\r\n");
                        return;
                }
            }
        }
    }
}

// =========================================================
// Init
// =========================================================
void init(kp *kargs) {
    cout = kargs->ST->ConOut;
    cin  = kargs->ST->ConIn;
    rs   = kargs->ST->RuntimeServices;
    bs   = kargs->ST->BootServices;
}

// =========================================================
// Shutdown
// =========================================================
void shutdown(kp *kargs) {
    cout->OutputString(cout, u"\r\nShutting down...\r\n");
    kargs->ST->RuntimeServices->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);
    while (1);
}

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

// =========================================================
// Screen info
// =========================================================
void screen_info(kp *kargs) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = kargs->gop;
    if (!gop) return;

    CHAR16 buffer[256];
    CHAR16 num1[12], num2[12];

    utoa16(gop->Mode->Mode, num1, sizeof(num1)/sizeof(CHAR16));
    utoa16(gop->Mode->MaxMode, num2, sizeof(num2)/sizeof(CHAR16));

    int pos = 0;
    const CHAR16 *text = u"Current Mode: ";
    while (*text) buffer[pos++] = *text++;
    int k = 0; while (num1[k]) buffer[pos++] = num1[k++];
    buffer[pos++] = u'/';
    k = 0; while (num2[k]) buffer[pos++] = num2[k++];
    buffer[pos++] = u'\r';
    buffer[pos++] = u'\n';
    buffer[pos] = u'\0';

    cout->OutputString(cout, buffer);

    for (UINT32 mode = 0; mode < gop->Mode->MaxMode; mode++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        UINTN size_of_info;
        EFI_STATUS status = gop->QueryMode(gop, mode, &size_of_info, &info);
        if (EFI_ERROR(status)) continue;

        CHAR16 mnum[12], hres[12], vres[12], ppf[12], ppsl[12];
        utoa16(mode, mnum, 12);
        utoa16(info->HorizontalResolution, hres, 12);
        utoa16(info->VerticalResolution, vres, 12);
        utoa16(info->PixelFormat, ppf, 12);
        utoa16(info->PixelsPerScanLine, ppsl, 12);

        pos = 0;
        text = u"Mode ";
        while (*text) buffer[pos++] = *text++;
        k = 0; while (mnum[k]) buffer[pos++] = mnum[k++];
        text = u": ";
        while (*text) buffer[pos++] = *text++;
        k = 0; while (hres[k]) buffer[pos++] = hres[k++];
        buffer[pos++] = u'x';
        k = 0; while (vres[k]) buffer[pos++] = vres[k++];
        text = u", PixelFormat=";
        while (*text) buffer[pos++] = *text++;
        k = 0; while (ppf[k]) buffer[pos++] = ppf[k++];
        text = u", PixelsPerScanLine=";
        while (*text) buffer[pos++] = *text++;
        k = 0; while (ppsl[k]) buffer[pos++] = ppsl[k++];
        buffer[pos++] = u'\r';
        buffer[pos++] = u'\n';
        buffer[pos] = u'\0';

        cout->OutputString(cout, buffer);
    }

    cout->OutputString(cout, u"\r\nPress any key to return to menu...\r\n");
    get_key();
}

// =========================================================
// Clear screen
// =========================================================
void clear_screen(void) {
    if (cout) cout->ClearScreen(cout);
}

// =========================================================
// Main menu
// =========================================================
void MainMenu(UINTN selected, kp *kargs) {
    clear_screen();
    cout->OutputString(cout, u"Use W/S to navigate, ENTER to confirm.\r\n\r\n");

    CHAR16* options[] = {
        u"Screen Info",
        u"Shutdown System",
        u"Back to Bootloader"
    };
    const UINTN optionCount = sizeof(options)/sizeof(options[0]);

    for (UINTN i = 0; i < optionCount; i++) {
        if (i == selected) {
            cout->SetAttribute(cout, EFI_TEXT_ATTR(EFI_BLACK, EFI_WHITE));
            cout->OutputString(cout, u"> ");
            cout->OutputString(cout, options[i]);
            cout->OutputString(cout, u"\r\n");
            cout->SetAttribute(cout, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLACK));
        } else {
            cout->OutputString(cout, u"  ");
            cout->OutputString(cout, options[i]);
            cout->OutputString(cout, u"\r\n");
        }
    }
}

// =========================================================
// Helper: itoa16/utoa16/ultoa16
// =========================================================
void itoa16(int val, CHAR16 *buf) {
    if (!buf) return;
    if (val == 0) { buf[0]=u'0'; buf[1]=0; return; }
    int i=0, neg=0; if(val<0){neg=1;val=-val;}
    CHAR16 tmp[20]; int j=0;
    while(val){ tmp[j++]=(CHAR16)(u'0'+val%10); val/=10;}
    if(neg) tmp[j++]=u'-';
    for(i=0;i<j;i++) buf[i]=tmp[j-1-i];
    buf[j]=0;
}

void utoa16(uint32_t value, CHAR16* buffer, size_t bufsize) {
    CHAR16 temp[12]; 
    int i = 0;
    if(value == 0) {
        buffer[0] = u'0';
        buffer[1] = u'\0';
        return;
    }
    while(value > 0 && i < 11) {
        temp[i++] = u'0' + (value % 10);
        value /= 10;
    }
    int j = 0;
    while(i > 0 && j < (int)bufsize-1) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = u'\0';
}

void ultoa16(unsigned long long val, CHAR16 *buf){
    if(!buf) return; if(val==0){buf[0]=u'0';buf[1]=0; return;}
    int i=0; CHAR16 tmp[32]; int j=0;
    while(val){ tmp[j++]=(CHAR16)(u'0'+(val%10)); val/=10;}
    for(i=0;i<j;i++) buf[i]=tmp[j-1-i];
    buf[j]=0;
}
