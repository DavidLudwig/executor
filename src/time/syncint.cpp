/* Copyright 1995, 1996 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

#include <base/common.h>

#include <time/syncint.h>
#include <base/cpu.h>
#include <PowerCore.h>

using namespace Executor;

void Executor::syncint_check_interrupt()
{
    if(INTERRUPT_PENDING())
    {
        syn68k_addr_t pc;

        pc = interrupt_process_any_pending(MAGIC_EXIT_EMULATOR_ADDRESS);
        if(pc != MAGIC_EXIT_EMULATOR_ADDRESS)
        {
            interpret_code(hash_lookup_code_and_create_if_needed(pc));
        }
    }
}

#if defined(WIN32)

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iostream>

using namespace std::literals::chrono_literals;

namespace
{
    using clock = std::chrono::steady_clock;
    std::mutex mutex;
    std::condition_variable cond;
    std::condition_variable wake_cond;
    clock::time_point last_interrupt;
    clock::time_point scheduled_interrupt;
    std::thread timer_thread;
}

void Executor::syncint_init(void)
{
    std::thread([]() {
        std::unique_lock<std::mutex> lock(mutex);
        for(;;)
        {
            if(scheduled_interrupt > last_interrupt)
            {
                auto status = cond.wait_until(lock, scheduled_interrupt);
                if(status == std::cv_status::timeout)
                {
                    last_interrupt = scheduled_interrupt;

                    interrupt_generate(M68K_TIMER_PRIORITY);
                    getPowerCore().requestInterrupt();
                    wake_cond.notify_all();
                }
            }
            else
            {
                cond.wait(lock);
            }
        }
    }).detach();
}

void Executor::syncint_wait_interrupt()
{
    std::unique_lock<std::mutex> lock(mutex);
    wake_cond.wait_for(lock, 1s, []() { return INTERRUPT_PENDING(); });

    // unlock here, in case the subsequent call to syncint_check_interrupt()
    // triggers a call to syncint_post(), which will need to lock 'mutex'.
    lock.unlock();

    syncint_check_interrupt();
}

void Executor::syncint_post(std::chrono::microseconds usecs, bool fromLast)
{
    std::unique_lock<std::mutex> lock(mutex);

    auto time = (fromLast ? last_interrupt : clock::now()) + usecs;

    if(time <= last_interrupt)
        last_interrupt = clock::time_point();
    if(scheduled_interrupt <= last_interrupt || time < scheduled_interrupt)
        scheduled_interrupt = time;
    
    cond.notify_one();
}
#else

#include <chrono>
#include <iostream>
#include <unistd.h>
#include <sys/time.h>

static void
handle_itimer_tick(int n)
{
    interrupt_generate(M68K_TIMER_PRIORITY);
    getPowerCore().requestInterrupt();
}

void Executor::syncint_init(void)
{
    struct sigaction s;

    s.sa_handler = handle_itimer_tick;
    sigemptyset(&s.sa_mask);
    s.sa_flags = 0;
    sigaction(SIGALRM, &s, nullptr);
}

/* Posting a delay of 0 will clear any pending interrupt. */
void Executor::syncint_post(std::chrono::microseconds usecs, bool fromLast)
{
    struct itimerval t;

    t.it_value.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(usecs).count();
    t.it_value.tv_usec = usecs.count() % 1000000;
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &t, nullptr);
}

void Executor::syncint_wait_interrupt()
{
    sigset_t zero_mask;
    sigemptyset(&zero_mask);
    sigsuspend(&zero_mask);
    syncint_check_interrupt();
}

#endif
