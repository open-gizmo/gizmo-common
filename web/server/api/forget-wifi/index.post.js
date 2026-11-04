import { defineEventHandler } from 'h3';
import { createSuccessResponse, forgetMockNetwork, getGizmoMockState, handleEspMockError } from '../../utils/gizmo-mock.js';

export default defineEventHandler(async event => {
    try {
        // Forget the saved network, then restart into the setup hotspot (wifi_forget_failed when the credentials can't be cleared)
        forgetMockNetwork(getGizmoMockState());
        return createSuccessResponse();
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'wifi_forget_failed', errorMessage: 'Failed to forget the network' });
    }
});