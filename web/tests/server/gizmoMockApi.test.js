import { IncomingMessage, ServerResponse } from 'node:http';
import { Socket } from 'node:net';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { createEvent } from 'h3';

// Load the handlers of the mock device API
const { default: getNetworks } = await import('../../server/api/networks/index.get.js');
const { default: postConnect } = await import('../../server/api/connect/index.post.js');
const { default: getCheckConnection } = await import('../../server/api/check-connection/index.get.js');
const { default: getDevice } = await import('../../server/api/device/index.get.js');
const { default: postRestart } = await import('../../server/api/restart/index.post.js');
const { default: postForgetWifi } = await import('../../server/api/forget-wifi/index.post.js');
const { buildMockHostname, configureGizmoMock, getGizmoMockState, getMockHostname, getSignalQuality, resetGizmoMockState } = await import('../../server/utils/gizmo-mock.js');

// Build a real h3 request event, with a JSON body when one is given (a string is sent as it is)
const buildEvent = (body = undefined) => {
    const request = new IncomingMessage(new Socket());
    request.method = 'POST';
    request.rawBody = Buffer.from(body === undefined ? '' : (typeof body === 'string' ? body : JSON.stringify(body)));
    return createEvent(request, new ServerResponse(request));
};

// Call a handler and return its answer with the status code it set
const callHandler = async (handler, event = buildEvent()) => {
    const response = await handler(event);
    return { statusCode: event.node.res.statusCode, response };
};

// Start every test from the demo device the dev server boots with
beforeEach(() => {
    resetGizmoMockState();
    configureGizmoMock({ name: 'Holiday Strip' });
});

// Response shape: the web app reads the same { errorId, errorMessage, data } as on the device
describe('mock device responses', () => {
    // A success has no error and the data
    it('answers a success like the firmware', async () => {
        const { statusCode, response } = await callHandler(getDevice);
        expect(statusCode).toBe(200);
        expect(response.errorId).toBeNull();
        expect(response.errorMessage).toBeNull();
        expect(response.data.name).toBe('Holiday Strip');
    });

    // The mock used to send h3 errors, so the app never found the errorId and never showed its translated device errors
    it('answers an error as the firmware body with its status code', async () => {
        const { statusCode, response } = await callHandler(postConnect, buildEvent({ ssid: '', password: '' }));
        expect(statusCode).toBe(400);
        expect(response).toEqual({ errorId: 'missing_required_fields', errorMessage: 'ssid', data: null });
    });

    // Bodies are checked like gizmo-common reads them
    it('refuses invalid, non-object and oversized bodies', async () => {
        const invalidJson = await callHandler(postConnect, buildEvent('{nope'));
        const arrayBody = await callHandler(postConnect, buildEvent([1]));
        const largeBody = await callHandler(postConnect, buildEvent({ ssid: 'Home', password: 'a'.repeat(2100) }));
        expect([invalidJson.statusCode, invalidJson.response.errorId, invalidJson.response.errorMessage]).toEqual([400, 'invalid_json', 'Invalid JSON']);
        expect([arrayBody.statusCode, arrayBody.response.errorMessage]).toEqual([400, 'Invalid JSON object']);
        expect([largeBody.statusCode, largeBody.response.errorId]).toEqual([413, 'request_body_too_large']);
    });
});

// GET /api/networks
describe('mock Wi-Fi scan', () => {
    // Like gizmo-common, the scan started by the first call is not done yet
    it('answers the first scan empty, then the networks', async () => {
        const firstScan = await callHandler(getNetworks);
        const secondScan = await callHandler(getNetworks);
        expect(firstScan.response.data.networks).toEqual([]);
        expect(secondScan.response.data.networks.length).toBeGreaterThan(0);
        expect(secondScan.response.data.networks[0]).toEqual({ ssid: 'BrumaLab WiFi', signal: -48, quality: 100, channel: 11, secured: true });
    });
});

