/* clock.h against CPython 3.14.8: the expected values are in clock_corpus.h (the generator is in its header). */
#include "avp/avp.h"
#include "avp/clock.h"
#include "avp/test.h"
#include "clock_corpus.h"

#include <math.h>
#include <stdio.h>
#include <windows.h>

static double bits_to_double(unsigned long long b) {
    double d;
    memcpy(&d, &b, sizeof d);
    return d;
}

AVP_TEST(clocks_are_consistent) {
    double a = avp_time();
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    u64 t = ((u64)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    double sys = (double)(t - 116444736000000000ULL) / 1e7;
    CHECK(fabs(sys - a) < 1.0);
    CHECK(avp_time_ns() / 1000000000LL == (i64)floor(avp_time()) || avp_time_ns() / 1000000000LL == (i64)floor(avp_time()) + 1);
    double m0 = avp_monotonic(), p0 = avp_perf_counter();
    i64 n0 = avp_monotonic_ns();
    avp_sleep(0.02);
    double m1 = avp_monotonic(), p1 = avp_perf_counter();
    i64 n1 = avp_perf_counter_ns();
    CHECK(m1 > m0);
    CHECK(p1 - p0 >= 0.0195);
    CHECK(p1 - p0 < 1.0);
    CHECK(n1 > n0);
    CHECK(fabs((double)(n1 - n0) / 1e9 - (m1 - m0)) < 0.05);
    CHECK(avp_process_time() >= 0.0);
}

AVP_TEST(perf_counter_is_the_exact_ns_conversion) {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    i64 ns = avp_perf_counter_ns();
    /* an independent 128-bit computation of ticks * 1e9 / freq at about the same moment */
    LARGE_INTEGER c2;
    QueryPerformanceCounter(&c2);
    u64 hi;
    u64 lo = _umul128((u64)c.QuadPart, 1000000000ULL, &hi);
    u64 rem;
    u64 lo_ns = _udiv128(hi, lo, (u64)f.QuadPart, &rem);
    u64 hi2;
    u64 lo2 = _umul128((u64)c2.QuadPart, 1000000000ULL, &hi2);
    u64 hi_ns = _udiv128(hi2, lo2, (u64)f.QuadPart, &rem);
    CHECK((u64)ns >= lo_ns);
    CHECK((u64)ns <= hi_ns);
}

AVP_TEST(sleep_waits_at_least_the_time) {
    double t0 = avp_perf_counter();
    avp_sleep(0.05);
    double dt = avp_perf_counter() - t0;
    CHECK(dt >= 0.0495);
    CHECK(dt < 0.5);
    t0 = avp_perf_counter();
    avp_sleep(0.0);
    avp_sleep(-3.0);
    CHECK(avp_perf_counter() - t0 < 0.02);
}

AVP_TEST(gmtime_matches_python) {
    for (size_t i = 0; i < AVP_ARRAY_LEN(clock_gm_cases); i++) {
        const ClockGmCase *c = &clock_gm_cases[i];
        AvpTm tm;
        REQUIRE(avp_gmtime(c->t, &tm));
        CHECK_MSG(tm.year == c->year && tm.mon == c->mon && tm.mday == c->mday && tm.hour == c->hour &&
                      tm.min == c->min && tm.sec == c->sec && tm.wday == c->wday && tm.yday == c->yday,
                  "gmtime(%.0f): %d-%02d-%02d %02d:%02d:%02d wday %d yday %d", c->t, tm.year, tm.mon, tm.mday, tm.hour,
                  tm.min, tm.sec, tm.wday, tm.yday);
    }
    AvpTm tm;
    CHECK(!avp_gmtime(NAN, &tm));
    CHECK(!avp_gmtime(INFINITY, &tm));
    CHECK(!avp_gmtime(1e300, &tm));
    /* the fraction is floored, as time.gmtime(-0.5) is 1969-12-31 23:59:59 */
    REQUIRE(avp_gmtime(-0.5, &tm));
    CHECK_EQ_I64(tm.year, 1969);
    CHECK_EQ_I64(tm.sec, 59);
    REQUIRE(avp_gmtime(1.999, &tm));
    CHECK_EQ_I64(tm.sec, 1);
}

AVP_TEST(out_of_range_times_fail_as_they_do_in_python) {
    /* python -c "import time; time.gmtime(-86400)" is OSError [Errno 22]; gmtime(-43200) works; localtime(-1) fails */
    AvpTm tm;
    CHECK(avp_gmtime(-43200, &tm) && tm.hour == 12 && tm.mday == 31);
    CHECK(!avp_gmtime(-86400, &tm));
    CHECK(!avp_gmtime(-100000, &tm));
    CHECK(!avp_localtime(-1, &tm));
    CHECK(!avp_localtime(-43200, &tm));
    CHECK(!avp_gmtime(1099511627776.0, &tm));
    CHECK(avp_gmtime(32535216000.0, &tm) && tm.year == 3001 && tm.mon == 1 && tm.mday == 1);
    CHECK(avp_localtime(32535215999.0, &tm) && tm.year == 3000);
}

AVP_TEST(localtime_follows_the_windows_zone) {
    /* the offset between localtime and gmtime must be what Windows says for that instant */
    double stamps[] = {0.0, 1700000000.0, 1720000000.0, 1234567890.0};
    for (size_t i = 0; i < AVP_ARRAY_LEN(stamps); i++) {
        AvpTm lt, gm;
        REQUIRE(avp_localtime(stamps[i], &lt));
        REQUIRE(avp_gmtime(stamps[i], &gm));
        FILETIME ft;
        u64 v = (u64)(stamps[i] * 1e7) + 116444736000000000ULL;
        ft.dwLowDateTime = (DWORD)v;
        ft.dwHighDateTime = (DWORD)(v >> 32);
        SYSTEMTIME utc, loc;
        FileTimeToSystemTime(&ft, &utc);
        SystemTimeToTzSpecificLocalTime(NULL, &utc, &loc);
        CHECK_MSG(lt.year == loc.wYear && lt.mon == loc.wMonth && lt.mday == loc.wDay && lt.hour == loc.wHour &&
                      lt.min == loc.wMinute && lt.sec == loc.wSecond,
                  "localtime(%.0f) = %d-%02d-%02d %02d:%02d:%02d, Windows says %d-%02d-%02d %02d:%02d:%02d", stamps[i], lt.year,
                  lt.mon, lt.mday, lt.hour, lt.min, lt.sec, loc.wYear, loc.wMonth, loc.wDay, loc.wHour, loc.wMinute,
                  loc.wSecond);
        CHECK(lt.isdst == 0 || lt.isdst == 1);
        double back;
        AvpTm roundtrip = lt;
        REQUIRE(avp_mktime(&roundtrip, &back));
        CHECK_EQ_F64(back, stamps[i]);
    }
    AvpTm now;
    avp_localtime_now(&now);
    avp_gmtime_now(&now);
    CHECK(now.year >= 2026);
}

AVP_TEST(strftime_matches_python) {
    for (size_t i = 0; i < AVP_ARRAY_LEN(clock_strftime_cases); i++) {
        const ClockStrftimeCase *c = &clock_strftime_cases[i];
        AvpTm tm;
        REQUIRE(avp_gmtime(c->t, &tm));
        AvpStr out;
        bool ok = avp_strftime(avp_scratch(), c->fmt, &tm, &out);
        if (!c->expect) {
            CHECK_MSG(!ok, "strftime(\"%s\", %.0f) should be an error, gave \"%.*s\"", c->fmt, c->t, AVP_SF(out));
        } else {
            CHECK_MSG(ok, "strftime(\"%s\", %.0f) failed", c->fmt, c->t);
            CHECK_MSG(ok && avp_str_eqz(out, c->expect), "strftime(\"%s\", %.0f): \"%.*s\", Python \"%s\"", c->fmt, c->t,
                      AVP_SF(out), c->expect);
        }
    }
}

AVP_TEST(strftime_now_and_zone_names) {
    AvpStr s;
    REQUIRE(avp_strftime_now(avp_scratch(), "%Y-%m-%d %H:%M:%S", &s));
    CHECK_EQ_U64(s.n, 19);
    REQUIRE(avp_strftime_now(avp_scratch(), "%Z|%z", &s));
    CHECK(s.n >= 6);
    CHECK(memchr(s.p, '|', s.n) != NULL);
}

AVP_TEST(datetime_fromtimestamp_utc_matches_python) {
    for (size_t i = 0; i < AVP_ARRAY_LEN(clock_fromts_cases); i++) {
        const ClockFromTsCase *c = &clock_fromts_cases[i];
        double ts = bits_to_double(c->ts);
        AvpDateTime dt;
        bool ok = avp_datetime_fromtimestamp(ts, true, &dt);
        if (!c->iso) {
            CHECK_MSG(!ok, "fromtimestamp(%.17g, utc) should fail", ts);
            continue;
        }
        REQUIRE(ok);
        char iso[48];
        avp_datetime_isoformat(&dt, 'T', iso);
        CHECK_MSG(strcmp(iso, c->iso) == 0, "fromtimestamp(%.17g, utc): %s, Python %s", ts, iso, c->iso);
    }
}

AVP_TEST(datetime_local_fromtimestamp_agrees_with_localtime) {
    AvpDateTime dt;
    AvpTm lt;
    REQUIRE(avp_datetime_fromtimestamp(1700000000.5, false, &dt));
    REQUIRE(avp_localtime(1700000000.0, &lt));
    CHECK_EQ_I64(dt.hour, lt.hour);
    CHECK_EQ_I64(dt.minute, lt.min);
    CHECK_EQ_I64(dt.microsecond, 500000);
    CHECK(!dt.aware);
    AvpDateTime now = avp_datetime_now();
    CHECK(now.year >= 2026 && !now.aware);
    AvpDateTime utc = avp_datetime_now_utc();
    CHECK(utc.aware && utc.utcoffset_s == 0);
    char iso[48];
    avp_datetime_isoformat(&utc, 'T', iso);
    CHECK_EQ_U64(strlen(iso), strlen("2026-11-01T21:20:38.709025+00:00"));
    CHECK(strcmp(iso + strlen(iso) - 6, "+00:00") == 0);
    AvpDateTime un = avp_datetime_utcnow();
    CHECK(!un.aware);
}

AVP_TEST(datetime_strftime_and_isoformat_match_python) {
    for (size_t i = 0; i < AVP_ARRAY_LEN(clock_dt_cases); i++) {
        const ClockDtCase *c = &clock_dt_cases[i];
        AvpDateTime dt = {c->y, c->mo, c->d, c->h, c->mi, c->s, c->us, c->aware != 0, c->off};
        char iso[48];
        avp_datetime_isoformat(&dt, 'T', iso);
        CHECK_MSG(strcmp(iso, c->iso) == 0, "isoformat: %s, Python %s", iso, c->iso);
        AvpStr out;
        bool ok = avp_datetime_strftime(avp_scratch(), &dt, c->fmt, &out);
        if (!c->expect) {
            CHECK_MSG(!ok, "datetime.strftime(\"%s\") should fail", c->fmt);
        } else {
            CHECK_MSG(ok && avp_str_eqz(out, c->expect), "datetime.strftime(\"%s\") of %s: \"%.*s\", Python \"%s\"", c->fmt, c->iso,
                      AVP_SF(out), c->expect);
        }
    }
}

AVP_TEST(date_fromisoformat_matches_python) {
    for (size_t i = 0; i < AVP_ARRAY_LEN(clock_date_cases); i++) {
        const ClockDateCase *c = &clock_date_cases[i];
        AvpDate d;
        bool ok = avp_date_fromisoformat(avp_str(c->s), &d);
        if (c->y == 0) {
            CHECK_MSG(!ok, "fromisoformat(\"%s\") should fail", c->s);
        } else {
            CHECK_MSG(ok && d.year == c->y && d.month == c->m && d.day == c->d, "fromisoformat(\"%s\"): ok %d %d-%d-%d, Python %d-%d-%d",
                      c->s, ok, ok ? d.year : 0, ok ? d.month : 0, ok ? d.day : 0, c->y, c->m, c->d);
        }
    }
}

AVP_TEST(date_ordinals_weekdays_and_isoformat) {
    for (size_t i = 0; i < AVP_ARRAY_LEN(clock_ord_cases); i++) {
        const ClockOrdCase *c = &clock_ord_cases[i];
        AvpDate d = {c->y, c->m, c->d};
        CHECK_EQ_I64(avp_date_toordinal(d), c->ordinal);
        CHECK_EQ_I64(avp_date_weekday(d), c->weekday);
        AvpDate back = avp_date_fromordinal(c->ordinal);
        CHECK_MSG(back.year == c->y && back.month == c->m && back.day == c->d, "fromordinal(%d) = %d-%d-%d", c->ordinal, back.year,
                  back.month, back.day);
        char s[16];
        avp_date_isoformat(d, s);
        char want[16];
        snprintf(want, sizeof want, "%04d-%02d-%02d", c->y, c->m, c->d);
        CHECK_CZ(s, want);
    }
    AvpDate a = {2026, 11, 1}, b = {2026, 10, 14};
    CHECK_EQ_I64(avp_date_diff_days(a, b), 18);
    CHECK_EQ_I64(avp_date_diff_days(b, a), -18);
    CHECK(avp_date_valid(2024, 2, 29) && !avp_date_valid(2026, 2, 29) && !avp_date_valid(0, 1, 1) && !avp_date_valid(10000, 1, 1));
    AvpDate today = avp_date_today();
    CHECK(today.year >= 2026);
}

AVP_TEST(timedelta_matches_python) {
    for (size_t i = 0; i < AVP_ARRAY_LEN(clock_td_cases); i++) {
        const ClockTdCase *c = &clock_td_cases[i];
        double secs = bits_to_double(c->secs);
        AvpTimeDelta td;
        bool ok = avp_timedelta_from_seconds(secs, &td);
        if (!c->ok) {
            CHECK_MSG(!ok, "timedelta(seconds=%.17g) should fail", secs);
            continue;
        }
        REQUIRE(ok);
        CHECK_MSG(td.days == c->days && td.seconds == c->seconds && td.microseconds == c->microseconds,
                  "timedelta(seconds=%.17g) = %lld d %d s %d us, Python %lld d %d s %d us", secs, td.days, td.seconds,
                  td.microseconds, c->days, c->seconds, c->microseconds);
        char s[64];
        avp_timedelta_str(td, s);
        CHECK_MSG(strcmp(s, c->str) == 0, "str(timedelta(seconds=%.17g)) = \"%s\", Python \"%s\"", secs, s, c->str);
        double total = avp_timedelta_total_seconds(td);
        double want = bits_to_double(c->total);
        if (llabs(td.days) < 100) CHECK_MSG(memcmp(&total, &want, 8) == 0, "total_seconds %.17g, Python %.17g", total, want);
    }
    AvpTimeDelta td;
    CHECK(!avp_timedelta_from_seconds(NAN, &td));
    CHECK(!avp_timedelta_from_seconds(INFINITY, &td));
}

AVP_TEST(round_matches_python) {
    for (size_t i = 0; i < AVP_ARRAY_LEN(clock_round_cases); i++) {
        const ClockRoundCase *c = &clock_round_cases[i];
        double x = bits_to_double(c->x), want = bits_to_double(c->expect);
        double got = avp_clock_round(x, c->nd);
        CHECK_MSG(memcmp(&got, &want, 8) == 0, "round(%.17g, %d) = %.17g, Python %.17g", x, c->nd, got, want);
    }
}

AVP_TEST(fixed_format_matches_python) {
    for (size_t i = 0; i < AVP_ARRAY_LEN(clock_fixed_cases); i++) {
        const ClockFixedCase *c = &clock_fixed_cases[i];
        double x = bits_to_double(c->x);
        char buf[512];
        size_t n = avp_fmt_fixed(buf, sizeof buf, x, c->nd);
        CHECK_MSG(strcmp(buf, c->expect) == 0 && n == strlen(c->expect), "'%%.%df' %% %.17g = \"%s\", Python \"%s\"", c->nd, x, buf, c->expect);
    }
}

AVP_TEST(stopwatch_measures_wall_time) {
    AvpStopwatch w = avp_stopwatch_start();
    avp_sleep(0.03);
    double s = avp_stopwatch_secs(&w);
    CHECK(s >= 0.0295 && s < 1.0);
}

AVP_TEST_MAIN("clock")
