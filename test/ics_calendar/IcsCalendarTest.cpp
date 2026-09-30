#include <gtest/gtest.h>

#include <string>

#include "IcsCalendar/IcsCalendar.h"

using namespace ics;

namespace {

int32_t D(int y, int m, int d) { return daysFromCivil(y, m, d); }

Calendar parse(const std::string& text, int32_t start, int32_t end, int offset = 0, size_t chunk = 0) {
  ParseOptions o;
  o.windowStartDay = start;
  o.windowEndDay = end;
  o.utcOffsetMinutes = offset;
  Parser p(o);
  const auto* bytes = reinterpret_cast<const uint8_t*>(text.data());
  if (chunk == 0) chunk = text.size();
  for (size_t i = 0; i < text.size(); i += chunk) p.feed(bytes + i, std::min(chunk, text.size() - i));
  return p.finish();
}

std::string wrap(const std::string& events) { return "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n" + events + "END:VCALENDAR\r\n"; }

}  // namespace

TEST(IcsDates, RoundTripAndWeekday) {
  EXPECT_EQ(D(1970, 1, 1), 0);
  int y, m, d;
  civilFromDays(D(2026, 9, 29), y, m, d);
  EXPECT_EQ(y, 2026);
  EXPECT_EQ(m, 9);
  EXPECT_EQ(d, 29);
  EXPECT_EQ(weekdayFromDays(D(2026, 9, 29)), 1);  // Tuesday
  EXPECT_EQ(weekdayFromDays(0), 3);               // Thursday
  EXPECT_EQ(daysInMonth(2028, 2), 29);
  EXPECT_EQ(daysInMonth(2100, 2), 28);
}

TEST(IcsParse, SimpleTimedEventUtcShift) {
  const auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20261001T230000Z\r\nDTEND:20261002T000000Z\r\nSUMMARY:Late\r\nEND:VEVENT\r\n"),
                       D(2026, 9, 1), D(2026, 12, 1), -300);
  ASSERT_EQ(c.occurrences.size(), 1u);
  EXPECT_EQ(c.occurrences[0].day, D(2026, 10, 1));
  EXPECT_EQ(c.occurrences[0].startMinute, 18 * 60);
  EXPECT_STREQ(c.title(c.occurrences[0]), "Late");
}

TEST(IcsParse, FloatingAndTzidAreLocal) {
  const auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART;TZID=America/New_York:20261001T090000\r\nSUMMARY:Meet\r\nEND:VEVENT\r\n"),
                       D(2026, 9, 1), D(2026, 12, 1), -300);
  ASSERT_EQ(c.occurrences.size(), 1u);
  EXPECT_EQ(c.occurrences[0].startMinute, 9 * 60);
}

TEST(IcsParse, AllDayMultiDayEndExclusive) {
  const auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART;VALUE=DATE:20261010\r\nDTEND;VALUE=DATE:20261013\r\nSUMMARY:Trip\r\nEND:VEVENT\r\n"),
                       D(2026, 9, 1), D(2026, 12, 1));
  ASSERT_EQ(c.occurrences.size(), 3u);
  EXPECT_TRUE(c.occurrences[0].allDay());
  EXPECT_EQ(c.occurrences[2].day, D(2026, 10, 12));
}

TEST(IcsParse, FoldedLinesEscapesAndChunking) {
  const std::string text = wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20261001T100000\r\nSUMMARY:Lunch\\, with\r\n  Sam\r\nEND:VEVENT\r\n");
  for (size_t chunk : {size_t(1), size_t(7), size_t(0)}) {
    const auto c = parse(text, D(2026, 9, 1), D(2026, 12, 1), 0, chunk);
    ASSERT_EQ(c.occurrences.size(), 1u);
    EXPECT_STREQ(c.title(c.occurrences[0]), "Lunch, with Sam");
  }
}

TEST(IcsParse, AlarmDoesNotOverwriteSummary) {
  const auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20261001T100000\r\nSUMMARY:Real\r\nBEGIN:VALARM\r\nSUMMARY:Alarm\r\nEND:VALARM\r\nEND:VEVENT\r\n"),
                       D(2026, 9, 1), D(2026, 12, 1));
  ASSERT_EQ(c.occurrences.size(), 1u);
  EXPECT_STREQ(c.title(c.occurrences[0]), "Real");
}

TEST(IcsParse, OutsideWindowDropped) {
  const auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20200101T100000Z\r\nSUMMARY:Old\r\nEND:VEVENT\r\n"), D(2026, 9, 1), D(2026, 12, 1));
  EXPECT_TRUE(c.occurrences.empty());
}

