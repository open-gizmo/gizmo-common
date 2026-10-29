#include <stdint.h>
#include <string.h>
#include <unity.h>

#include <gizmo/format.h>

using gizmo::DURATION_TEXT_SIZE;

namespace {
	// Format a duration into a buffer of the documented size
	void formatIntoBuffer(uint32_t totalSeconds, char *buffer) {
		gizmo::formatDuration(totalSeconds, buffer, DURATION_TEXT_SIZE);
	}
}

// Nothing to prepare before each test
void setUp() {}

// Nothing to clean up after each test
void tearDown() {}

// Short durations are shown with two-digit minutes and seconds
void test_formats_short_durations_as_mm_ss() {
	char buffer[DURATION_TEXT_SIZE] = {0};

	// Expect zero-padded minutes and seconds
	formatIntoBuffer(0, buffer);
	TEST_ASSERT_EQUAL_STRING("00:00", buffer);
	formatIntoBuffer(65, buffer);
	TEST_ASSERT_EQUAL_STRING("01:05", buffer);
	formatIntoBuffer(59 * 60 + 59, buffer);
	TEST_ASSERT_EQUAL_STRING("59:59", buffer);
}

// One hour is shown as 60 minutes
void test_formats_one_hour_as_minutes() {
	char buffer[DURATION_TEXT_SIZE] = {0};

	// Format one hour
	formatIntoBuffer(60 * 60, buffer);

	// Expect 60 minutes
	TEST_ASSERT_EQUAL_STRING("60:00", buffer);
}

// Durations of 100 minutes or more used to be cut off ("100:0") by a 6-byte buffer in the reolink switch
void test_formats_three_digit_minutes_without_cutting_them() {
	char buffer[DURATION_TEXT_SIZE] = {0};

	// Format 100 minutes
	formatIntoBuffer(100 * 60, buffer);

	// Expect the full text
	TEST_ASSERT_EQUAL_STRING("100:00", buffer);
}

// The documented buffer size fits the largest possible value
void test_buffer_size_fits_any_duration() {
	char buffer[DURATION_TEXT_SIZE] = {0};

	// Format the largest value a uint32_t can hold
	formatIntoBuffer(UINT32_MAX, buffer);

	// Expect the full text, with nothing cut off
	TEST_ASSERT_EQUAL_STRING("71582788:15", buffer);
}

// A buffer that is too small is cut off safely instead of overflowing
void test_small_buffer_is_cut_off_safely() {
	char buffer[8] = { 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x' };

	// Format into the first 4 bytes only
	gizmo::formatDuration(600, buffer, 4);

	// Expect a terminated, cut-off text and the rest of the buffer untouched
	TEST_ASSERT_EQUAL_STRING("10:", buffer);
	TEST_ASSERT_EQUAL_CHAR('x', buffer[4]);
}

// Run all the duration tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_formats_short_durations_as_mm_ss);
	RUN_TEST(test_formats_one_hour_as_minutes);
	RUN_TEST(test_formats_three_digit_minutes_without_cutting_them);
	RUN_TEST(test_buffer_size_fits_any_duration);
	RUN_TEST(test_small_buffer_is_cut_off_safely);
	return UNITY_END();
}