#ifndef GIZMO_EMBEDDED_ASSETS_H
#define GIZMO_EMBEDDED_ASSETS_H

#include <stddef.h>
#include <stdint.h>

#ifdef ARDUINO
#include <ESPAsyncWebServer.h>
#endif

namespace gizmo {
	// A gzipped web file built into the firmware (the gizmo-embed tool generates the list from a static site)
	struct EmbeddedAsset {
		const char *routePath; // Path the file is served at, for example "/" or "/_nuxt/app.js"
		const char *contentType; // MIME type of the original file
		const uint8_t *data; // Gzipped contents
		size_t size; // Size of the gzipped contents
	};

	// Find the asset served at a path, ignoring one trailing slash (nullptr if there is none)
	const EmbeddedAsset *findEmbeddedAsset(const EmbeddedAsset *assets, size_t count, const char *path);

	// Check whether a missing path should get the 404 page: a page request, not an API call or a missing script
	bool isDocumentPath(const char *path, bool acceptsHtml);

	// Get the Cache-Control value of a path (hashed build files are cached for a year, everything else is revalidated)
	const char *getAssetCacheControl(const char *path);

#ifdef ARDUINO
	// Serve an embedded asset for a GET or HEAD request, or the 404 page for a missing page (call in the server's not-found handler; true if it answered)
	bool handleEmbeddedAssetRequest(AsyncWebServerRequest *request, const EmbeddedAsset *assets, size_t count, const char *notFoundPath = "/404.html");
#endif
}

#endif