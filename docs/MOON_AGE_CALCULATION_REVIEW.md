# Moon Age Calculation Review

## Purpose

This note records a precision issue in the current moon-age display and gives
guidance for a future correction. It intentionally does **not** change firmware
code.

The current implementation in `src/clock/clock_time.cpp` computes moon age
from one fixed new-moon epoch and one fixed synodic-month length:

- reference new moon: 2000-01-07 03:14 JST
- synodic month: 42,524 minutes = 29 days 12 hours 44 minutes

This is a reasonable lightweight approximation, but it is not accurate enough
to support a moon-age display rounded to 0.1 day over the full RTC date range.

## Reproducible discrepancy

A useful check case is 2026-09-25 in Japan.

The current algorithm implies the preceding new moon occurred at:

- 2026-09-12 05:14 JST

Using the RTC time 2026-09-25 09:57 JST, that gives approximately:

- current implementation: **13.2 days**

However, the National Astronomical Observatory of Japan (NAOJ) gives the actual
new moon for this lunation as:

- 2026-09-11 12:27 JST

Elapsed time from that new moon to 2026-09-25 09:57 JST is approximately:

- astronomical moon age: **13.9 days**

NAOJ's daily Tokyo table gives the moon age at local noon on 2026-09-25 as:

- noon moon age: **14.0 days**

The current calculation is therefore about **0.7 day low** in this case. The
difference is large enough to be visible even though the UI only shows one
decimal place.

## Why the current calculation drifts

The main issue is not integer rounding and not the nominal average month length
itself. The implementation assumes every lunation has exactly the same length.

Real synodic months vary because the Moon and Earth do not move at constant
angular speed in circular orbits. A fixed-period model therefore moves the
predicted new-moon time away from the real new-moon time by several hours, and
occasionally much more.

For the 2026-09-25 example:

- predicted preceding new moon: 2026-09-12 05:14 JST
- actual preceding new moon: 2026-09-11 12:27 JST
- new-moon timing error: **16 hours 47 minutes**

That timing error directly appears as approximately 0.7 day of moon-age error.

Changing only the fixed month constant is not a robust solution. It might reduce
the error for one part of the calendar while increasing it elsewhere.

## Moon age versus "15th night"

Care is also needed when comparing the displayed number with traditional
calendar terminology.

NAOJ identifies 2026-09-25 as the **Mid-Autumn Moon (中秋の名月)**, corresponding
to the 15th day of the eighth month in the traditional lunisolar calendar.
That does **not** mean the astronomical moon age is 15.0 days.

Moon age is elapsed time since the instant of new moon, with new moon defined as
age 0.0. Therefore the traditional lunar-calendar day number and astronomical
moon age normally differ by roughly one day and should not be treated as the
same value.

For this date:

- traditional calendar context: 15th night / 中秋の名月
- astronomical moon age around 09:57 JST: about 13.9 days
- NAOJ noon moon age: 14.0 days

## Recommended future direction

Before changing the implementation, first decide which quantity the UI is
intended to show.

### Option A: Astronomical moon age at the current RTC time

This is the most natural interpretation of the existing decimal-day display.
Determine the actual or sufficiently accurate preceding new-moon instant, then
display elapsed time from that instant.

For a firmware implementation, use either:

1. a compact astronomical new-moon calculation with bounded error across the
   supported 2000-2099 range; or
2. a small table of new-moon event times covering the supported date range.

A table is simple to validate and predictable on an embedded target. A
calculation avoids stored event data but requires careful validation.

If the display remains at 0.1-day resolution, the implementation should aim for
substantially better than 0.1-day timing accuracy. Ideally, the error should be
below about 0.05 day (72 minutes) so rounding to one decimal place is stable
except very near a display boundary.

### Option B: Almanac-style daily moon age

If the intention is to match Japanese calendar/almanac values such as NAOJ's
daily tables, define the displayed value at a fixed reference time, for example
local noon.

This is a different UI semantic from continuously updating astronomical age.
The choice should be documented explicitly so users know why a morning value
may differ slightly from the published noon value.

### Option C: Keep the current lightweight approximation

If code size and simplicity are more important than astronomical accuracy, the
current method can remain, but the UI/documentation should identify it as an
approximate moon age. Showing one decimal place may otherwise imply more
precision than the algorithm provides.

## Validation advice

A future replacement should be checked against authoritative phase data rather
than only against a few hand-selected dates.

At minimum, validate:

- several new moons across different years;
- dates near the start and end of the supported 2000-2099 RTC range;
- cases where the current fixed-period approximation is known to be far from
  the real new-moon time;
- rounding boundaries around each 0.1-day display transition;
- local-time handling, especially that the RTC and reference phase times use
  the same time basis.

A useful acceptance test is to compare the calculated preceding new-moon time
and resulting moon age against NAOJ data for a representative set of dates.

## References

- National Astronomical Observatory of Japan, 2026 phases of the Moon:
  https://eco.mtk.nao.ac.jp/koyomi/yoko/2026/rekiyou263.html
- National Astronomical Observatory of Japan, Tokyo Moon rise/set and noon moon
  age for September 2026:
  https://eco.mtk.nao.ac.jp/koyomi/dni/2026/m1309.html
- National Astronomical Observatory of Japan, Mid-Autumn Moon, September 2026:
  https://www.nao.ac.jp/astro/sky/2026/09-topics03.html

## Scope of this note

This document is advisory only. No source-code behavior is changed by adding
this note.
