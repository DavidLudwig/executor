/* Copyright 1986-1995 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

/* Forward declarations in QuickDraw.h (DO NOT DELETE THIS LINE) */

#include <base/common.h>
#include <QuickDraw.h>
#include <CQuickDraw.h>
#include <ToolboxEvent.h>
#include <ToolboxUtil.h>

#include <quickdraw/quick.h>
#include <quickdraw/cquick.h>
#include <quickdraw/region.h>

using namespace Executor;

static BOOLEAN EquivRect(Rect *, Rect *);
static INTEGER fromhex(char c);

static BOOLEAN EquivRect(Rect *rp1, Rect *rp2)
{
    return rp1->bottom - rp1->top == rp2->bottom - rp2->top && rp1->right - rp1->left == rp2->right - rp2->left;
}

#define RANDSEED ((ULONGINT)randSeed)
INTEGER Executor::C_Random()
{
    INTEGER retval;

        // FIXME: what is going on here?
    LM(RndSeed) = TickCount(); /* what better? */

    int32_t randSeed = qdGlobals().randSeed;
    if(randSeed >= 0x80000000)
        randSeed = (randSeed & 0x7FFFFFFF) + 1;
    randSeed = (randSeed * 16807 + ((((randSeed >> 14) * 16807) + (((randSeed & ((1 << 14) - 1)) * 16807) >> 14))
                                        >> 17))
                   & 0x7FFFFFFF;
    if(randSeed == 0x7FFFFFFF)
        randSeed = 0;
    qdGlobals().randSeed = randSeed;
    retval = randSeed;
    return retval == -32768 ? 0 : retval;
}

BOOLEAN Executor::C_GetPixel(INTEGER h, INTEGER v)
{
    BitMap temp_bm;
    unsigned char temp_fbuf[4];
    Rect src_rect, dst_rect;

    temp_bm.baseAddr = (Ptr)temp_fbuf;
    temp_bm.bounds.top = 0;
    temp_bm.bounds.bottom = 1;
    temp_bm.bounds.left = 0;
    temp_bm.bounds.right = 1;
    temp_bm.rowBytes = 4;

    src_rect.top = v;
    src_rect.bottom = v + 1;
    src_rect.left = h;
    src_rect.right = h + 1;

    dst_rect = temp_bm.bounds;

    CopyBits(PORT_BITS_FOR_COPY(qdGlobals().thePort), &temp_bm,
             &src_rect, &dst_rect, srcCopy, nullptr);

    return (*temp_fbuf & 0x80) != 0;
}

/* fromhex: converts from '0'-'9' to 0-9, 'a-z' and 'A-Z' similarly */

static INTEGER fromhex(char c)
{
    if(c >= '0' && c <= '9')
        return (c - '0');
    else if(c >= 'A' && c <= 'Z')
        return (c - 'A' + 10);
    else
        return (c - 'a' + 10);
}

void Executor::C_StuffHex(Ptr p, StringPtr s)
{
    char *sp, *ep;
    unsigned len;

    len = s[0];
    sp = (char *)s + 1;
    ep = sp + (len & ~1);
    for(; sp != ep; sp += 2)
        *p++ = (fromhex(*sp) << 4) | fromhex(sp[1]);

    if(len & 1)
        *p = (*p & 0xF) | (fromhex(*sp) << 4);
}

void Executor::C_ScalePt(GUEST<Point> *pt, Rect *srcr, Rect *dstr)
{
    INTEGER srcdh, srcdv, dstdh, dstdv;

    if(pt->h || pt->v)
    {
        srcdh = srcr->right - srcr->left;
        srcdv = srcr->bottom - srcr->top;
        dstdh = dstr->right - dstr->left;
        dstdv = dstr->bottom - dstr->top;

        pt->h = ((((LONGINT)pt->h * dstdh) << 1) / srcdh + 1) >> 1;
        pt->v = ((((LONGINT)pt->v * dstdv) << 1) / srcdv + 1) >> 1;

        if(pt->v < 1)
            pt->v = 1;
        if(pt->h < 1)
            pt->h = 1;
    }
}

void Executor::C_MapPt(GUEST<Point> *pt, Rect *srcr, Rect *dstr)
{
    INTEGER srcdh, srcdv, dstdh, dstdv;

    srcdh = srcr->right - srcr->left;
    srcdv = srcr->bottom - srcr->top;
    dstdh = dstr->right - dstr->left;
    dstdv = dstr->bottom - dstr->top;

    pt->h = pt->h - (srcr->left);
    pt->v = pt->v - (srcr->top);
    pt->h = (LONGINT)pt->h * dstdh / srcdh;
    pt->v = (LONGINT)pt->v * dstdv / srcdv;
    pt->h = pt->h + (dstr->left);
    pt->v = pt->v + (dstr->top);
}

