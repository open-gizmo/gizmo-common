import { defineEventHandler } from 'h3';
import { buildDevicePayload, createSuccessResponse, getGizmoMockState, handleEspMockError } from '../../utils/gizmo-mock.js';

export default defineEventHandler(async event => {
    try {
        // Return the details of the device
        return createSuccessResponse(buildDevicePayload({ state: getGizmoMockState() }));
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_device_read_failed', errorMessage: 'Failed to read the device details' });
    }
});