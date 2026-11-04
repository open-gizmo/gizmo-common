#include <unity.h>

#include <gizmo/embedded_assets.h>

using gizmo::EmbeddedAsset;

namespace {
	// Fake contents shared by every asset
	const uint8_t CONTENTS[] = { 0x1f, 0x8b };

	// Assets as the gizmo-embed tool lists them for a small Nuxt site
	const EmbeddedAsset ASSETS[] = {
		{ "/", "text/html; charset=utf-8", CONTENTS, sizeof(CONTENTS) },
		{ "/404.html", "text/html; charset=utf-8", CONTENTS, sizeof(CONTENTS) },
		{ "/settings", "text/html; charset=utf-8", CONTENTS, sizeof(CONTENTS) },
		{ "/_nuxt/entry.abc123.js", "application/javascript; charset=utf-8", CONTENTS, sizeof(CONTENTS) }
	};
	constexpr size_t ASSET_COUNT = sizeof(ASSETS) / sizeof(ASSETS[0]);

	// Get the route of the asset found at a path ("" when none is found)
	const char *findRoute(const char *path) {
		const EmbeddedAsset *asset = gizmo::findEmbeddedAsset(ASSETS, ASSET_COUNT, path);
		return asset != nullptr ? asset->routePath : "";
	}
}

// Nothing to prepare before each test
void setUp() {}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Lookup ********************************/

// Pages and files are found at their exact path
void test_assets_are_found_by_path() {
	TEST_ASSERT_EQUAL_STRING("/", findRoute("/"));
	TEST_ASSERT_EQUAL_STRING("/settings", findRoute("/settings"));
	TEST_ASSERT_EQUAL_STRING("/_nuxt/entry.abc123.js", findRoute("/_nuxt/entry.abc123.js"));
}

// A trailing slash finds the same page
void test_trailing_slash_is_ignored() {
	TEST_ASSERT_EQUAL_STRING("/settings", findRoute("/settings/"));
}

// A path that only starts like an asset is not that asset
void test_prefix_is_not_a_match() {
	TEST_ASSERT_EQUAL_STRING("", findRoute("/set"));
	TEST_ASSERT_EQUAL_STRING("", findRoute("/settings/wifi"));
}

// No list or no path finds nothing
void test_missing_input_finds_nothing() {
	TEST_ASSERT_NULL(gizmo::findEmbeddedAsset(nullptr, 0, "/"));
	TEST_ASSERT_NULL(gizmo::findEmbeddedAsset(ASSETS, ASSET_COUNT, nullptr));
}

/******************************** 404 page ********************************/

// A missing page gets the 404 page
void test_missing_page_gets_the_404_page() {
	TEST_ASSERT_TRUE(gizmo::isDocumentPath("/missing", false));
	TEST_ASSERT_TRUE(gizmo::isDocumentPath("/missing.page", true));
}

// A missing file, API call or build file gets a plain 404
void test_missing_file_gets_a_plain_404() {
	TEST_ASSERT_FALSE(gizmo::isDocumentPath("/favicon.ico", false));
	TEST_ASSERT_FALSE(gizmo::isDocumentPath("/api/state", true));
	TEST_ASSERT_FALSE(gizmo::isDocumentPath("/_nuxt/missing.js", true));
}

// A dot in a folder name doesn't make a page look like a file
void test_dot_in_folder_is_still_a_page() {
	TEST_ASSERT_TRUE(gizmo::isDocumentPath("/v1.2/notes", false));
}

/******************************** Caching ********************************/

// Hashed build files are cached for a year
void test_hashed_files_are_cached() {
	TEST_ASSERT_EQUAL_STRING("public, max-age=31536000, immutable", gizmo::getAssetCacheControl("/_nuxt/entry.abc123.js"));
}

// Pages and the build manifest are checked again on every load (they keep their name between builds)
void test_pages_and_manifest_are_revalidated() {
	TEST_ASSERT_EQUAL_STRING("no-cache", gizmo::getAssetCacheControl("/"));
	TEST_ASSERT_EQUAL_STRING("no-cache", gizmo::getAssetCacheControl("/_nuxt/builds/latest.json"));
}

// Run all the embedded asset tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_assets_are_found_by_path);
	RUN_TEST(test_trailing_slash_is_ignored);
	RUN_TEST(test_prefix_is_not_a_match);
	RUN_TEST(test_missing_input_finds_nothing);
	RUN_TEST(test_missing_page_gets_the_404_page);
	RUN_TEST(test_missing_file_gets_a_plain_404);
	RUN_TEST(test_dot_in_folder_is_still_a_page);
	RUN_TEST(test_hashed_files_are_cached);
	RUN_TEST(test_pages_and_manifest_are_revalidated);
	return UNITY_END();
}