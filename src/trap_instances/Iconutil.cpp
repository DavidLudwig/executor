#define INSTANTIATE_TRAPS_Iconutil
#include <Iconutil.h>

// Function for preventing the linker from considering the static constructors in this module unused
namespace Executor {
namespace ReferenceTraps {
    void Iconutil() {}
}
}
