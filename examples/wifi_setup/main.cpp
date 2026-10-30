#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <gizmo/http.h>
#include <gizmo/log.h>
#include <gizmo/wifi_setup.h>

// Web server shared by the Wi-Fi setup API and the project's own pages
AsyncWebServer server(80);

// Start logging, the Wi-Fi setup and the web server
void setup() {
	gizmo::initLog(true);

	// Join the saved network, or open the "Gizmo-Example" hotspot to set one up
	gizmo::setupWifi(server, { "Gizmo-Example", "example", "/setup", nullptr });

	// Serve the project's own setup page (a real project serves its own UI here)
	server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", "Your setup page goes here: it calls /api/networks and /api/connect.");
	});

	// Redirect hotspot clients to the setup page, and answer 404 otherwise
	server.onNotFound([](AsyncWebServerRequest *request) {
		if (gizmo::handleCaptivePortalRequest(request)) return;
		gizmo::sendErrorResponse(request, 404, "not_found", "Not found");
	});

	// Start the web server
	server.begin();
}

// Keep the Wi-Fi connected
void loop() {
	gizmo::manageWifi();
}