#include "embedded_assets.h"

#include <string.h>

namespace {
	// Prefix of the files the web app build puts a content hash in
	constexpr const char *HASHED_ASSET_PREFIX = "/_nuxt/";

	// Prefix of the build manifest, which keeps its name between builds
	constexpr const char *BUILD_MANIFEST_PREFIX = "/_nuxt/builds/";

	// Prefix of the API routes
	constexpr const char *API_PREFIX = "/api/";

	// Check whether a text starts with a prefix
	bool startsWith(const char *text, const char *prefix) {
		return strncmp(text, prefix, strlen(prefix)) == 0;
	}
}

namespace gizmo {
	// Find the asset served at a path, ignoring one trailing slash (nullptr if there is none)
	const EmbeddedAsset *findEmbeddedAsset(const EmbeddedAsset *assets, size_t count, const char *path) {
		if (assets == nullptr || path == nullptr) {
			return nullptr;
		}

		// Ignore one trailing slash, except on the root path
		size_t pathLength = strlen(path);
		if (pathLength > 1 && path[pathLength - 1] == '/') {
			pathLength -= 1;
		}

		// Compare the path with every asset
		for (size_t i = 0; i < count; i++) {
			const char *routePath = assets[i].routePath;
			if (strlen(routePath) == pathLength && strncmp(routePath, path, pathLength) == 0) {
				return &assets[i];
			}
		}

		// No asset at this path
		return nullptr;
	}

	// Check whether a missing path should get the 404 page: a page request, not an API call or a missing script
	bool isDocumentPath(const char *path, bool acceptsHtml) {
		if (path == nullptr) {
			return false;
		}

		// API calls and build files get a plain 404
		if (startsWith(path, API_PREFIX) || startsWith(path, HASHED_ASSET_PREFIX)) {
			return false;
		}

		// A browser asking for a page
		if (acceptsHtml) {
			return true;
		}

		// A path without a file extension in its last part
		const char *lastSlash = strrchr(path, '/');
		const char *lastDot = strrchr(path, '.');
		return lastDot == nullptr || (lastSlash != nullptr && lastDot < lastSlash);
	}

	// Get the Cache-Control value of a path (hashed build files are cached for a year, everything else is revalidated)
	const char *getAssetCacheControl(const char *path) {
		if (path != nullptr && startsWith(path, HASHED_ASSET_PREFIX) && !startsWith(path, BUILD_MANIFEST_PREFIX)) {
			return "public, max-age=31536000, immutable";
		}
		return "no-cache";
	}

#ifdef ARDUINO
	// Serve an embedded asset for a GET or HEAD request, or the 404 page for a missing page (call in the server's not-found handler; true if it answered)
	bool handleEmbeddedAssetRequest(AsyncWebServerRequest *request, const EmbeddedAsset *assets, size_t count, const char *notFoundPath) {
		if (request->method() != HTTP_GET && request->method() != HTTP_HEAD) {
			return false;
		}

		// Find the asset at the requested path
		const String &path = request->url();
		const EmbeddedAsset *asset = findEmbeddedAsset(assets, count, path.c_str());
		uint16_t statusCode = 200;

		// Fall back to the 404 page for a missing page
		if (asset == nullptr) {
			const AsyncWebHeader *acceptHeader = request->getHeader("Accept");
			const bool acceptsHtml = acceptHeader != nullptr && acceptHeader->value().indexOf("text/html") >= 0;
			if (notFoundPath == nullptr || !isDocumentPath(path.c_str(), acceptsHtml)) {
				return false;
			}
			asset = findEmbeddedAsset(assets, count, notFoundPath);
			if (asset == nullptr) {
				return false;
			}
			statusCode = 404;
		}

		// Send the gzipped asset with its cache header
		AsyncWebServerResponse *response = request->beginResponse(statusCode, asset->contentType, asset->data, asset->size);
		response->addHeader("Content-Encoding", "gzip");
		response->addHeader("Cache-Control", statusCode == 200 ? getAssetCacheControl(asset->routePath) : "no-cache");
		request->send(response);
		return true;
	}
#endif
}