/* Copyright 1989 - 1996 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

#include <base/common.h>
#include <QuickDraw.h>
#include <PrintMgr.h>
#include <MemoryMgr.h>
#include <FontMgr.h>
#include <ResourceMgr.h>

#include <print/PSstrings.h>
#include <mman/mman.h>
#include <rsys/uniquefile.h>
#include <print/print.h>
#include <rsys/string.h>
#include <quickdraw/cquick.h>
#include <vdriver/vdriver.h>
#include <file/file.h>

#include <prefs/prefs.h>

#if defined(MSDOS) || defined(CYGWIN32)
#include <stdarg.h>
#include <rsys/cleanup.h>

bool deferred_printing_p = false /* true */;

#endif

using namespace Executor;
using namespace std;

int pageno = 0; /* This isn't really the way to do it */
namespace Executor
{
string ROMlib_document_paper_sizes;
string ROMlib_paper_size;
string ROMlib_paper_size_name;
string ROMlib_paper_size_name_terminator;
int ROMlib_rotation = 0;
int ROMlib_translate_x = 0;
int ROMlib_translate_y = 0;
int ROMlib_resolution_x = 72;
int ROMlib_resolution_y = 72;

int ROMlib_paper_x = 0;
int ROMlib_paper_y = 0;

/* This boolean is here to prevent Energy Scheming from causing trouble.
   ES calls PrPageClose twice at the end.  This fix is sub-optimal, but
   probably won't hurt anything. */

static bool page_is_open = false;
}
#include <print/nextprint.h>

LONGINT Executor::pagewanted = 0;
static int lastpagewanted = 0;

void Executor::C_donotPrArc(GrafVerb verb, Rect *r, INTEGER starta,
                            INTEGER arca)
{
}

void Executor::C_PrArc(GrafVerb verb, Rect *r, INTEGER starta, INTEGER arca)
{
    if(pageno >= pagewanted && pageno <= lastpagewanted)
        NeXTPrArc(verb, r, starta, arca, qdGlobals().thePort);
}

void Executor::C_donotPrBits(const BitMap *srcbmp, const Rect *srcrp, const Rect *dstrp,
                             INTEGER mode, RgnHandle mask)
{
}

void Executor::C_PrBits(const BitMap *srcbmp, const Rect *srcrp, const Rect *dstrp, INTEGER mode,
                        RgnHandle mask)
{
    if(pageno >= pagewanted && pageno <= lastpagewanted)
        NeXTPrBits(srcbmp, srcrp, dstrp,
                   mode, mask, qdGlobals().thePort);
}

void Executor::C_donotPrLine(Point p)
{
}

void Executor::C_PrLine(Point p)
{
    if(pageno >= pagewanted && pageno <= lastpagewanted)
    {
        Point cp;

        cp.h = p.h;
        cp.v = p.v;
        NeXTPrLine(cp, qdGlobals().thePort);
    }
}

void Executor::C_donotPrOval(GrafVerb v, Rect *rp)
{
}

void Executor::C_PrOval(GrafVerb v, Rect *rp)
{
    if(pageno >= pagewanted && pageno <= lastpagewanted)
        NeXTPrOval(v, rp, qdGlobals().thePort);
}

void Executor::C_textasPS(INTEGER n, Ptr textbufp, Point num, Point den)
{
#if 1
    if(pageno >= pagewanted && pageno <= lastpagewanted)
    {
        NeXTsendps(n, textbufp);
#if !defined(CYGWIN32)
        NeXTsendps(1, (Ptr) "\r");
#else
        NeXTsendps(2, (Ptr) "\r\n");
#endif
    }
#endif
}

void Executor::C_donotPrGetPic(Ptr dp, INTEGER bc)
{
    gui_abort();
}

void Executor::C_PrGetPic(Ptr dp, INTEGER bc)
{
    gui_abort();
    if(pageno >= pagewanted && pageno <= lastpagewanted)
        NeXTPrGetPic(dp, bc, qdGlobals().thePort);
}