TEST(IcsRecur, WeeklyByDayWithCountAndExdate) {
  const auto c = parse(
      wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20261005T090000\r\nRRULE:FREQ=WEEKLY;BYDAY=MO,WE;COUNT=6\r\nEXDATE:20261007T090000\r\nSUMMARY:Gym\r\nEND:VEVENT\r\n"),
      D(2026, 9, 1), D(2026, 12, 31));
  // Mon 5, Wed 7 (excluded), Mon 12, Wed 14, Mon 19, Wed 21 -> 6 counted, 5 emitted
  ASSERT_EQ(c.occurrences.size(), 5u);
  EXPECT_EQ(c.occurrences[0].day, D(2026, 10, 5));
  EXPECT_EQ(c.occurrences[1].day, D(2026, 10, 12));
  EXPECT_EQ(c.occurrences[4].day, D(2026, 10, 21));
}

TEST(IcsRecur, DailyUntil) {
  const auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20261001T090000\r\nRRULE:FREQ=DAILY;UNTIL=20261003T235959Z\r\nSUMMARY:D\r\nEND:VEVENT\r\n"),
                       D(2026, 9, 1), D(2026, 12, 31));
  EXPECT_EQ(c.occurrences.size(), 3u);
}

TEST(IcsRecur, MonthlyNthWeekdayAndLast) {
  const auto nth = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20261008T090000\r\nRRULE:FREQ=MONTHLY;BYDAY=2TH;COUNT=3\r\nSUMMARY:M\r\nEND:VEVENT\r\n"),
                         D(2026, 9, 1), D(2027, 6, 1));
  ASSERT_EQ(nth.occurrences.size(), 3u);
  EXPECT_EQ(nth.occurrences[1].day, D(2026, 11, 12));
  const auto last = parse(wrap("BEGIN:VEVENT\r\nUID:b\r\nDTSTART:20261030T090000\r\nRRULE:FREQ=MONTHLY;BYDAY=-1FR;COUNT=2\r\nSUMMARY:L\r\nEND:VEVENT\r\n"),
                          D(2026, 9, 1), D(2027, 6, 1));
  ASSERT_EQ(last.occurrences.size(), 2u);
  EXPECT_EQ(last.occurrences[1].day, D(2026, 11, 27));
}

TEST(IcsRecur, MonthlyDay31SkipsShortMonths) {
  const auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20261031T090000\r\nRRULE:FREQ=MONTHLY;COUNT=3\r\nSUMMARY:E\r\nEND:VEVENT\r\n"),
                       D(2026, 9, 1), D(2027, 12, 31));
  ASSERT_EQ(c.occurrences.size(), 3u);
  EXPECT_EQ(c.occurrences[1].day, D(2026, 12, 31));
}

TEST(IcsRecur, YearlyBirthday) {
  const auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART;VALUE=DATE:20100314\r\nRRULE:FREQ=YEARLY\r\nSUMMARY:Pi\r\nEND:VEVENT\r\n"),
                       D(2026, 1, 1), D(2027, 12, 31));
  ASSERT_EQ(c.occurrences.size(), 2u);
  EXPECT_EQ(c.occurrences[0].day, D(2026, 3, 14));
  EXPECT_TRUE(c.occurrences[0].allDay());
}

TEST(IcsRecur, OverrideMovesAndCancels) {
  const auto c = parse(
      wrap("BEGIN:VEVENT\r\nUID:x\r\nDTSTART:20261005T090000\r\nRRULE:FREQ=WEEKLY;COUNT=3\r\nSUMMARY:Standup\r\nEND:VEVENT\r\n"
           "BEGIN:VEVENT\r\nUID:x\r\nRECURRENCE-ID:20261012T090000\r\nDTSTART:20261013T110000\r\nSUMMARY:Standup moved\r\nEND:VEVENT\r\n"
           "BEGIN:VEVENT\r\nUID:x\r\nRECURRENCE-ID:20261019T090000\r\nDTSTART:20261019T090000\r\nSTATUS:CANCELLED\r\nSUMMARY:Standup\r\nEND:VEVENT\r\n"),
      D(2026, 9, 1), D(2026, 12, 31));
  ASSERT_EQ(c.occurrences.size(), 2u);
  EXPECT_EQ(c.occurrences[0].day, D(2026, 10, 5));
  EXPECT_EQ(c.occurrences[1].day, D(2026, 10, 13));
  EXPECT_STREQ(c.title(c.occurrences[1]), "Standup moved");
}

