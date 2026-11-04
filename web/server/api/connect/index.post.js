import { defineEventHandler } from 'h3';
import { MAX_PASSWORD_LENGTH, MAX_SSID_LENGTH, createSuccessResponse, getGizmoMockState, handleEspMockError, readMockJsonBody, startMockConnection, validateCredentialField } from '../../utils/gizmo-mock.js';

export default defineEventHandler(async event => {
    try {
        // Get the request body
        const body = await readMockJsonBody(event);

        // Validate the credentials (the password can be empty for open networks)
        validateCredentialField({ fieldName: 'ssid', value: body.ssid, maxLength: MAX_SSID_LENGTH, allowEmpty: false });
        validateCredentialField({ fieldName: 'password', value: body.password, maxLength: MAX_PASSWORD_LENGTH, allowEmpty: true });

        // Start a mock connection attempt and return its status
        const connectionStatus = startMockConnection({ state: getGizmoMockState(), ssid: body.ssid, password: body.password });
        return createSuccessResponse({ connectionStatus });
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_connect_failed', errorMessage: 'Failed to start the Wi-Fi connection' });
    }
});