void Executor::C_donotPrPutPic(Ptr sp, INTEGER bc)
{
}

void Executor::C_PrPutPic(Ptr sp, INTEGER bc)
{
    if(pageno >= pagewanted && pageno <= lastpagewanted)
        NeXTPrPutPic(sp, bc, qdGlobals().thePort);
}

void Executor::C_donotPrPoly(GrafVerb verb, PolyHandle ph)
{
}

void Executor::C_PrPoly(GrafVerb verb, PolyHandle ph)
{
    if(pageno >= pagewanted && pageno <= lastpagewanted)
        NeXTPrPoly(verb, ph, qdGlobals().thePort);
}

void Executor::C_donotPrRRect(GrafVerb verb, Rect *r, INTEGER width,
                              INTEGER height)
{
}

void Executor::C_PrRRect(GrafVerb verb, Rect *r, INTEGER width, INTEGER height)
{
    if(pageno >= pagewanted && pageno <= lastpagewanted)
        NeXTPrRRect(verb, r, width, height, qdGlobals().thePort);
}

void Executor::C_donotPrRect(GrafVerb v, Rect *rp)
{
}

void Executor::C_PrRect(GrafVerb v, Rect *rp)
{
    if(pageno >= pagewanted && pageno <= lastpagewanted)
        NeXTPrRect(v, rp, qdGlobals().thePort);
}

void Executor::C_donotPrRgn(GrafVerb verb, RgnHandle rgn)
{
}

void Executor::C_PrRgn(GrafVerb verb, RgnHandle rgn)
{
    if(pageno >= pagewanted && pageno <= lastpagewanted)
        NeXTPrRgn(verb, rgn, qdGlobals().thePort);
}

INTEGER Executor::C_PrTxMeas(INTEGER n, Ptr p, GUEST<Point> *nump,
                             GUEST<Point> *denp, FontInfo *finfop)
{
    StdTxMeas(n, p, nump, denp, finfop);
    return NeXTPrTxMeas(n, p, nump, denp,
                        finfop, qdGlobals().thePort);
}

void Executor::C_donotPrText(INTEGER n, Ptr textbufp, Point num, Point den)
{
}

void Executor::C_PrText(INTEGER n, Ptr textbufp, Point num, Point den)
{
    if(pageno >= pagewanted && pageno <= lastpagewanted)
    {
        NeXTPrText(n, textbufp, num, den, qdGlobals().thePort);
    }
}

static QDProcs prprocs;
static QDProcs sendpsprocs;
static bool need_restore;

void Executor::C_PrComment(INTEGER kind, INTEGER size, Handle hand)
{
    SignedByte state;
    GUEST<INTEGER> *ip;
    INTEGER flippage, angle;
    GUEST<Fixed> *fp;
    Fixed yoffset, xoffset;

    if(pageno >= pagewanted && pageno <= lastpagewanted)
    {
        switch(kind)
        {
            case textbegin:
                do_textbegin((TTxtPicHdl)hand);
                break;
            case textend:
                do_textend();
                break;
            case textcenter:
                do_textcenter((TCenterRecHdl)hand);
                break;
            case rotatebegin:
                ip = (GUEST<INTEGER> *)*hand;
                flippage = ip[0];
                angle = ip[1];
                ROMlib_rotatebegin(flippage, angle);
                break;
            case rotatecenter:
                fp = (GUEST<Fixed> *)*hand;
                yoffset = fp[0];
                xoffset = fp[1];
                ROMlib_rotatecenter(qdGlobals().thePort->pnLoc.v + (double)yoffset / (1L << 16),
                                    qdGlobals().thePort->pnLoc.h + (double)xoffset / (1L << 16));
                break;
            case rotateend:
                ROMlib_rotateend();
                break;
            case psbeginnosave:
                need_restore = false;
            /* FALL THROUGH */
            case postscriptbegin:
                if(ROMlib_passpostscript)
                {
                    if(kind == postscriptbegin)
                    {
                        ROMlib_gsave();
                        need_restore = true;
                    }
                    qdGlobals().thePort->grafProcs = &sendpsprocs;
                }
                break;
            case postscriptend:
                if(ROMlib_passpostscript)
                {
                    if(need_restore)
                    {
                        ROMlib_grestore();
                        need_restore = false;
                    }
                    qdGlobals().thePort->grafProcs = &prprocs;
                }
                break;
            case postscripttextis:
                if(ROMlib_passpostscript)
                    (PORT_GRAF_PROCS(qdGlobals().thePort))->textProc
                        = &textasPS;
                break;
            case postscripthandle:
                if(pageno >= pagewanted && pageno <= lastpagewanted && ROMlib_passpostscript)
                {
                    state = HGetState(hand);
                    HLock(hand);
                    NeXTsendps(size, *hand);
                    HSetState(hand, state);
                }
                break;
            case postscriptfile:
            default:
                warning_unimplemented("kind = %d", kind);
                break;
        }
    }
}

