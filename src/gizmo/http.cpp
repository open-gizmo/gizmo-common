#include "http.h"

// Web API helpers, only available on Arduino targets (native test builds skip them)
#ifdef ARDUINO

namespace {
	constexpr const char *DEFAULT_CORS_HEADERS = "Content-Type";
	constexpr const char *DEFAULT_CORS_METHODS = "GET, POST, OPTIONS";

	// Get the value of a specific header
	String getHeaderValue(AsyncWebServerRequest *request, const char *headerName) {
		if (!request->hasHeader(headerName)) return String();
		return request->getHeader(headerName)->value();
	}

	// Validate if the CORS origin is allowed (same origin only)
	bool isAllowedCorsOrigin(AsyncWebServerRequest *request) {
		const String origin = getHeaderValue(request, "Origin");

		// Allow if no Origin header is present (standard for same-origin requests)
		if (origin.length() == 0) {
			return true;
		}

		// Block opaque origins
		if (origin == "null") {
			return false;
		}

		// Allow if Origin matches the Host header (same-origin)
		const String host = getHeaderValue(request, "Host");
		return host.length() != 0 && (origin == "http://" + host || origin == "https://" + host);
	}

	// Append CORS headers to the response
	void appendCorsHeaders(AsyncWebServerRequest *request, AsyncWebServerResponse *response, const char *allowedMethods) {
		// Only append CORS headers if the Origin is allowed
		const String origin = getHeaderValue(request, "Origin");
		if (origin.length() == 0 || !isAllowedCorsOrigin(request)) {
			return;
		}

		// Append the appropriate CORS headers
		response->addHeader("Access-Control-Allow-Origin", origin);
		response->addHeader("Vary", "Origin");
		response->addHeader("Access-Control-Allow-Methods", allowedMethods);
		response->addHeader("Access-Control-Allow-Headers", DEFAULT_CORS_HEADERS);
		response->addHeader("Access-Control-Max-Age", "600");
	}

	// Initialize the standard response structure
	void initializeResponse(JsonDocument &doc) {
		doc.clear();
		doc["errorId"] = nullptr;
		doc["errorMessage"] = nullptr;
		doc["data"] = nullptr;
	}

	// Serialize the JSON document and send the response
	void sendResponse(AsyncWebServerRequest *request, uint16_t statusCode, JsonDocument &doc) {
		String jsonResponse;
		serializeJson(doc, jsonResponse);
		AsyncWebServerResponse *response = request->beginResponse(statusCode, "application/json", jsonResponse);
		appendCorsHeaders(request, response, DEFAULT_CORS_METHODS);
		request->send(response);
	}
}

namespace gizmo {
	// Send a success response ({ errorId: null, errorMessage: null, data })
	void sendSuccessResponse(AsyncWebServerRequest *request, uint16_t statusCode, const JsonDocument *data) {
		JsonDocument responseDoc;
		initializeResponse(responseDoc);

		// Add the data object if provided
		if (data != nullptr) {
			responseDoc["data"].set(data->as<JsonVariantConst>());
		}

		// Send the response
		sendResponse(request, statusCode, responseDoc);
	}

	// Send an error response ({ errorId, errorMessage, data: null })
	void sendErrorResponse(AsyncWebServerRequest *request, uint16_t statusCode, const char *errorId, const char *errorMessage) {
		JsonDocument doc;
		initializeResponse(doc);

		// Populate the error details
		doc["errorId"] = errorId;
		doc["errorMessage"] = errorMessage;

		// Send the response
		sendResponse(request, statusCode, doc);
	}

	// Allow same-origin requests only, and send a 403 response otherwise
	bool ensureCorsAllowed(AsyncWebServerRequest *request) {
		if (isAllowedCorsOrigin(request)) {
			return true;
		}

		// Send error response for disallowed CORS origin
		sendErrorResponse(request, 403, "cors_origin_not_allowed", "CORS origin not allowed");
		return false;
	}

	// Answer a CORS preflight request
	void sendCorsPreflightResponse(AsyncWebServerRequest *request, const char *allowedMethods) {
		// Check if CORS is allowed for this request
		if (!isAllowedCorsOrigin(request)) {
			sendErrorResponse(request, 403, "cors_origin_not_allowed", "CORS origin not allowed");
			return;
		}

		// Send a 204 No Content response with the appropriate CORS headers
		AsyncWebServerResponse *response = request->beginResponse(204);
		appendCorsHeaders(request, response, allowedMethods);
		request->send(response);
	}

	// Gather a request body that arrives in chunks
	RequestBodyResult accumulateRequestBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
		// If it's the first chunk, allocate memory for the entire body
		if (index == 0) {
			releaseRequestBody(request);

			// Reject bodies larger than any valid API payload before allocating them
			if (total > MAX_REQUEST_BODY_SIZE) {
				return { RequestBodyState::TooLarge, nullptr, 0 };
			}

			// Allocate the body buffer
			request->_tempObject = malloc(total + 1);
			if (request->_tempObject == nullptr) {
				return { RequestBodyState::Error, nullptr, 0 };
			}
		}

		// If the first chunk was rejected, ignore the rest of the body (the error response was already sent)
		if (request->_tempObject == nullptr) {
			return { RequestBodyState::Ignored, nullptr, 0 };
		}

		// Copy the current chunk into the allocated memory
		memcpy(static_cast<uint8_t *>(request->_tempObject) + index, data, len);

		// If still receiving data, wait for the next call
		if (index + len != total) {
			return { RequestBodyState::InProgress, nullptr, 0 };
		}

		// All data received, return the complete body
		char *body = static_cast<char *>(request->_tempObject);
		body[total] = '\0';
		return { RequestBodyState::Ready, body, total };
	}

	// Free the memory of a gathered request body
	void releaseRequestBody(AsyncWebServerRequest *request) {
		if (request->_tempObject == nullptr) return;
		free(request->_tempObject);
		request->_tempObject = nullptr;
	}

	// Read a JSON object body in a body handler: true once it's parsed, false while waiting or after an error response was sent
	bool readJsonBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total, JsonDocument &doc) {
		// Validate CORS on the first chunk only (so a rejected request gets a single response)
		if (index == 0 && !ensureCorsAllowed(request)) {
			return false;
		}

		// Accumulate the request body (later chunks of a rejected body are ignored)
		const RequestBodyResult body = accumulateRequestBody(request, data, len, index, total);
		if (body.state == RequestBodyState::InProgress || body.state == RequestBodyState::Ignored) {
			return false;
		}

		// If the body is too large, send error response
		if (body.state == RequestBodyState::TooLarge) {
			sendErrorResponse(request, 413, "request_body_too_large", "Request body too large");
			return false;
		}

		// If the body couldn't be read, send error response
		if (body.state == RequestBodyState::Error) {
			sendErrorResponse(request, 500, "request_body_read_failed", "Failed to read request body");
			return false;
		}

		// Parse the JSON body and release its memory
		const DeserializationError error = deserializeJson(doc, body.data, body.length);
		releaseRequestBody(request);

		// If error, send error response
		if (error) {
			sendErrorResponse(request, 400, "invalid_json", "Invalid JSON");
			return false;
		}

		// Validate that the root is a JSON object
		if (!doc.is<JsonObject>()) {
			sendErrorResponse(request, 400, "invalid_json", "Invalid JSON object");
			return false;
		}

		// Body ready
		return true;
	}
}

#endif