TEST(IcsCalendar, SortingMaskAndQueries) {
  const auto c = parse(
      wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20261001T150000\r\nSUMMARY:B\r\nEND:VEVENT\r\n"
           "BEGIN:VEVENT\r\nUID:b\r\nDTSTART:20261001T080000\r\nDTEND:20261001T090000\r\nSUMMARY:A\r\nEND:VEVENT\r\n"
           "BEGIN:VEVENT\r\nUID:c\r\nDTSTART;VALUE=DATE:20261001\r\nSUMMARY:AllDay\r\nEND:VEVENT\r\n"
           "BEGIN:VEVENT\r\nUID:d\r\nDTSTART:20261015T120000\r\nSUMMARY:Later\r\nEND:VEVENT\r\n"),
      D(2026, 9, 1), D(2026, 12, 31));
  std::vector<size_t> idx;
  c.eventsOn(D(2026, 10, 1), idx);
  ASSERT_EQ(idx.size(), 3u);
  EXPECT_STREQ(c.title(c.occurrences[idx[0]]), "AllDay");
  EXPECT_STREQ(c.title(c.occurrences[idx[1]]), "A");
  EXPECT_EQ(c.occurrences[idx[1]].endMinute, 9 * 60);
  EXPECT_EQ(c.monthMask(2026, 10), (1u << 0) | (1u << 14));
  c.upcoming(D(2026, 10, 2), 5, idx);
  ASSERT_EQ(idx.size(), 1u);
}

TEST(IcsCalendar, SerializeRoundTripAndRejectsGarbage) {
  auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20261001T150000\r\nSUMMARY:Héllo\r\nEND:VEVENT\r\n"), D(2026, 9, 1), D(2026, 12, 31));
  c.syncedDay = D(2026, 9, 29);
  const std::string blob = serialize(c);
  Calendar back;
  ASSERT_TRUE(deserialize(reinterpret_cast<const uint8_t*>(blob.data()), blob.size(), back));
  ASSERT_EQ(back.occurrences.size(), 1u);
  EXPECT_STREQ(back.title(back.occurrences[0]), "Héllo");
  EXPECT_EQ(back.syncedDay, D(2026, 9, 29));
  Calendar bad;
  EXPECT_FALSE(deserialize(reinterpret_cast<const uint8_t*>(blob.data()), blob.size() - 1, bad));
  EXPECT_FALSE(deserialize(reinterpret_cast<const uint8_t*>("nope"), 4, bad));
}

TEST(IcsParse, NotACalendar) {
  ParseOptions o;
  Parser p(o);
  const std::string html = "<html><body>Sign in</body></html>\n";
  p.feed(reinterpret_cast<const uint8_t*>(html.data()), html.size());
  p.finish();
  EXPECT_FALSE(p.sawCalendar());
}

TEST(IcsParse, OccurrenceCapSetsTruncated) {
  ParseOptions o;
  o.windowStartDay = D(2026, 1, 1);
  o.windowEndDay = D(2027, 12, 31);
  o.maxOccurrences = 10;
  Parser p(o);
  const std::string t = wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20260101T090000\r\nRRULE:FREQ=DAILY\r\nSUMMARY:X\r\nEND:VEVENT\r\n");
  p.feed(reinterpret_cast<const uint8_t*>(t.data()), t.size());
  const auto c = p.finish();
  EXPECT_EQ(c.occurrences.size(), 10u);
  EXPECT_TRUE(c.truncated);
}

TEST(IcsParse, LongTitleKeptToAHundredBytes) {
  const std::string longTitle(150, 'x');
  const auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20261001T100000\r\nSUMMARY:" + longTitle + "\r\nEND:VEVENT\r\n"),
                       D(2026, 9, 1), D(2026, 12, 1));
  ASSERT_EQ(c.occurrences.size(), 1u);
  EXPECT_EQ(std::string(c.title(c.occurrences[0])).size(), 100u);
}

TEST(IcsParse, OvernightEventSplitsAcrossMidnight) {
  const auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20261001T220000\r\nDTEND:20261002T060000\r\nSUMMARY:Night shift\r\nEND:VEVENT\r\n"),
                       D(2026, 9, 1), D(2026, 12, 1));
  ASSERT_EQ(c.occurrences.size(), 2u);
  EXPECT_EQ(c.occurrences[0].day, D(2026, 10, 1));
  EXPECT_EQ(c.occurrences[0].startMinute, 22 * 60);
  EXPECT_EQ(c.occurrences[0].endMinute, 1440);
  EXPECT_TRUE(c.occurrences[0].toNextDay());
  EXPECT_FALSE(c.occurrences[0].fromPreviousDay());
  EXPECT_EQ(c.occurrences[1].day, D(2026, 10, 2));
  EXPECT_EQ(c.occurrences[1].startMinute, 0);
  EXPECT_EQ(c.occurrences[1].endMinute, 6 * 60);
  EXPECT_TRUE(c.occurrences[1].fromPreviousDay());
  EXPECT_FALSE(c.occurrences[1].toNextDay());
}