void Executor::C_MapRect(Rect *r, Rect *srcr, Rect *dstr)
{
    MapPt((GUEST<Point> *)&r->top, srcr, dstr);
    MapPt((GUEST<Point> *)&r->bottom, srcr, dstr);
}

#define IMPOSSIBLE -1
#define MAPH(x) (x = UNFIX((x - xoff1) * xcoff) + xoff2)
#define MAPV(y) (y = UNFIX((y - yoff1) * ycoff) + yoff2)
#define UNFIX(x) ((x) >> 16)

void Executor::C_MapRgn(RgnHandle rh, Rect *srcr, Rect *dstr)
{
    GUEST<INTEGER> *ip, *op, *saveop;
    INTEGER oldv, newv, *tempp, srcdh, dstdh, srcdv, dstdv;
    INTEGER xoff1, xoff2, yoff1, yoff2, x1, x2;
    Fixed xcoff, ycoff;
    INTEGER buf1[1000], buf2[1000], *mergebuf, *freebuf, *ipe;
    LONGINT hold;
    BOOLEAN done;

    if(EquivRect(srcr, dstr))
        OffsetRgn(rh, dstr->left - srcr->left,
                  dstr->top - srcr->top);
    else
    {
        if(RGN_SMALL_P(rh))
            MapRect(&(*rh)->rgnBBox, srcr, dstr);
        else
        {
            srcdh = srcr->right - (xoff1 = srcr->left);
            dstdh = dstr->right - (xoff2 = dstr->left);
            srcdv = srcr->bottom - (yoff1 = srcr->top);
            dstdv = dstr->bottom - (yoff2 = dstr->top);
            xcoff = FixRatio(dstdh, srcdh);
            ycoff = FixRatio(dstdv, srcdv);
            ip = op = (GUEST<INTEGER> *)*rh + 5;
            oldv = -32768;
            buf1[0] = 32767;
            mergebuf = buf1;
            freebuf = buf2;
            do
            {
                done = (newv = *ip++) == 32767;
                MAPV(newv);
                if(newv != oldv || done)
                {
                    if(mergebuf[0] != 32767)
                    {
                        *op++ = oldv;
                        saveop = op;
                        hold = IMPOSSIBLE;
                        for(tempp = mergebuf; (x1 = *tempp++) != 32767;)
                        {
                            MAPH(x1);
                            if(hold == IMPOSSIBLE)
                                hold = (unsigned short)x1;
                            else if(hold == x1)
                                hold = IMPOSSIBLE;
                            else
                            {
                                *op++ = hold;
                                hold = (unsigned short)x1;
                            }
                        }
                        if(hold != IMPOSSIBLE)
                            *op++ = hold;
                        gui_assert(!((op - saveop) & 1));
                        if(op == saveop)
                            --op;
                        else
                            *op++ = 32767;
                    }
                    gui_assert(op < ip);
                    mergebuf[0] = 32767;
                    oldv = newv;
                }
                if(!done)
                {
                    for(tempp = freebuf, ipe = mergebuf,
                    x1 = *ip++, x2 = *ipe++;
                        ;)
                    {
                        if(x1 < x2)
                        {
                            *tempp++ = x1;
                            x1 = *ip++;
                        }
                        else if(x1 > x2)
                        {
                            *tempp++ = x2;
                            x2 = *ipe++;
                        }
                        else
                        {
                            if(x1 == 32767)
                                /*-->*/ break;
                            x1 = *ip++;
                            x2 = *ipe++;
                        }
                    }
                    gui_assert(!((tempp - freebuf) & 1));
                    *tempp++ = 32767;
                    tempp = freebuf;
                    freebuf = mergebuf;
                    mergebuf = tempp;
                }
            } while(!done);
            *op++ = 32767;
            ROMlib_sizergn(rh, false);
        }
    }
}

void Executor::C_MapPoly(PolyHandle poly, Rect *srcr, Rect *dstr)
{
    GUEST<Point> *ip, *ep;

    if(EquivRect(srcr, dstr))
        OffsetPoly(poly, dstr->left - srcr->left,
                   dstr->top - srcr->top);
    else
    {
        MapPt((GUEST<Point> *)&(*poly)->polyBBox.top, srcr, dstr);
        MapPt((GUEST<Point> *)&(*poly)->polyBBox.bottom, srcr, dstr);
        for(ip = (*poly)->polyPoints,
        ep = ip + ((*poly)->polySize - SMALLPOLY) / sizeof(Point);
            ip != ep;)
            MapPt(ip++, srcr, dstr);
    }
}
