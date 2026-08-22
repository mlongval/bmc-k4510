//
// BMC-K4510 on the Pi 3B+: bring the board up, mount the card, then hand
// over to the same frontend the desktop runs (sdl/main.c) against
// circle-libsdl2. Files live in SD:/k4510/{rom,data,fs}.
//
#include "kernel.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_circle.h>
#include <unistd.h>
#include <cstdio>

extern "C" int  k4510_frontend_main(int argc, char **argv);
extern "C" void c64kbd_init(void);

static const char From[] = "k4510";

CKernel::CKernel(void)
    : m_Serial(0, FALSE, 0), m_Timer(&m_Interrupt), m_Logger(m_Options.GetLogLevel(), &m_Timer),
      m_EMMC(&m_Interrupt, &m_Timer, &m_ActLED)
{
    m_ActLED.Blink(2);
}

boolean CKernel::Initialize(void)
{
    boolean bOK = TRUE;
    if (bOK) bOK = m_Serial.Initialize(115200);
    if (bOK) bOK = m_Logger.Initialize(&m_Serial);
    if (bOK) bOK = m_Interrupt.Initialize();
    if (bOK) bOK = m_Timer.Initialize();
    if (bOK) bOK = m_EMMC.Initialize();
    if (bOK) { FRESULT r = f_mount(&m_FileSystem, "SD:", 1); if (r != FR_OK) { m_Logger.Write(From, LogError, "mount SD: failed (%d)", (int)r); bOK = FALSE; } }
    if (bOK) SDL2Circle_ArmCoreRuntime();
    return bOK;
}

TShutdownMode CKernel::Run(void)
{
    m_Logger.Write(From, LogNotice, "BMC-K4510 -- 45GS02 / VICKe / SHEILA / 4 x SID, bare metal");
    if (SDL2Circle_DeclareVirtualDevice(32, 640, 480) != 0)
        m_Logger.Write(From, LogWarning, "virtual device: %s", SDL_GetError());
    if (chdir("SD:/k4510") != 0) { m_Logger.Write(From, LogError, "no SD:/k4510 directory on the card"); return ShutdownHalt; }
    c64kbd_init();
    static char a0[] = "k4510", a1[] = "rom/kernal.bin", a2[] = "fs";
    char *argv[] = { a0, a1, a2, nullptr };
    int rc = k4510_frontend_main(3, argv);
    m_Logger.Write(From, LogNotice, "frontend returned %d", rc);
    return ShutdownReboot;
}
