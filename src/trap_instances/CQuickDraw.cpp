#define INSTANTIATE_TRAPS_CQuickDraw
#include <CQuickDraw.h>

// Function for preventing the linker from considering the static constructors in this module unused
namespace Executor {
namespace ReferenceTraps {
    void CQuickDraw() {}
}
}
