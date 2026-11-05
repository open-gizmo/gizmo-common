import { readRawBody, setResponseStatus } from 'h3';

// Limits of the firmware (gizmo-common), counted in bytes like the C strings on the device
export const MAX_REQUEST_BODY_SIZE = 2048;
export const MAX_SSID_LENGTH = 32;
export const MAX_PASSWORD_LENGTH = 64;
export const MOCK_IP = '192.168.1.42';
export const MOCK_VERSION = 'dev';
export const MOCK_FREE_HEAP = 187432;

// Time the device waits to send the last response before it restarts (gizmo-common, device.cpp)
export const RESTART_DELAY_MS = 1000;

// Wi-Fi connection status codes of gizmo-common
export const wifiStatus = Object.freeze({
    DISCONNECTED: 0,
    CONNECTED: 1,
    CONNECTING: 2
});

// Networks found by the mock scan
const scannedNetworks = () => ([
    { ssid: 'BrumaLab WiFi', signal: -48, quality: 100, channel: 11, secured: true },
    { ssid: 'Office Fiber', signal: -63, quality: 74, channel: 6, secured: true },
    { ssid: 'Warehouse Guest', signal: -71, quality: 58, channel: 1, secured: false },
    { ssid: 'ESP-Test-Hotspot', signal: -83, quality: 34, channel: 3, secured: true }
]);

// State of the demo device the dev server starts with (already set up on a network): the apps add their own fields to it
const createState = () => ({
    name: 'Gizmo', // Name the device introduces itself with (an app sets it with configureGizmoMock)
    hostname: '', // mDNS name of the firmware, without .local (an app sets it with configureGizmoMock; empty builds it from the name)
    startedAt: Date.now(),
    savedSsid: 'BrumaLab WiFi',
    isAccessPointActive: false, // The setup hotspot is only open while the device has no working network
    isForgetFailing: false, // Makes "forget network" fail like a device that can't write its flash (for tests)
    hasScanned: false, // The first scan after boot comes back empty, like on the device
    pendingConnection: null,
    isFallbackPending: false, // A failed attempt goes back to the saved network on a later poll
    connectionStatus: wifiStatus.CONNECTED
});

// Get the mock device state (kept across hot reloads)
export const getGizmoMockState = () => {
    if (!globalThis.__gizmoMockState) {
        globalThis.__gizmoMockState = createState();
    }

    return globalThis.__gizmoMockState;
};

// Name the mock device like the app and its firmware (call it once when the dev server starts, from a Nitro plugin of the app)
export const configureGizmoMock = ({ name, hostname = '' }) => {
    const state = getGizmoMockState();
    state.name = name;
    state.hostname = hostname;
};

// Start again from the demo device (the dev server never needs it, the tests do)
export const resetGizmoMockState = () => {
    delete globalThis.__gizmoMockState;
};

// Return the networks found by the scan (empty on the first call, while the device is still scanning)
export const scanNetworks = state => {
    if (!state.hasScanned) {
        state.hasScanned = true;
        return [];
    }

    return scannedNetworks();
};

// Length of a string in bytes, like strlen() on the device
export const byteLength = text => Buffer.byteLength(text, 'utf8');

// Success response of the firmware ({ errorId: null, errorMessage: null, data })
export const createSuccessResponse = (data = null) => ({
    errorId: null,
    errorMessage: null,
    data
});

// Error of the mock device, turned into the firmware error response by handleEspMockError
export class EspMockError extends Error {
    constructor(statusCode, errorId, errorMessage) {
        super(errorMessage);
        this.statusCode = statusCode;
        this.errorId = errorId;
    }
}

// Send the firmware error response ({ errorId, errorMessage, data: null }) with its status code
export const handleEspMockError = ({ event, error, errorId, errorMessage }) => {
    const mockError = error instanceof EspMockError ? error : new EspMockError(500, errorId, errorMessage);
    setResponseStatus(event, mockError.statusCode);
    return {
        errorId: mockError.errorId,
        errorMessage: mockError.message,
        data: null
    };
};

// Read the JSON object of a request body with the same checks as the firmware
export const readMockJsonBody = async event => {
    const rawBody = (await readRawBody(event, 'utf8')) || '';

    // The body must fit the firmware buffer
    if (byteLength(rawBody) > MAX_REQUEST_BODY_SIZE) {
        throw new EspMockError(413, 'request_body_too_large', 'Request body too large');
    }

    // The body must be valid JSON
    let body;
    try {
        body = JSON.parse(rawBody);
    } catch {
        throw new EspMockError(400, 'invalid_json', 'Invalid JSON');
    }

    // The body must be an object
    if (!body || typeof body !== 'object' || Array.isArray(body)) {
        throw new EspMockError(400, 'invalid_json', 'Invalid JSON object');
    }

    return body;
};

