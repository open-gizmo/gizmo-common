#include <Arduino.h>
#include <gizmo/format.h>
#include <gizmo/log.h>

// Show the library at work: log a formatted duration and price once per second
void setup() {
	gizmo::initLog(true);
	gizmo::printLogln("gizmo-common basic example");
}

// Log the uptime and a sample price
void loop() {
	char duration[gizmo::DURATION_TEXT_SIZE];
	char price[24];

	// Format the uptime and a small price
	gizmo::formatDuration(millis() / 1000, duration, sizeof(duration));
	gizmo::formatNumber(0.08123, gizmo::getSignificantDecimals(0.08123), gizmo::NumberFormat::US, price, sizeof(price));

	// Log both and wait a second
	gizmo::printLogfln("Uptime %s, price %s", duration, price);
	delay(1000);
}