#include <cassert>
#include <cstdio>

#include "AlertLogic.h"

using namespace terrawatch::acoustic;

int main()
{
    const float quiet[6] = {0.01f, 0.01f, 0.05f, 0.05f, 0.05f, 0.83f};
    const float saw[6] = {0.60f, 0.01f, 0.05f, 0.05f, 0.05f, 0.24f};
    const float bang[6] = {0.01f, 0.97f, 0.01f, 0.01f, 0.01f, 0.01f};
    {   // one chainsaw window alone is not an alert; a second one within 60 s is, and then it stays quiet for 60 s
        AlertLogic l(0.18f, 0.93f); std::uint32_t t = 1000;
        assert(l.update(quiet, t).kind == AlertKind::None);
        assert(l.update(saw, t += 5000).kind == AlertKind::None);
        const Alert a = l.update(saw, t += 5000); assert(a.kind == AlertKind::Chainsaw && a.confidencePercent == 60);
        assert(l.update(saw, t += 5000).kind == AlertKind::None);
        assert(l.update(saw, t += 55000).kind == AlertKind::Chainsaw);            // 60 s after the first alert (at 11 s), two hits still within 60 s
    }
    {   // fire: needs two windows above its threshold within 60 s, then stays quiet for 10 minutes; off when the threshold is above 1
        const float fire[6] = {0.02f, 0.01f, 0.95f, 0.01f, 0.01f, 0.00f};
        AlertLogic l(0.18f, 0.93f, 0.90f); std::uint32_t t = 1000;
        assert(l.update(fire, t).kind == AlertKind::None);
        const Alert a = l.update(fire, t += 4000); assert(a.kind == AlertKind::Fire && a.confidencePercent == 95);
        assert(l.update(fire, t += 4000).kind == AlertKind::None);
        assert(l.update(fire, t += 300000).kind == AlertKind::None);
        assert(l.update(fire, t += 400000).kind == AlertKind::None);               // the first hit of a new pair
        assert(l.update(fire, t += 4000).kind == AlertKind::Fire);
        AlertLogic off(0.18f, 0.93f); std::uint32_t u = 0;
        assert(off.update(fire, u).kind == AlertKind::None && off.update(fire, u += 3000).kind == AlertKind::None);
    }
    {   // hits more than 60 s apart do not add up
        AlertLogic l(0.18f, 0.93f); std::uint32_t t = 0;
        assert(l.update(saw, t).kind == AlertKind::None);
        assert(l.update(quiet, t += 30000).kind == AlertKind::None);
        assert(l.update(saw, t += 40000).kind == AlertKind::None);
    }
    {   // a blast is an alert at once, repeated blasts within 10 s are not
        AlertLogic l(0.18f, 0.93f); std::uint32_t t = 500;
        assert(l.update(bang, t).kind == AlertKind::Blast);
        assert(l.update(bang, t += 3000).kind == AlertKind::None);
        assert(l.update(bang, t += 8000).kind == AlertKind::Blast);
    }
    {   // the millisecond clock wraps without breaking the 60 s window
        AlertLogic l(0.18f, 0.93f); std::uint32_t t = 0xFFFFFFFFu - 4000u;
        assert(l.update(saw, t).kind == AlertKind::None);
        assert(l.update(saw, t += 6000u).kind == AlertKind::Chainsaw);
    }
    std::puts("acoustic alert logic: all tests passed");
    return 0;
}
