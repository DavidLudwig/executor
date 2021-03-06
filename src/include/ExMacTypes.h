#if !defined(_MACTYPES_H_)
#define _MACTYPES_H_

/*
 * Copyright 1986, 1989, 1990, 1995 by Abacus Research and Development, Inc.
 * All rights reserved.
 *

 */

#include <base/mactype.h>
#include <base/byteswap.h>

namespace Executor
{
typedef int16_t INTEGER;
typedef int32_t LONGINT;
typedef uint32_t ULONGINT;
typedef int8_t BOOLEAN;

typedef int16_t CharParameter; /* very important not to use this as char */

typedef int8_t SignedByte;
typedef uint8_t Byte;
typedef int8_t *Ptr;
typedef GUEST<Ptr> *Handle;

typedef int8_t Boolean;

typedef Byte Str15[16];
typedef Byte Str31[32];
typedef Byte Str32[33];
typedef Byte Str63[64];
typedef Byte Str255[256];
typedef Byte *StringPtr;
typedef const unsigned char *ConstStringPtr;

typedef GUEST<StringPtr> *StringHandle;


typedef LONGINT Fixed, Fract;

/* SmallFract represnts values between 0 and 65535 */
typedef unsigned short SmallFract;

enum
{
    MaxSmallFract = 0xFFFF,
};

typedef double Extended;

typedef LONGINT Size;

typedef INTEGER OSErr;
typedef LONGINT OSType;
typedef LONGINT ResType;

class OSErrorException : public std::runtime_error
{
public:
    OSErr code;

    OSErrorException(OSErr err) : std::runtime_error("oserror"), code(err) {}
};

struct QHdr
{
    GUEST_STRUCT;
    GUEST<INTEGER> qFlags;
    GUEST<union __qe *> qHead; /* actually QElemPtr */
    GUEST<union __qe *> qTail; /* actually QElemPtr */
};
typedef QHdr *QHdrPtr;
typedef union __qe *QElemPtr;

enum
{
    noErr = 0,
    paramErr = (-50),
};

#if 0
// custom case in mactype.h
/* from Quickdraw.h */
struct Point { GUEST_STRUCT;
    GUEST< INTEGER> v;
    GUEST< INTEGER> h;
};

struct NativePoint {
    INTEGER v;
    INTEGER h;
};
#endif

#define ZEROPOINT(p) (p.v = 0, p.h = 0)

struct Rect
{
    GUEST_STRUCT;
    GUEST<INTEGER> top;
    GUEST<INTEGER> left;
    GUEST<INTEGER> bottom;
    GUEST<INTEGER> right;

    Rect() = default;
    Rect(GUEST<INTEGER> t, GUEST<INTEGER> l, GUEST<INTEGER> b, GUEST<INTEGER> r)
        : top(t)
        , left(l)
        , bottom(b)
        , right(r)
    {
    }
};

typedef Rect *RectPtr;

inline short RECT_WIDTH(const Rect *r)
{
    return r->right - r->left;
}
inline short RECT_HEIGHT(const Rect *r)
{
    return r->bottom - r->top;
}

#define RECT_ZERO(r)                \
    do                              \
        memset(r, 0, sizeof(Rect)); \
    while(false)

inline bool RECT_EQUAL_P(const Rect *r1, const Rect *r2)
{
    const uint32_t *__p1 = (const uint32_t *)(r1);
    const uint32_t *__p2 = (const uint32_t *)(r2);
    return __p1[0] == __p2[0] && __p1[1] == __p2[1];
}

/* from IntlUtil.h */
typedef INTEGER ScriptCode;
typedef INTEGER LangCode;

static_assert(sizeof(QHdr) == 10);
static_assert(sizeof(Point) == 4);
static_assert(sizeof(Rect) == 8);
}
#endif /* _MACTYPES_H_ */
