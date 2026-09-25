#include "clock/clock_time.h"

#include <cmath>
#include <cstdio>

bool is_leap_year(int year) {
    return ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
}

uint8_t days_in_month(int year, int month) {
    static constexpr uint8_t kMonthDays[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return kMonthDays[month - 1];
}

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kJd2000Jan1Utc = 2451544.5;
constexpr double kJde2000NewMoon = 2451550.09766;
constexpr double kMeanSynodicMonthDays = 29.530588861;
constexpr double kJstOffsetDays = 9.0 / 24.0;

int days_before_month(int year, int month) {
    static constexpr int kDays[] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    int days = kDays[month - 1];
    if (month > 2 && is_leap_year(year)) {
        ++days;
    }
    return days;
}

double sin_degrees(double degrees) {
    const double reduced_degrees = std::remainder(degrees, 360.0);
    return std::sin(reduced_degrees * (kPi / 180.0));
}

double delta_t_seconds(double decimal_year) {
    if (decimal_year < 2005.0) {
        const double t = decimal_year - 2000.0;
        const double t2 = t * t;
        const double t3 = t2 * t;
        const double t4 = t3 * t;
        const double t5 = t4 * t;
        return 63.86 + 0.3345 * t - 0.060374 * t2 +
               0.0017275 * t3 + 0.000651814 * t4 +
               0.00002373599 * t5;
    }

    if (decimal_year <= 2050.0) {
        const double t = decimal_year - 2000.0;
        return 62.92 + 0.32217 * t + 0.005589 * t * t;
    }

    const double u = (decimal_year - 1820.0) / 100.0;
    return -20.0 + 32.0 * u * u - 0.5628 * (2150.0 - decimal_year);
}

double approximate_decimal_year(double jd) {
    return 2000.0 + (jd - 2451545.0) / 365.2425;
}

double new_moon_jde_tt(int lunation) {
    // Meeus, Astronomical Algorithms, 2nd ed., Chapter 49 (new moon).
    const double k = static_cast<double>(lunation);
    const double t = k / 1236.85;
    const double t2 = t * t;
    const double jde_mean =
        kJde2000NewMoon + kMeanSynodicMonthDays * k +
        (0.00015437 + (-0.00000015 + 0.00000000073 * t) * t) * t2;

    const double eccentricity = 1.0 + (-0.002516 - 0.0000074 * t) * t;
    const double mean_sun_anomaly =
        2.5534 + 29.1053567 * k + (-0.0000014 - 0.00000011 * t) * t2;
    const double mean_moon_anomaly =
        201.5643 + 385.81693528 * k +
        (0.0107582 + (0.00001238 - 0.000000058 * t) * t) * t2;
    const double moon_argument_latitude =
        160.7108 + 390.67050284 * k +
        (-0.0016118 + (-0.00000227 + 0.000000011 * t) * t) * t2;
    const double ascending_node_longitude =
        124.7746 - 1.56375588 * k + (0.0020672 + 0.00000215 * t) * t2;

    const double m = mean_sun_anomaly;
    const double moon_m = mean_moon_anomaly;
    const double f = moon_argument_latitude;
    const double correction =
        -0.40720 * sin_degrees(moon_m) +
        0.17241 * eccentricity * sin_degrees(m) +
        0.01608 * sin_degrees(2.0 * moon_m) +
        0.01039 * sin_degrees(2.0 * f) +
        0.00739 * eccentricity * sin_degrees(moon_m - m) -
        0.00514 * eccentricity * sin_degrees(moon_m + m) +
        0.00208 * eccentricity * eccentricity * sin_degrees(2.0 * m) -
        0.00111 * sin_degrees(moon_m - 2.0 * f) -
        0.00057 * sin_degrees(moon_m + 2.0 * f) +
        0.00056 * eccentricity * sin_degrees(2.0 * moon_m + m) -
        0.00042 * sin_degrees(3.0 * moon_m) +
        0.00042 * eccentricity * sin_degrees(m + 2.0 * f) +
        0.00038 * eccentricity * sin_degrees(m - 2.0 * f) -
        0.00024 * eccentricity * sin_degrees(2.0 * moon_m - m) -
        0.00017 * sin_degrees(ascending_node_longitude) -
        0.00007 * sin_degrees(moon_m + 2.0 * m) +
        0.00004 * sin_degrees(2.0 * (moon_m - f)) +
        0.00004 * sin_degrees(3.0 * m) +
        0.00003 * sin_degrees(moon_m + m - 2.0 * f) +
        0.00003 * sin_degrees(2.0 * (moon_m + f)) -
        0.00003 * sin_degrees(moon_m + m + 2.0 * f) +
        0.00003 * sin_degrees(moon_m - m + 2.0 * f) -
        0.00002 * sin_degrees(moon_m - m - 2.0 * f) -
        0.00002 * sin_degrees(3.0 * moon_m + m) +
        0.00002 * sin_degrees(4.0 * moon_m);

    const double a1 = 299.77 + 0.107408 * k - 0.009173 * t2;
    const double a2 = 251.88 + 0.016321 * k;
    const double a3 = 251.83 + 26.651886 * k;
    const double a4 = 349.42 + 36.412478 * k;
    const double a5 = 84.66 + 18.206239 * k;
    const double a6 = 141.74 + 53.303771 * k;
    const double a7 = 207.14 + 2.453732 * k;
    const double a8 = 154.84 + 7.306860 * k;
    const double a9 = 34.52 + 27.261239 * k;
    const double a10 = 207.19 + 0.121824 * k;
    const double a11 = 291.34 + 1.844379 * k;
    const double a12 = 161.72 + 24.198154 * k;
    const double a13 = 239.56 + 25.513099 * k;
    const double a14 = 331.55 + 3.592518 * k;
    const double planetary_correction =
        0.000325 * sin_degrees(a1) + 0.000165 * sin_degrees(a2) +
        0.000164 * sin_degrees(a3) + 0.000126 * sin_degrees(a4) +
        0.000110 * sin_degrees(a5) + 0.000062 * sin_degrees(a6) +
        0.000060 * sin_degrees(a7) + 0.000056 * sin_degrees(a8) +
        0.000047 * sin_degrees(a9) + 0.000042 * sin_degrees(a10) +
        0.000040 * sin_degrees(a11) + 0.000037 * sin_degrees(a12) +
        0.000035 * sin_degrees(a13) + 0.000023 * sin_degrees(a14);

    return jde_mean + correction + planetary_correction;
}

double new_moon_jd_utc(int lunation) {
    // Meeus returns TT. Delta T converts to UT1; UT1 and UTC differ by <1 s.
    const double jde_tt = new_moon_jde_tt(lunation);
    const double year = approximate_decimal_year(jde_tt);
    return jde_tt - delta_t_seconds(year) / 86400.0;
}

}  // namespace