// POST /api/connect: the validation of gizmo-common
describe('mock Wi-Fi credentials validation', () => {
    // Send credentials and return the answer
    const send = body => callHandler(postConnect, buildEvent(body));

    // The SSID is required and cannot be empty
    it('requires a network name', async () => {
        expect((await send({ password: '' })).response).toEqual({ errorId: 'missing_required_fields', errorMessage: 'ssid', data: null });
        expect((await send({ ssid: '', password: '' })).response).toEqual({ errorId: 'missing_required_fields', errorMessage: 'ssid', data: null });
        expect((await send({ ssid: null, password: '' })).response.errorId).toBe('missing_required_fields');
    });

    // The password must be sent, but it can be empty (open networks)
    it('requires the password field, and accepts it empty', async () => {
        const missing = await send({ ssid: 'Office Fiber' });
        expect(missing.statusCode).toBe(400);
        expect(missing.response).toEqual({ errorId: 'missing_required_fields', errorMessage: 'password', data: null });
        const empty = await send({ ssid: 'Warehouse Guest', password: '' });
        expect(empty.statusCode).toBe(200);
    });

    // The SSID is up to 32 bytes and the password up to 64, and a longer one is invalid
    it('accepts exactly the longest SSID and password, and refuses one byte more', async () => {
        expect((await send({ ssid: 'a'.repeat(32), password: 'b'.repeat(64) })).statusCode).toBe(200);
        expect((await send({ ssid: 'a'.repeat(33), password: '' })).response).toEqual({ errorId: 'invalid_connection_field', errorMessage: 'ssid', data: null });
        expect((await send({ ssid: 'Home', password: 'b'.repeat(65) })).response).toEqual({ errorId: 'invalid_connection_field', errorMessage: 'password', data: null });
    });

    // The device counts bytes (strlen), not characters: sixteen accented letters are 32 bytes, seventeen are 34
    it('counts the limits in bytes', async () => {
        expect((await send({ ssid: 'è'.repeat(16), password: '' })).statusCode).toBe(200);
        expect((await send({ ssid: 'è'.repeat(17), password: '' })).response.errorId).toBe('invalid_connection_field');
    });

    // A value that is not a string is not valid, whatever its size
    it('refuses fields that are not strings', async () => {
        expect((await send({ ssid: 123, password: '' })).response).toEqual({ errorId: 'invalid_connection_field', errorMessage: 'ssid', data: null });
        expect((await send({ ssid: 'Home', password: ['x'] })).response).toEqual({ errorId: 'invalid_connection_field', errorMessage: 'password', data: null });
    });

    // The ssid is checked before the password, as the firmware does
    it('reports the network name first', async () => {
        expect((await send({ ssid: 'a'.repeat(40), password: 'b'.repeat(70) })).response.errorMessage).toBe('ssid');
    });

    // A refused request must not start an attempt
    it('starts no connection when the credentials are refused', async () => {
        await send({ ssid: 'a'.repeat(33), password: '' });
        expect(getGizmoMockState().pendingConnection).toBeNull();
        expect(getGizmoMockState().connectionStatus).toBe(1);
    });
});

