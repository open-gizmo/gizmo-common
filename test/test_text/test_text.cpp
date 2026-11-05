#include <string.h>
#include <unity.h>

#include <gizmo/text.h>

// Nothing to prepare before each test
void setUp() {}

// Nothing to clean up after each test
void tearDown() {}

// A text that fits is copied whole
void test_text_that_fits_is_copied() {
	char buffer[8];
	TEST_ASSERT_TRUE(gizmo::copyString(buffer, "bitcoin", sizeof(buffer)));
	TEST_ASSERT_EQUAL_STRING("bitcoin", buffer);
}

// A text that's too long is cut and still ended
void test_long_text_is_cut_and_ended() {
	char buffer[5] = { 'x', 'x', 'x', 'x', 'x' };
	TEST_ASSERT_FALSE(gizmo::copyString(buffer, "kaspa coin", sizeof(buffer)));
	TEST_ASSERT_EQUAL_STRING("kasp", buffer);
}

// A null text copies as empty
void test_null_text_is_empty() {
	char buffer[4] = "old";
	TEST_ASSERT_TRUE(gizmo::copyString(buffer, nullptr, sizeof(buffer)));
	TEST_ASSERT_EQUAL_STRING("", buffer);
}

// A zero-size buffer is never written
void test_zero_size_buffer_is_not_written() {
	char buffer[2] = { 'x', 'y' };
	TEST_ASSERT_FALSE(gizmo::copyString(buffer, "text", 0));
	TEST_ASSERT_EQUAL_CHAR('x', buffer[0]);
	TEST_ASSERT_TRUE(gizmo::copyString(buffer, "", 0));
}

// A text can be moved within its own buffer, like the scrolling lines of an LCD
void test_overlapping_copy_is_safe() {
	char buffer[16] = "hello world";
	TEST_ASSERT_TRUE(gizmo::copyString(buffer, buffer + 6, sizeof(buffer)));
	TEST_ASSERT_EQUAL_STRING("world", buffer);
}

// Run all the text tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_text_that_fits_is_copied);
	RUN_TEST(test_long_text_is_cut_and_ended);
	RUN_TEST(test_null_text_is_empty);
	RUN_TEST(test_zero_size_buffer_is_not_written);
	RUN_TEST(test_overlapping_copy_is_safe);
	return UNITY_END();
}