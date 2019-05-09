#include "gtest/gtest.h"
#include <FileMgr.h>
#include <file/file.h>
#include <MemoryMgr.h>
#include <WindowMgr.h>
#include <MenuMgr.h>
#include <FontMgr.h>

using namespace Executor;

QDGlobals qd;
#include <quickdraw/cquick.h>
#include <vdriver/vdriver.h>
#include <ResourceMgr.h>

#include <PowerCore.h>

StringPtr PSTR(const char* s)
{
    size_t n = strlen(s);
    StringPtr p = (StringPtr)NewPtr(n + 1);   // just leaking it
    p[0] = n;
    memcpy(p+1, s, n);
    return p;
}

class MockVDriver : public VideoDriver
{
    virtual void setColors(int first_color, int num_colors,
                               const struct ColorSpec *color_array) override
    {

    }

    virtual bool setMode(int width, int height, int bpp,
                                bool grayscale_p) override
    {
        width_ = 512;
        height_ = 342;
        rowBytes_ = 64;
        framebuffer_ = new uint8_t[64*342];
        bpp_ = 1;
        return true;
    }
};

class ExecutorTestEnvironment : public testing::Environment
{
    char *thingOnStack;
    fs::path tempDir;
public:
    ExecutorTestEnvironment(char* thingOnStack) : thingOnStack(thingOnStack) {}

    virtual void SetUp() override
    {
        tempDir = fs::current_path() / fs::unique_path("temptest-%%%%-%%%%-%%%%-%%%%");

        Executor::InitMemory(thingOnStack);
        Executor::ROMlib_fileinit();

        fs::create_directory(tempDir);

        if(auto fsspec = nativePathToFSSpec(tempDir))
        {
            WDPBRec wdpb;

            wdpb.ioVRefNum = fsspec->vRefNum;
            wdpb.ioWDDirID = fsspec->parID;
            wdpb.ioWDProcID = FOURCC('X', 'c', 't', 'r');
            wdpb.ioNamePtr = fsspec->name;
            PBOpenWD(&wdpb, false);
            SetVol(nullptr, wdpb.ioVRefNum);
        }

        vdriver = new MockVDriver();
        initialize_68k_emulator(nullptr, false, (uint32_t *)SYN68K_TO_US(0), 0);
        traps::init(false);

        PowerCore& cpu = getPowerCore();
#if SIZEOF_CHAR_P == 4
        cpu.memoryBases[0] = (void*)ROMlib_offset;
        cpu.memoryBases[1] = nullptr;
        cpu.memoryBases[2] = nullptr;
        cpu.memoryBases[3] = nullptr;
#elif SIZEOF_CHAR_P >= 8        
        cpu.memoryBases[0] = (void*)ROMlib_offsets[0];
        cpu.memoryBases[1] = (void*)ROMlib_offsets[1];
        cpu.memoryBases[2] = (void*)ROMlib_offsets[2];
        cpu.memoryBases[3] = (void*)ROMlib_offsets[3];
#endif

        LM(ResErrProc) = 0;
        InitResources();
        ROMlib_InitGDevices();
        LM(TheZone) = LM(ApplZone);

        ROMlib_color_init();
        LM(WindowList) = nullptr;
        LM(MenuList) = nullptr;
        LM(MBDFHndl) = nullptr;
        LM(MBSaveLoc) = nullptr;
        EM_A5 = EM_A7 = ptr_to_longint(LM(MemTop)-4);
        InitGraf((Ptr)&qd.thePort);
        InitFonts();
        InitWindows();
        InitMenus();
    }

    virtual void TearDown() override
    {
        if(!tempDir.empty())
            fs::remove_all(tempDir);
    }
};

TEST(MemoryMgr, NewClearGetSizeDispose)
{
    Ptr p = NewPtrClear(42);

    ASSERT_NE(nullptr, p);
    ASSERT_EQ(0, *p);

    ASSERT_EQ(42, GetPtrSize(p));

    DisposePtr(p);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    char thingOnStack;
    testing::AddGlobalTestEnvironment(new ExecutorTestEnvironment(&thingOnStack));

    int result = RUN_ALL_TESTS();
    return result;
}
