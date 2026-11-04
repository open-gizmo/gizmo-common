import { defineEventHandler } from 'h3';
import { createSuccessResponse, getGizmoMockState, handleEspMockError, scanNetworks } from '../../utils/gizmo-mock.js';

export default defineEventHandler(async event => {
    try {
        // Start a scan and return the networks found so far (the first call is empty, like on the device)
        return createSuccessResponse({ networks: scanNetworks(getGizmoMockState()) });
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'wifi_scan_failed', errorMessage: 'Unable to scan Wi-Fi networks.' });
    }
});