static bool printport_open_p = false;

static void
ourinit(TPPrPort port, BOOLEAN preserve_font)
{
    GUEST<INTEGER> saved_font = port->gPort.txFont;

    printer_init();
    update_printing_globals();

    if(!printport_open_p)
    {
        OpenPort((GrafPtr)&printport);
        printport_open_p = true;
    }
    else
        InitPort((GrafPtr)&printport);
    printport.txFont = -32768; /* force reload */
    printport.pnLoc.h = -32768;
    printport.pnLoc.v = -32768;
    OpenPort(&port->gPort);
    sendpsprocs.textProc = &donotPrText;
    sendpsprocs.lineProc = &donotPrLine;
    sendpsprocs.rectProc = &donotPrRect;
    sendpsprocs.rRectProc = &donotPrRRect;
    sendpsprocs.ovalProc = &donotPrOval;
    sendpsprocs.arcProc = &donotPrArc;
    sendpsprocs.polyProc = &donotPrPoly;
    sendpsprocs.rgnProc = &donotPrRgn;
    sendpsprocs.bitsProc = &donotPrBits;
    sendpsprocs.commentProc = &PrComment;
    sendpsprocs.txMeasProc = &PrTxMeas;
#if 0
  sendpsprocs.getPicProc = &donotPrGetPic;
  sendpsprocs.putPicProc = &donotPrPutPic;
#else
    sendpsprocs.getPicProc = &StdGetPic;
    sendpsprocs.putPicProc = &StdPutPic;
#endif
    prprocs.textProc = &PrText;
    prprocs.lineProc = &PrLine;
    prprocs.rectProc = &PrRect;
    prprocs.rRectProc = &PrRRect;
    prprocs.ovalProc = &PrOval;
    prprocs.arcProc = &PrArc;
    prprocs.polyProc = &PrPoly;
    prprocs.rgnProc = &PrRgn;
    prprocs.bitsProc = &PrBits;
    prprocs.commentProc = &PrComment;
    prprocs.txMeasProc = &PrTxMeas;
#if 0
  prprocs.getPicProc = &PrGetPic;
  prprocs.putPicProc = &PrPutPic;
#else
    prprocs.getPicProc = &StdGetPic;
    prprocs.putPicProc = &StdPutPic;
#endif
    port->saveprocs = prprocs;
#if 1
    port->gPort.device = 1;
#endif
    port->gPort.grafProcs = &port->saveprocs;

    SetRect(&port->gPort.portBits.bounds,
            -1 * ROMlib_resolution_x / 2,
            -1 * ROMlib_resolution_y / 2,
            ROMlib_paper_x - (ROMlib_resolution_x / 2),
            ROMlib_paper_y - (ROMlib_resolution_y / 2));
    SetRect(&port->gPort.portRect, 0, 0,
            ROMlib_paper_x - 72, ROMlib_paper_y - 72);
    (*port->gPort.visRgn)->rgnBBox = port->gPort.portRect;

    if(preserve_font)
        port->gPort.txFont = saved_font;

    NeXTOpenPage();
}

