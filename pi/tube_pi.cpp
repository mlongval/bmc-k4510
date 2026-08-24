// The Tube co-processor's platform on the Pi: time and sleep for core 3.
// SDL_GetTicks and the performance counter read the system timer directly
// from any core. The waits are deliberately NOT SDL_Delay: off core 0 the
// shim's delay runs the audio callback from the calling context (docs/
// CORE-SPLIT.md), and the SIDs belong to core 1. Core 3 exists only to run
// the interpreter, so spinning on the counter with a yield hint is the
// right wait here -- exactly what the shim itself does off core 0.
#include <SDL2/SDL.h>
#include "../core/tube_cp.h"

extern "C" unsigned tube_cp_ticks(void) { return SDL_GetTicks(); }

extern "C" void tube_cp_usleep(unsigned us)
{
    Uint64 end = SDL_GetPerformanceCounter() + (Uint64)us * SDL_GetPerformanceFrequency() / 1000000u;
    while (SDL_GetPerformanceCounter() < end) asm volatile("yield" ::: "memory");
}