/******************************** Wi-Fi ********************************/

// Validate one credential field like gizmo-common
export const validateCredentialField = ({ fieldName, value, maxLength, allowEmpty }) => {
    // The field must be present
    if (value == null || (!allowEmpty && value === '')) {
        throw new EspMockError(400, 'missing_required_fields', fieldName);
    }

    // The field must be a string that fits
    if (typeof value !== 'string' || byteLength(value) > maxLength) {
        throw new EspMockError(400, 'invalid_connection_field', fieldName);
    }
};

// Start a mock connection attempt (the password "wrong" simulates a failure) and return the status like the firmware
export const startMockConnection = ({ state, ssid, password }) => {
    state.pendingConnection = {
        ssid,
        attemptsRemaining: 2,
        shouldFail: password.toLowerCase() === 'wrong'
    };
    state.isFallbackPending = false;
    state.connectionStatus = wifiStatus.CONNECTING;
    return state.connectionStatus;
};

// Advance the mock connection attempt (a couple of polls while connecting, then the result)
export const advanceMockConnection = state => {
    if (state.pendingConnection) {
        if (state.pendingConnection.attemptsRemaining > 0) {
            state.pendingConnection.attemptsRemaining -= 1;
        } else if (state.pendingConnection.shouldFail) {
            // New credentials that fail are dropped, and the device goes back to the saved network later
            state.connectionStatus = wifiStatus.DISCONNECTED;
            state.pendingConnection = null;
            state.isFallbackPending = Boolean(state.savedSsid);
        } else {
            // Credentials are saved only once they connect
            state.connectionStatus = wifiStatus.CONNECTED;
            state.savedSsid = state.pendingConnection.ssid;
            state.pendingConnection = null;
        }
    } else if (state.isFallbackPending) {
        // Back on the saved network after a failed attempt
        state.connectionStatus = wifiStatus.CONNECTED;
        state.isFallbackPending = false;
    }
};

/******************************** Device ********************************/

// Turn an RSSI into a signal quality from 0 to 100 (-100 dBm or less is 0, -50 dBm or more is 100), like device.cpp
export const getSignalQuality = rssi => {
    if (rssi <= -100) return 0;
    if (rssi >= -50) return 100;
    return 2 * (rssi + 100);
};

// Hostname of the device: its name in lowercase letters and digits, as the project sets it (for example "Holiday Strip" is "holiday-strip")
export const buildMockHostname = name => {
    const slug = String(name ?? '').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '');
    return `${slug || 'gizmo'}.local`;
};

// Get the hostname the mock device answers with: the firmware's one when the app set it, otherwise built from the name
export const getMockHostname = state => {
    return state.hostname ? `${state.hostname}.local` : buildMockHostname(state.name);
};

// Build the details of the device like GET /api/device (the address, the hostname and the signal only once connected)
export const buildDevicePayload = ({ state, now = Date.now() }) => {
    const payload = {
        name: state.name,
        version: MOCK_VERSION,
        uptimeSeconds: Math.floor((now - state.startedAt) / 1000),
        freeHeap: MOCK_FREE_HEAP,
        ssid: state.savedSsid,
        accessPointActive: state.isAccessPointActive
    };

    // Add the address and the signal once connected
    if (state.connectionStatus === wifiStatus.CONNECTED) {
        const savedNetwork = scannedNetworks().find(network => network.ssid === state.savedSsid);
        payload.ip = MOCK_IP;
        payload.hostname = getMockHostname(state);
        payload.signalQuality = getSignalQuality(savedNetwork?.signal ?? -60);
    }

    return payload;
};

// Restart the mock device: it boots again on its saved network, or opens the setup hotspot when it has none
export const restartMockDevice = state => {
    state.startedAt = Date.now();
    state.pendingConnection = null;
    state.isFallbackPending = false;
    state.hasScanned = false;
    state.connectionStatus = state.savedSsid ? wifiStatus.CONNECTED : wifiStatus.DISCONNECTED;
    state.isAccessPointActive = !state.savedSsid;
};

// Restart the mock device once the response has been sent, like scheduleRestart() (the timer does not keep the dev server alive)
export const scheduleMockRestart = state => {
    const timer = setTimeout(() => restartMockDevice(state), RESTART_DELAY_MS);
    timer.unref?.();
};

// Forget the saved network: the credentials go at once, and the device restarts into its setup hotspot (throws wifi_forget_failed when the flash can't be written)
export const forgetMockNetwork = state => {
    if (state.isForgetFailing) {
        throw new EspMockError(500, 'wifi_forget_failed', 'Failed to forget the network');
    }

    state.savedSsid = '';
    scheduleMockRestart(state);
};