#if !defined(_syncint_h_)
#define _syncint_h_

#include <chrono>

namespace Executor
{
extern void syncint_init(void);
extern void syncint_post(std::chrono::microseconds usecs, bool fromLast = false);
extern void syncint_wait_interrupt();
extern void syncint_check_interrupt();
}

#endif /* !_syncint_h_ */
