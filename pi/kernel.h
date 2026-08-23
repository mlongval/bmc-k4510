// BMC-K4510 Pi kernel: core 0 devices, core 1 the emulator, core 2 presentation.
#ifndef _kernel_h
#define _kernel_h
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/sched/scheduler.h>
#include <circle/multicore.h>
#include <circle/memory.h>
#include <circle/types.h>
#include <SDCard/emmc.h>
#include <fatfs/ff.h>

enum TShutdownMode { ShutdownNone, ShutdownHalt, ShutdownReboot };

class CK4510Cores : public CMultiCoreSupport
{
public:
    CK4510Cores(void) : CMultiCoreSupport(CMemorySystem::Get()) {}
    void Run(unsigned nCore) override;
};

class CKernel
{
public:
    CKernel(void);
    boolean Initialize(void);
    TShutdownMode Run(void);
private:
    CActLED             m_ActLED;
    CKernelOptions      m_Options;
    CDeviceNameService  m_DeviceNameService;
    CSerialDevice       m_Serial;
    CExceptionHandler   m_ExceptionHandler;
    CInterruptSystem    m_Interrupt;
    CTimer              m_Timer;
    CLogger             m_Logger;
    CScheduler          m_Scheduler;
    CEMMCDevice         m_EMMC;
    FATFS               m_FileSystem;
    CK4510Cores         m_Cores;
};
#endif
