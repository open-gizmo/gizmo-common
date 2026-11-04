import { defineEventHandler } from 'h3';
import { createSuccessResponse, getGizmoMockState, handleEspMockError, scheduleMockRestart } from '../../utils/gizmo-mock.js';

export default defineEventHandler(async event => {
    try {
        // Answer first, then restart about a second later, like the firmware
        scheduleMockRestart(getGizmoMockState());
        return createSuccessResponse();
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_restart_failed', errorMessage: 'Failed to restart the device' });
    }
});