#include <stdio.h>

static bool need_pclose;

#if defined(LINUX)
static void (*old_pipe_signal)(int);
#endif

#if !defined(MACOSX_)
static FILE *
open_ps_file(bool *need_pclosep)
{
    FILE *retval;

    retval = nullptr;
    *need_pclosep = false;

#if defined(LINUX)
    if(ROMlib_printer != "PostScript File")
    {
        value_t prog;

        prog = find_key("Printer", ROMlib_printer);
        if(prog != "")
            retval = popen(prog.c_str(), "w");
        if(retval)
        {
            old_pipe_signal = signal(SIGPIPE, SIG_IGN);
            *need_pclosep = true;
        }
    }
#endif

#if defined(MSDOS) || defined(CYGWIN32)
    if(ROMlib_printer == "Direct To Port")
    {
        value_t port;

        port = find_key("Port", ROMlib_port);
        if(port != "")
            retval = fopen(port.c_str(), "w");
    }
#endif

    /* 
   * Under DOS we don't print to a pipe, so we open ROMlib_spool_file
   * and then through that file to GhostScript.
   */

    if(!retval)
    {
#if !defined(MSDOS) && !defined(CYGWIN32)
        if(ROMlib_printer == "PostScript File")
#endif
        {
            if(!ROMlib_spool_file)
            {
                warning_unexpected(NULL_STRING);
                retval = nullptr;
            }
            else
            {
                retval = Ufopen(ROMlib_spool_file, "w");
#if !defined(MSDOS) && !defined(CYGWIN32)
                free(ROMlib_spool_file);
                ROMlib_spool_file = nullptr;
#endif
            }
        }
    }
    return retval;
}
#endif /* !MACOSX_ */

static bool already_open = false;

#if defined(QUESTIONABLE_FIX_FOR_LOGBOOK_THAT_BREAKS_PRINTING_UNDER_TESTGEN)
static Byte save_FractEnable;
#endif

#if defined(CYGWIN32)
static THPrint last_thprint;
static uint32_t job_dialog_count;
static uint32_t job_dialog_desired = 1;
#endif

static void
call_job_dialog_if_needed(THPrint thprint)
{
#if defined(CYGWIN32)
    if(thprint != last_thprint || job_dialog_count < job_dialog_desired)
    {
        PrJobDialog(thprint);
        last_thprint = thprint;
        job_dialog_count = job_dialog_desired;
    }
    ++job_dialog_desired;
#endif
}

void
Executor::ROMlib_acknowledge_job_dialog(THPrint thprint)
{
#if defined(CYGWIN32)
    last_thprint = thprint;
    ++job_dialog_count;
#endif
}

TPPrPort Executor::C_PrOpenDoc(THPrint hPrint, TPPrPort port, Ptr pIOBuf)
{
    call_job_dialog_if_needed(hPrint);
    if(port)
    {
        port->fOurPtr = false;
    }
    else
    {
        port = (TPPrPort)NewPtr((Size)sizeof(TPrPort));
        port->fOurPtr = true;
    }
    ourinit(port, false);

#if !defined(MACOSX_)
    pagewanted = (*hPrint)->prJob.iFstPage;
    lastpagewanted = (*hPrint)->prJob.iLstPage;
#else
    lastpagewanted = 9999;
#endif

    if(!already_open)
    {
#if defined(QUESTIONABLE_FIX_FOR_LOGBOOK_THAT_BREAKS_PRINTING_UNDER_TESTGEN)
        save_FractEnable = LM(FractEnable);
        LM(FractEnable) = 0xff;
#endif

        if(!ROMlib_printfile)
            ROMlib_printfile = open_ps_file(&need_pclose);

        if(ROMlib_printfile)
        {
            Handle h;
            Ptr p;
            int len;

            h = GetResource(TICK("PREC"), 103);
            if(h)
            {
                p = *h;
                len = GetHandleSize(h);
            }
            else
            {
                p = nullptr;
                len = 0;
            }
            fprintf(ROMlib_printfile, ROMlib_doc_begin,
                    ROMlib_document_paper_sizes.c_str(), ROMlib_paper_size_name.c_str(),
                    ROMlib_paper_size_name_terminator.c_str(),
                    ROMlib_paper_orientation.c_str());
            fprintf(ROMlib_printfile, "%s", ROMlib_doc_prolog);
            fprintf(ROMlib_printfile, ROMlib_doc_end_prolog,
                    ROMlib_paper_size.c_str(), ROMlib_paper_size_name.c_str(),
                    ROMlib_paper_size_name_terminator.c_str(),
                    (*hPrint)->prJob.iCopies, len, p);
        }
        pageno = 0;
    }

    already_open = true;
    return port;
}