// POST /api/connect and GET /api/check-connection: the connection flow
describe('mock Wi-Fi connection', () => {
    // Good credentials connect after a few polls and are saved
    it('connects and saves the new network', async () => {
        // Start the attempt
        const { response } = await callHandler(postConnect, buildEvent({ ssid: 'Warehouse Guest', password: '' }));
        expect(response.data.connectionStatus).toBe(2);

        // Poll until the attempt ends
        const statuses = [];
        for (let poll = 0; poll < 3; poll += 1) {
            const { response: pollResponse } = await callHandler(getCheckConnection);
            statuses.push(pollResponse.data.connectionStatus);
        }

        // Expect two polls while connecting, then the connection on the new network
        expect(statuses).toEqual([2, 2, 1]);
        const { response: deviceResponse } = await callHandler(getDevice);
        expect(deviceResponse.data.ssid).toBe('Warehouse Guest');
    });

    // The answer of a connected device has the network, the address and the hostname of the app
    it('answers the network, the address and the hostname once connected', async () => {
        const { response } = await callHandler(getCheckConnection);
        expect(response.data).toEqual({ connectionStatus: 1, ssid: 'BrumaLab WiFi', ip: '192.168.1.42', hostname: 'holiday-strip.local' });
    });

    // Wrong credentials fail once, then the device is back on the saved network
    it('goes back to the saved network after a failed attempt', async () => {
        // Try a wrong password
        await callHandler(postConnect, buildEvent({ ssid: 'Office Fiber', password: 'wrong' }));

        // Poll through the failure
        const statuses = [];
        for (let poll = 0; poll < 4; poll += 1) {
            const { response } = await callHandler(getCheckConnection);
            statuses.push(response.data.connectionStatus);
        }

        // Expect the failure, then the saved network again
        expect(statuses).toEqual([2, 2, 0, 1]);
        const { response } = await callHandler(getCheckConnection);
        expect(response.data.ssid).toBe('BrumaLab WiFi');
    });

    // The device with no network sets up like the firmware: after the connection the setup hotspot closes
    it('closes the setup hotspot once the setup page has seen the connection', async () => {
        // Forget the network and let the device restart into its hotspot
        vi.useFakeTimers();
        await callHandler(postForgetWifi);
        vi.advanceTimersByTime(1000);
        vi.useRealTimers();
        expect((await callHandler(getDevice)).response.data.accessPointActive).toBe(true);

        // Connect from the setup page
        await callHandler(postConnect, buildEvent({ ssid: 'Office Fiber', password: 'secret' }));
        for (let poll = 0; poll < 3; poll += 1) await callHandler(getCheckConnection);

        // Expect the hotspot closed and the network saved
        const { response } = await callHandler(getDevice);
        expect(response.data).toMatchObject({ ssid: 'Office Fiber', accessPointActive: false });
    });
});

// GET /api/device: the contract of device.cpp
describe('mock device details', () => {
    // The web app reads all of these fields
    it('answers every field of the firmware', async () => {
        const { response } = await callHandler(getDevice);
        expect(Object.keys(response.data).sort()).toEqual(['accessPointActive', 'freeHeap', 'hostname', 'ip', 'name', 'signalQuality', 'ssid', 'uptimeSeconds', 'version']);
        expect(response.data).toMatchObject({
            name: 'Holiday Strip',
            ssid: 'BrumaLab WiFi',
            accessPointActive: false,
            ip: '192.168.1.42',
            hostname: 'holiday-strip.local',
            signalQuality: 100
        });
        expect(typeof response.data.version).toBe('string');
        expect(response.data.freeHeap).toBeGreaterThan(0);
    });

    // The app names the mock device (from a Nitro plugin), so every project's mock introduces itself
    it('uses the name the app configured', async () => {
        configureGizmoMock({ name: 'Gizmo Ticker' });
        const { response } = await callHandler(getDevice);
        expect(response.data).toMatchObject({ name: 'Gizmo Ticker', hostname: 'gizmo-ticker.local' });
    });

    // The uptime counts the seconds since the boot
    it('counts the uptime from the boot', async () => {
        getGizmoMockState().startedAt = Date.now() - 93784000;
        const { response } = await callHandler(getDevice);
        expect(response.data.uptimeSeconds).toBeGreaterThanOrEqual(93784);
        expect(response.data.uptimeSeconds).toBeLessThan(93790);
    });

    // The firmware sends the address, the hostname and the signal only once connected
    it('leaves out the address and the signal while the device is not connected', async () => {
        const state = getGizmoMockState();
        state.connectionStatus = 0;
        const { response } = await callHandler(getDevice);
        expect(response.data).not.toHaveProperty('ip');
        expect(response.data).not.toHaveProperty('hostname');
        expect(response.data).not.toHaveProperty('signalQuality');
        expect(response.data.ssid).toBe('BrumaLab WiFi');
    });

    // The signal of the saved network is turned into a quality like device.cpp does
    it('turns the signal of the saved network into a quality', async () => {
        getGizmoMockState().savedSsid = 'Office Fiber';
        const { response } = await callHandler(getDevice);
        expect(response.data.signalQuality).toBe(74);
    });

    // The quality goes from 0 to 100 whatever the signal, with the same ends as the firmware
    it('keeps the signal quality between 0 and 100', () => {
        expect([getSignalQuality(-30), getSignalQuality(-50), getSignalQuality(-75), getSignalQuality(-100), getSignalQuality(-120)]).toEqual([100, 100, 50, 0, 0]);
    });

    // The hostname is the name of the project in lowercase, with dashes
    it('builds the hostname from the name', () => {
        expect(buildMockHostname('Holiday Strip')).toBe('holiday-strip.local');
        expect(buildMockHostname('  Gizmo   Reolink Switch! ')).toBe('gizmo-reolink-switch.local');
        expect(buildMockHostname(undefined)).toBe('gizmo.local');
        expect(buildMockHostname('')).toBe('gizmo.local');
    });

    // An app whose firmware hostname isn't its display name sets the hostname itself
    it('uses the firmware hostname the app sets', () => {
        configureGizmoMock({ name: 'Reolink Switch', hostname: 'gizmo-reolink-switch' });
        expect(getMockHostname(getGizmoMockState())).toBe('gizmo-reolink-switch.local');
        configureGizmoMock({ name: 'Holiday Strip' });
        expect(getMockHostname(getGizmoMockState())).toBe('holiday-strip.local');
    });
});

