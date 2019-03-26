#include <base/logging.h>
#include <iomanip>
#include <cctype>

using namespace Executor;

int logging::nestingLevel = 0;
static bool loggingEnabled = false;

void logging::resetNestingLevel()
{
    nestingLevel  = 0;
}
void logging::indent()
{
    for(int i = 0; i < nestingLevel; i++)
        std::clog << "  ";
}

bool logging::enabled()
{
    return loggingEnabled;
}

void logging::setEnabled(bool e)
{
    loggingEnabled = e;
}

bool logging::loggingActive()
{
    return nestingLevel <= 1;
}

void logging::logEscapedChar(unsigned char c)
{
    if(c == '\'' || c == '\"' || c == '\\')
        std::clog << '\\' << c;
    else if(std::isprint(c))
        std::clog << c;
    else
        std::clog << "\\0" << std::oct << (unsigned)c << std::dec;
}

bool logging::canConvertBack(const void* p)
{
    if(!p || p == (const void*) -1)
        return true;
#if SIZEOF_CHAR_P == 4
    bool valid = true;
#else
    bool valid = false;
    for(int i = 0; i < 4; i++)
    {
        if((uintptr_t)p >= ROMlib_offsets[i] &&
            (uintptr_t)p < ROMlib_offsets[i] + ROMlib_sizes[i])
            valid = true;
    }
#endif
    return valid;
}

bool logging::validAddress(const void* p)
{
    if(!p)
        return false;
    if( (uintptr_t)p & 1 )
        return false;
#if SIZEOF_CHAR_P == 4
    bool valid = true;
#else
    bool valid = false;
    for(int i = 0; i < 4; i++)
    {
        if((uintptr_t)p >= ROMlib_offsets[i] &&
            (uintptr_t)p < ROMlib_offsets[i] + ROMlib_sizes[i])
            valid = true;
    }
#endif
    if(!valid)
        return false;

    return true;
}

bool logging::validAddress(syn68k_addr_t p)
{
    if(p == 0 || (p & 1))
        return false;
    return validAddress(SYN68K_TO_US(p));
}


void logging::logValue(char x)
{
    std::clog << (int)x;
    std::clog << " = '";
    logEscapedChar(x);
    std::clog << '\'';
}
void logging::logValue(unsigned char x)
{
    std::clog << (int)x;
    if(std::isprint(x))
        std::clog << " = '" << x << '\'';
}
void logging::logValue(signed char x)
{
    std::clog << (int)x;
    if(std::isprint(x))
        std::clog << " = '" << x << '\'';
}
void logging::logValue(int16_t x) { std::clog << x; }
void logging::logValue(uint16_t x) { std::clog << x; }
void logging::logValue(int32_t x)
{
    std::clog << x << " = '";
    logEscapedChar((x >> 24) & 0xFF);
    logEscapedChar((x >> 16) & 0xFF);
    logEscapedChar((x >> 8) & 0xFF);
    logEscapedChar(x & 0xFF);
    std::clog << "'";
}
void logging::logValue(uint32_t x)
{
    std::clog << x << " = '";
    logEscapedChar((x >> 24) & 0xFF);
    logEscapedChar((x >> 16) & 0xFF);
    logEscapedChar((x >> 8) & 0xFF);
    logEscapedChar(x & 0xFF);
    std::clog << "'";
}
void logging::logValue(unsigned char* p)
{
    std::clog << "0x" << std::hex << US_TO_SYN68K_CHECK0_CHECKNEG1(p) << std::dec;
    if(validAddress(p) && validAddress(p+256))
    {
        std::clog << " = \"\\p";
        for(int i = 1; i <= p[0]; i++)
            logEscapedChar(p[i]);
        std::clog << '"';
    }
}
void logging::logValue(const void* p)
{
    if(canConvertBack(p))
        std::clog << "0x" << std::hex << US_TO_SYN68K_CHECK0_CHECKNEG1(p) << std::dec;
    else
        std::clog << "?";
}
void logging::logValue(void* p)
{
    if(canConvertBack(p))
        std::clog << "0x" << std::hex << US_TO_SYN68K_CHECK0_CHECKNEG1(p) << std::dec;
    else
        std::clog << "?";
}
void logging::logValue(ProcPtr p)
{
    if(canConvertBack(p))
        std::clog << "0x" << std::hex << US_TO_SYN68K_CHECK0_CHECKNEG1(p) << std::dec;
    else
        std::clog << "?";
}


void logging::dumpRegsAndStack()
{
    std::clog << std::hex << /*std::showbase <<*/ std::setfill('0');
    std::clog << "D0=" << std::setw(8) << EM_D0 << " ";
    std::clog << "D1=" << std::setw(8) << EM_D1 << " ";
    std::clog << "A0=" << std::setw(8) << EM_A0 << " ";
    std::clog << "A1=" << std::setw(8) << EM_A1 << " ";
    //std::clog << std::noshowbase;
    std::clog << "Stack: ";
    uint8_t *p = (uint8_t*)SYN68K_TO_US(EM_A7);
    for(int i = 0; i < 12; i++)
        std::clog << std::setfill('0') << std::setw(2) << (unsigned)p[i] << " ";
    std::clog << std::dec;
}

void logging::logUntypedArgs(const char *name)
{
    if(loggingActive())
    {
        std::clog.clear();
        indent();
        std::clog << name << " ";
        dumpRegsAndStack();
        std::clog << std::endl;
    }
}
void logging::logUntypedReturn(const char *name)
{
    if(loggingActive())
    {
        indent();
        std::clog << "returning: " << name << " ";
        dumpRegsAndStack();
        std::clog << std::endl << std::flush;
    }
}
