import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { registerEndpoint } from '@nuxt/test-utils/runtime';
import { fetchEsp } from '~/composables/useGizmoApi.js';
import { handleBackendErrors, splitUptime } from '~/composables/useGizmoUtils.js';

// Record the error dialogs instead of rendering them
const dialogs = vi.hoisted(() => []);
vi.mock('@brumaombra/ui-vintage/message-dialog', () => ({
    showMessageDialog: options => {
        dialogs.push(options);
    }
}));

// Errors of the device: one dialog, with the translated message when the error ID is known
describe('handleBackendErrors', () => {
    // Start without dialogs, and keep the logged errors out of the test output
    beforeEach(() => {
        dialogs.length = 0;
        vi.spyOn(console, 'error').mockImplementation(() => {});
    });

    // It used to throw the error again after the dialog, which ended every failed action with an unhandled error
    it('shows the error without throwing it again', () => {
        const error = Object.assign(new Error('Bad request'), { statusCode: 400, data: { errorId: null, errorMessage: 'brightness', data: null } });
        expect(() => handleBackendErrors({ error, defaultMessage: 'Could not save', showDialog: true })).not.toThrow();
        expect(dialogs).toHaveLength(1);
    });

    // A known error ID of the firmware is shown in the user's words, not as the raw message ("ssid")
    it('translates a known error ID of the device', () => {
        const error = { data: { errorId: 'invalid_connection_field', errorMessage: 'ssid', data: null } };
        handleBackendErrors({ error, showDialog: true });
        expect(dialogs[0].message).toContain('32 characters');
        expect(dialogs[0].type).toBe('error');
    });

    // The error of the new device routes is translated too
    it('translates the failure to forget the network', () => {
        const error = { data: { errorId: 'wifi_forget_failed', errorMessage: 'Failed to forget the network', data: null } };
        handleBackendErrors({ error, showDialog: true });
        expect(dialogs[0].message).toBe('The device could not forget the Wi-Fi network. Please try again.');
    });

    // An error ID only the app knows used to show the translation key itself ("api.backendErrors.x"), now the message of the device is shown
    it('shows the message of the device for an error ID without a translation', () => {
        const error = { data: { errorId: 'unknown_error_of_the_app', errorMessage: 'Device busy', data: null } };
        handleBackendErrors({ error, showDialog: true });
        expect(dialogs[0].message).toBe('Device busy');
    });

    // Without an error ID, the device message, then the default message, is shown
    it('falls back to the message of the device, then to the default one', () => {
        handleBackendErrors({ error: { data: { errorId: null, errorMessage: 'Device busy' } }, showDialog: true });
        handleBackendErrors({ error: {}, defaultMessage: 'Could not save', showDialog: true });
        expect(dialogs.map(dialog => dialog.message)).toEqual(['Device busy', 'Could not save']);
    });

    // Without showDialog, the error is only logged
    it('shows no dialog unless asked', () => {
        handleBackendErrors({ error: new Error('Offline') });
        expect(dialogs).toHaveLength(0);
        expect(console.error).toHaveBeenCalled();
    });
});

// The request wrapper: the device address comes from the runtime config
describe('fetchEsp', () => {
    // Put the runtime config back between tests
    afterEach(() => {
        useRuntimeConfig().public.espBaseUrl = '';
    });

    // Without a base address the request goes to the same origin (the mock routes of the dev server, or the device itself)
    it('calls the same origin when no device address is set', async () => {
        registerEndpoint('/api/device', () => ({ errorId: null, errorMessage: null, data: { name: 'Same origin' } }));
        useRuntimeConfig().public.espBaseUrl = '';
        const response = await fetchEsp('api/device');
        expect(response.data.name).toBe('Same origin');
    });

    // A device address is joined to the path without doubling the slash (nothing answers there, so the error tells which address was called)
    it('joins the device address to the path', async () => {
        useRuntimeConfig().public.espBaseUrl = ' http://192.168.1.50/ ';
        await expect(fetchEsp('/api/restart', { method: 'POST' })).rejects.toMatchObject({ request: 'http://192.168.1.50/api/restart' });
    });
});

// Time since the last boot
describe('splitUptime', () => {
    // The card shows days, hours and minutes
    it('splits the seconds in days, hours and minutes', () => {
        expect(splitUptime(93784)).toEqual({ days: 1, hours: 2, minutes: 3 });
        expect(splitUptime(59)).toEqual({ days: 0, hours: 0, minutes: 0 });
    });

    // A missing or wrong value is shown as zero, not as NaN
    it('treats a missing or wrong value as zero', () => {
        expect(splitUptime(undefined)).toEqual({ days: 0, hours: 0, minutes: 0 });
        expect(splitUptime(-5)).toEqual({ days: 0, hours: 0, minutes: 0 });
    });
});