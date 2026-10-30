#ifndef GIZMO_HTTP_H
#define GIZMO_HTTP_H

// Web API helpers, only available on Arduino targets (native test builds skip them)
#ifdef ARDUINO

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

namespace gizmo {
	// Largest accepted request body (every gizmo API payload is well under 1 KB)
	constexpr size_t MAX_REQUEST_BODY_SIZE = 2048;

	// Progress of a request body that arrives in chunks
	enum class RequestBodyState : uint8_t {
		InProgress,
		Ready,
		Error,
		TooLarge,
		Ignored, // A later chunk of a body that was already rejected (its error response was already sent)
	};

	// The request body once it's complete
	struct RequestBodyResult {
		RequestBodyState state;
		const char *data;
		size_t length;
	};

	// Send a success response ({ errorId: null, errorMessage: null, data })
	void sendSuccessResponse(AsyncWebServerRequest *request, uint16_t statusCode = 200, const JsonDocument *data = nullptr);

	// Send an error response ({ errorId, errorMessage, data: null })
	void sendErrorResponse(AsyncWebServerRequest *request, uint16_t statusCode, const char *errorId, const char *errorMessage);

	// Allow same-origin requests only, and send a 403 response otherwise
	bool ensureCorsAllowed(AsyncWebServerRequest *request);

	// Answer a CORS preflight request
	void sendCorsPreflightResponse(AsyncWebServerRequest *request, const char *allowedMethods);

	// Gather a request body that arrives in chunks
	RequestBodyResult accumulateRequestBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

	// Free the memory of a gathered request body
	void releaseRequestBody(AsyncWebServerRequest *request);

	// Read a JSON object body in a body handler: true once it's parsed, false while waiting or after an error response was sent
	bool readJsonBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total, JsonDocument &doc);
}

#endif

#endif