int weekday_from_date(int year, int month, int day) {
    const int y = year - 1;
    const int days_before_year = y * 365 + y / 4 - y / 100 + y / 400;
    const int ordinal = days_before_year + days_before_month(year, month) + day;
    return ordinal % 7;
}

const char* weekday_name(int weekday) {
    static constexpr const char* kNames[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    if (weekday < 0 || weekday > 6) {
        return "---";
    }
    return kNames[weekday];
}

int days_since_2000_01_01(int year, int month, int day) {
    int days = 0;
    for (int y = 2000; y < year; ++y) {
        days += is_leap_year(y) ? 366 : 365;
    }
    return days + days_before_month(year, month) + day - 1;
}

bool is_valid_datetime(const ds3231_datetime_t& dt) {
    if (dt.year < 2000 || dt.year > 2099 || dt.month < 1 || dt.month > 12 ||
        dt.day < 1 || dt.hour > 23 || dt.minute > 59 || dt.second > 59) {
        return false;
    }

    return dt.day <= days_in_month(dt.year, dt.month);
}

int moon_age_tenths(const ds3231_datetime_t& dt) {
    if (!is_valid_datetime(dt)) {
        return 0;
    }

    const int days = days_since_2000_01_01(dt.year, dt.month, dt.day);
    const double local_day_fraction =
        (dt.hour * 3600.0 + dt.minute * 60.0 + dt.second) / 86400.0;
    const double now_jd_utc =
        kJd2000Jan1Utc + days + local_day_fraction - kJstOffsetDays;
    const int estimated_lunation = static_cast<int>(
        std::floor((now_jd_utc - kJde2000NewMoon) /
                   kMeanSynodicMonthDays));

    double preceding_new_moon_jd_utc = -1.0;
    for (int offset = -1; offset <= 1; ++offset) {
        const double candidate_jd_utc =
            new_moon_jd_utc(estimated_lunation + offset);
        if (candidate_jd_utc <= now_jd_utc &&
            candidate_jd_utc > preceding_new_moon_jd_utc) {
            preceding_new_moon_jd_utc = candidate_jd_utc;
        }
    }

    if (preceding_new_moon_jd_utc < 0.0) {
        return 0;
    }

    const double age_days = now_jd_utc - preceding_new_moon_jd_utc;
    const int age_tenths = static_cast<int>(std::floor(age_days * 10.0 + 0.5));
    return age_tenths;
}

void format_clock_lines(const ds3231_datetime_t& dt,
                        bool rtc_ok,
                        bool show_seconds,
                        char* date_line,
                        size_t date_len,
                        char* time_line,
                        size_t time_len) {

    if (rtc_ok) {
        const int weekday = weekday_from_date(dt.year, dt.month, dt.day);
        std::snprintf(date_line, date_len, "%04u-%02u-%02u %s",
                      dt.year, dt.month, dt.day, weekday_name(weekday));
        if (show_seconds) {
            std::snprintf(time_line, time_len, "%02u:%02u:%02u",
                          dt.hour, dt.minute, dt.second);
        } else {
            std::snprintf(time_line, time_len, "%02u:%02u",
                          dt.hour, dt.minute);
        }
    } else {
        std::snprintf(date_line, date_len, "---- -- -- ---");
        std::snprintf(time_line, time_len, show_seconds ? "--:--:--" : "--:--");
    }
}

void format_moon_age_line(const ds3231_datetime_t& dt,
                          bool rtc_ok,
                          char* moon_line,
                          size_t moon_len) {
    if (!rtc_ok) {
        std::snprintf(moon_line, moon_len, "Moon --.-d");
        return;
    }

    const int age = moon_age_tenths(dt);
    std::snprintf(moon_line, moon_len, "Moon %02d.%01dd", age / 10, age % 10);
}