void Executor::C_PrOpenPage(TPPrPort port, TPRect pPageFrame)
{
    ++pageno;
    ourinit(port, true);
    if(pageno >= pagewanted && pageno <= lastpagewanted)
    {
        if(ROMlib_printfile)
            fprintf(ROMlib_printfile, ROMlib_page_begin, pageno - pagewanted + 1,
                    pageno - pagewanted + 1,
                    ROMlib_paper_x, ROMlib_paper_y, ROMlib_paper_size.c_str(),
                    ROMlib_paper_size_name.c_str(), ROMlib_paper_size_name_terminator.c_str(),
                    ROMlib_rotation, ROMlib_translate_x, ROMlib_translate_y,
                    72.0 / ROMlib_resolution_x, -1 * 72.0 / ROMlib_resolution_y);
        ROMlib_suppressclip = false;
    }
    page_is_open = true;
}

void Executor::C_PrClosePage(TPPrPort pPrPort)
{
    if(page_is_open)
    {
        if(pageno >= pagewanted && pageno <= lastpagewanted)
        {
            if(ROMlib_printfile)
                fprintf(ROMlib_printfile, ROMlib_page_end,
                        -55 + (ROMlib_rotation ? 30 : 0));
        }
    }
    page_is_open = false;
}

void Executor::C_PrCloseDoc(TPPrPort port)
{
    if(ROMlib_printfile)
    {
        fprintf(ROMlib_printfile, ROMlib_doc_end, pageno - pagewanted + 1,
                ROMlib_paper_x, ROMlib_paper_y);
        fflush(ROMlib_printfile);
    }

    if(need_pclose)
    {
#if !defined(CYGWIN32) && !defined(_MSC_VER)
        pclose(ROMlib_printfile);
#else
        ; /* CYGWIN32 has no pclose */
#endif
#if defined(LINUX)
        signal(SIGPIPE, old_pipe_signal);
#endif
    }
    else if(ROMlib_printfile)
    {
        fclose(ROMlib_printfile);
#if defined(CYGWIN32)
        warning_trace_info("ROMlib_printer = %s, WIN32_TOKEN = %s",
                           ROMlib_printer, WIN32_TOKEN);
        if(strcmp(ROMlib_printer, WIN32_TOKEN) == 0)
        {
            print_file(ROMlib_wp, ROMlib_spool_file, nullptr);
        }
#endif
    }
    ROMlib_printfile = 0;

    if(port->fOurPtr)
        DisposePtr((Ptr)port);
#if defined(QUESTIONABLE_FIX_FOR_LOGBOOK_THAT_BREAKS_PRINTING_UNDER_TESTGEN)
    LM(FractEnable) = save_FractEnable;
#endif
    already_open = false;

    if(printport_open_p)
    {
        ClosePort((GrafPtr)&printport);
        printport_open_p = false;
    }
}

void Executor::C_PrPicFile(THPrint hPrint, TPPrPort pPrPort, Ptr pIOBuf,
                           Ptr pDevBuf, TPrStatus *prStatus) /* TODO */
{
    warning_unimplemented(NULL_STRING);
}

void
Executor::print_reinit(void)
{
    printport_open_p = false;
}
