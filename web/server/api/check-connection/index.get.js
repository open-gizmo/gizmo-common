import { defineEventHandler } from 'h3';
import { MOCK_IP, advanceMockConnection, createSuccessResponse, getGizmoMockState, getMockHostname, handleEspMockError, wifiStatus } from '../../utils/gizmo-mock.js';

export default defineEventHandler(async event => {
    try {
        // Advance the mock connection attempt, if there is one
        const state = getGizmoMockState();
        advanceMockConnection(state);

        // Return the network, address and hostname once connected (the setup hotspot closes: the setup page has its answer)
        if (state.connectionStatus === wifiStatus.CONNECTED) {
            state.isAccessPointActive = false;
            return createSuccessResponse({
                connectionStatus: state.connectionStatus,
                ssid: state.savedSsid,
                ip: MOCK_IP,
                hostname: getMockHostname(state)
            });
        }

        // Otherwise return the current connection state
        return createSuccessResponse({ connectionStatus: state.connectionStatus });
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_connection_status_failed', errorMessage: 'Failed to read the connection status' });
    }
});