# Moon Age Calculation Review

## Purpose

This note records the moon-age precision issue, the chosen display meaning, and
the firmware correction.

Before this correction, `src/clock/clock_time.cpp` computed moon age from one
fixed new-moon epoch and one fixed synodic-month length:

- reference new moon: 2000-01-07 03:14 JST
- synodic month: 42,524 minutes = 29 days 12 hours 44 minutes

This is a reasonable lightweight approximation, but it is not accurate enough
to support a moon-age display rounded to 0.1 day over the full RTC date range.

## Reproducible discrepancy

A useful check case is 2026-09-25 in Japan.

The previous fixed-period algorithm implied the preceding new moon occurred at:

- 2026-09-12 05:14 JST

Using the RTC time 2026-09-25 09:57 JST, that gives approximately:

- previous calculation: **13.2 days**

However, the National Astronomical Observatory of Japan (NAOJ) gives the actual
new moon for this lunation as:

- 2026-09-11 12:27 JST

Elapsed time from that new moon to 2026-09-25 09:57 JST is approximately:

- astronomical moon age: **13.9 days**

NAOJ's daily Tokyo table gives the moon age at local noon on 2026-09-25 as:

- noon moon age: **14.0 days**

The previous calculation was therefore about **0.7 day low** in this case. The
difference is large enough to be visible even though the UI only shows one
decimal place.

## Why the fixed-period calculation drifted

The old issue was not integer rounding or the nominal average month length
itself. The implementation assumed every lunation had exactly the same length.

Real synodic months vary because the Moon and Earth do not move at constant
angular speed in circular orbits. A fixed-period model therefore moved the
predicted new-moon time away from the real new-moon time by several hours, and
occasionally much more.

For the 2026-09-25 example:

- predicted preceding new moon: 2026-09-12 05:14 JST
- actual preceding new moon: 2026-09-11 12:27 JST
- new-moon timing error: **16 hours 47 minutes**

That timing error directly appears as approximately 0.7 day of moon-age error.

Changing only the fixed month constant was not a robust solution. It could
reduce the error for one part of the calendar while increasing it elsewhere.

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

## Implemented correction

The display uses **Option A: astronomical moon age at the current RTC time**.
Moon age is elapsed time since the preceding new-moon instant, rounded to
0.1 day. It is not the traditional lunar-calendar day number or an almanac value
fixed at local noon.

`src/clock/clock_time.cpp` calculates new-moon instants using the compact
periodic-correction formula in Jean Meeus, *Astronomical Algorithms*, 2nd ed.,
Chapter 49. The result is in Terrestrial Time (TT). The firmware converts it to
Universal Time (UT1) using the Espenak/Meeus ΔT polynomials for 2000–2099, then
compares it with the RTC timestamp converted from JST to UTC (JST = UTC+9). The
sub-second UT1–UTC difference is ignored. This avoids a stored century-long
phase table.

The event search checks neighboring lunation indices around the mean-month
estimate and selects the latest calculated new moon not later than the RTC time.
The age calculation includes RTC seconds and rounds to the nearest tenth of a
day. Actual lunations can exceed the nominal 29.5-day age, so the display keeps
values such as `29.8` before the next new moon when appropriate.

## Validation

The calculation was compared with published phase times, the RTC range edges,
and display rounding boundaries:

- January 2000, March 2024, and September 2026 NAOJ phase times match within 20
  seconds when the published minutes are treated as minute-resolution values.
- The NAOJ example at 2026-09-25 09:57 JST displays **13.9 days**, from the
  2026-09-11 12:27 JST new moon.
- The USNO phase table lists the 2099-12-11 new moon at 23:10 UT; the calculation
  gives 23:08:49 UT. At the RTC range endpoints, the displayed ages are 23.7
  days on 2000-01-01 00:00 JST and 19.7 days on 2099-12-31 23:59 JST.
- A longer 2000 lunation displays 29.8 days before its next new moon. Around a
  0.05-day rounding boundary, adjacent RTC seconds produce 0.0 and 0.1 days as
  expected.

RTC local timestamps are interpreted as JST (UTC+9), matching NAOJ's phase
table time basis. A small host harness exercised the C++ implementation; the
Pico firmware also builds successfully.

## References

- Meeus, Jean. *Astronomical Algorithms*, 2nd ed., Chapter 49.
- National Astronomical Observatory of Japan, 2026 phases of the Moon:
  https://eco.mtk.nao.ac.jp/koyomi/yoko/2026/rekiyou263.html
- National Astronomical Observatory of Japan, 2000 phases of the Moon:
  https://eco.mtk.nao.ac.jp/koyomi/yoko/pdf/yoko2000.pdf
- National Astronomical Observatory of Japan, 2024 phases of the Moon:
  https://eco.mtk.nao.ac.jp/koyomi/yoko/2024/rekiyou243.html
- NASA GSFC, polynomial expressions for ΔT adapted from Espenak and Meeus:
  https://eclipse.gsfc.nasa.gov/SEhelp/deltatpoly2004.html
- U.S. Naval Observatory, 2099 primary phases (times in UT):
  https://aa.usno.navy.mil/api/moon/phases/year?year=2099
- U.S. Naval Observatory, December 1999 primary phases (range start check):
  https://aa.usno.navy.mil/api/moon/phases/date?date=1999-12-1&nump=2
- National Astronomical Observatory of Japan, Tokyo Moon rise/set and noon moon
  age for September 2026:
  https://eco.mtk.nao.ac.jp/koyomi/dni/2026/m1309.html
- National Astronomical Observatory of Japan, Mid-Autumn Moon, September 2026:
  https://www.nao.ac.jp/astro/sky/2026/09-topics03.html