// POST /api/restart and POST /api/forget-wifi
describe('mock restart and forget network', () => {
    // The mock restarts a second after the answer, like scheduleRestart(): use a fake clock to wait for it
    beforeEach(() => {
        vi.useFakeTimers();
    });
    afterEach(() => {
        vi.useRealTimers();
    });

    // The answer comes first, the restart a second later
    it('restarts about a second after answering', async () => {
        const state = getGizmoMockState();
        state.startedAt = Date.now() - 600000;
        state.hasScanned = true;

        // Expect a success without data, and nothing changed yet
        const { statusCode, response } = await callHandler(postRestart);
        expect(statusCode).toBe(200);
        expect(response).toEqual({ errorId: null, errorMessage: null, data: null });
        expect((await callHandler(getDevice)).response.data.uptimeSeconds).toBeGreaterThanOrEqual(600);

        // Expect the uptime back to zero, the network kept, and the first scan empty again
        vi.advanceTimersByTime(1000);
        const { response: deviceResponse } = await callHandler(getDevice);
        expect(deviceResponse.data.uptimeSeconds).toBe(0);
        expect(deviceResponse.data).toMatchObject({ ssid: 'BrumaLab WiFi', accessPointActive: false });
        expect((await callHandler(getNetworks)).response.data.networks).toEqual([]);
    });

    // The credentials go at once, the device restarts into its setup hotspot a second later
    it('forgets the network and restarts into the setup hotspot', async () => {
        // Expect a success without data, and the network gone at once
        const { statusCode, response } = await callHandler(postForgetWifi);
        expect(statusCode).toBe(200);
        expect(response).toEqual({ errorId: null, errorMessage: null, data: null });
        expect((await callHandler(getDevice)).response.data.ssid).toBe('');

        // Expect the hotspot open and no address after the restart
        vi.advanceTimersByTime(1000);
        const { response: deviceResponse } = await callHandler(getDevice);
        expect(deviceResponse.data).toMatchObject({ ssid: '', accessPointActive: true });
        expect(deviceResponse.data).not.toHaveProperty('ip');
        expect((await callHandler(getCheckConnection)).response.data).toEqual({ connectionStatus: 0 });
    });

    // The firmware answers wifi_forget_failed (500) when it can't clear the credentials, and does not restart
    it('answers wifi_forget_failed when the network cannot be forgotten', async () => {
        getGizmoMockState().isForgetFailing = true;
        const { statusCode, response } = await callHandler(postForgetWifi);
        expect(statusCode).toBe(500);
        expect(response).toEqual({ errorId: 'wifi_forget_failed', errorMessage: 'Failed to forget the network', data: null });

        // Expect the network kept and no restart
        vi.advanceTimersByTime(5000);
        expect((await callHandler(getDevice)).response.data).toMatchObject({ ssid: 'BrumaLab WiFi', accessPointActive: false });
    });
});