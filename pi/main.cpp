#include "kernel.h"
#include <circle/startup.h>
int main(void)
{
    CKernel Kernel;
    if (!Kernel.Initialize()) { halt(); return EXIT_HALT; }
    TShutdownMode Mode = Kernel.Run();
    if (Mode == ShutdownReboot) { reboot(); return EXIT_REBOOT; }
    halt();
    return EXIT_HALT;
}