TEST(IcsParse, MultiDayTimedEventAndDurationForm) {
  const auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20261001T200000\r\nDURATION:P2DT4H\r\nSUMMARY:Retreat\r\nEND:VEVENT\r\n"),
                       D(2026, 9, 1), D(2026, 12, 1));
  ASSERT_EQ(c.occurrences.size(), 3u);  // 2d4h from Oct 1 20:00 ends Oct 4 00:00: Oct 1, 2, 3
  EXPECT_TRUE(c.occurrences[1].fromPreviousDay() && c.occurrences[1].toNextDay());
  EXPECT_EQ(c.occurrences[1].startMinute, 0);
  EXPECT_TRUE(c.occurrences[2].fromPreviousDay());
  EXPECT_FALSE(c.occurrences[2].toNextDay());
  EXPECT_EQ(c.occurrences[2].endMinute, 1440);
}

TEST(IcsParse, EndingAtMidnightStaysOnOneDay) {
  const auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20261001T230000\r\nDTEND:20261002T000000\r\nSUMMARY:Late\r\nEND:VEVENT\r\n"),
                       D(2026, 9, 1), D(2026, 12, 1));
  ASSERT_EQ(c.occurrences.size(), 1u);
  EXPECT_EQ(c.occurrences[0].endMinute, 1440);
  EXPECT_FALSE(c.occurrences[0].toNextDay());
}

TEST(IcsCalendar, FlagsSurviveSerialization) {
  auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20261001T220000\r\nDTEND:20261002T060000\r\nSUMMARY:N\r\nEND:VEVENT\r\n"),
                 D(2026, 9, 1), D(2026, 12, 1));
  const std::string blob = serialize(c);
  Calendar back;
  ASSERT_TRUE(deserialize(reinterpret_cast<const uint8_t*>(blob.data()), blob.size(), back));
  ASSERT_EQ(back.occurrences.size(), 2u);
  EXPECT_TRUE(back.occurrences[0].toNextDay());
  EXPECT_TRUE(back.occurrences[1].fromPreviousDay());
}

TEST(IcsRecur, BySetPosLastDayOfMonth) {
  // Apple's "last day of the month": every weekday listed, keep the last.
  const auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART;VALUE=DATE:20260430\r\nDTEND;VALUE=DATE:20260501\r\n"
                            "RRULE:FREQ=MONTHLY;UNTIL=20310331;BYDAY=SU,MO,TU,WE,TH,FR,SA;BYSETPOS=-1\r\nSUMMARY:Pay\r\nEND:VEVENT\r\n"),
                       D(2026, 4, 1), D(2026, 8, 31));
  ASSERT_EQ(c.occurrences.size(), 5u);  // Apr 30, May 31, Jun 30, Jul 31, Aug 31
  EXPECT_EQ(c.occurrences[0].day, D(2026, 4, 30));
  EXPECT_EQ(c.occurrences[1].day, D(2026, 5, 31));
  EXPECT_EQ(c.occurrences[2].day, D(2026, 6, 30));
  EXPECT_EQ(c.occurrences[4].day, D(2026, 8, 31));
}

TEST(IcsRecur, BySetPosLastWeekdayAndNthMonday) {
  const auto weekday = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20260930T090000\r\nRRULE:FREQ=MONTHLY;BYDAY=MO,TU,WE,TH,FR;BYSETPOS=-1\r\nSUMMARY:W\r\nEND:VEVENT\r\n"),
                             D(2026, 9, 1), D(2026, 12, 31));
  ASSERT_EQ(weekday.occurrences.size(), 4u);
  EXPECT_EQ(weekday.occurrences[1].day, D(2026, 10, 30));  // Fri
  EXPECT_EQ(weekday.occurrences[2].day, D(2026, 11, 30));  // Mon
  const auto monday = parse(wrap("BEGIN:VEVENT\r\nUID:b\r\nDTSTART:20260928T090000\r\nRRULE:FREQ=MONTHLY;BYDAY=MO;BYSETPOS=4;COUNT=3\r\nSUMMARY:M\r\nEND:VEVENT\r\n"),
                            D(2026, 9, 1), D(2026, 12, 31));
  ASSERT_EQ(monday.occurrences.size(), 3u);
  EXPECT_EQ(monday.occurrences[0].day, D(2026, 9, 28));
  EXPECT_EQ(monday.occurrences[1].day, D(2026, 10, 26));
}

TEST(IcsRecur, MonthlyHonoursByMonth) {
  const auto c = parse(wrap("BEGIN:VEVENT\r\nUID:a\r\nDTSTART:20260115T090000\r\nRRULE:FREQ=MONTHLY;BYMONTH=1,7\r\nSUMMARY:X\r\nEND:VEVENT\r\n"),
                       D(2026, 1, 1), D(2026, 12, 31));
  ASSERT_EQ(c.occurrences.size(), 2u);
  EXPECT_EQ(c.occurrences[1].day, D(2026, 7, 15));
}
