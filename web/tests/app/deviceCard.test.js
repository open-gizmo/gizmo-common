import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { defineComponent } from 'vue';
import { setResponseStatus } from 'h3';
import { mountSuspended, registerEndpoint } from '@nuxt/test-utils/runtime';
import GizmoDeviceCard from '~/components/gizmo/GizmoDeviceCard.vue';
import { resetGizmoStore, useGizmoStore } from '~/composables/useGizmoStore.js';

// Record the result dialogs instead of showing them
const messages = vi.hoisted(() => []);
vi.mock('@brumaombra/ui-vintage/message-dialog', () => ({
    showMessageDialog: options => {
        messages.push(options);
    }
}));

// Details of a device on a network, as GET /api/device answers them
const buildDevice = () => ({
    name: 'Gizmo Ticker',
    version: 'v2026.11.04.1',
    uptimeSeconds: 93784,
    freeHeap: 187432,
    ssid: 'Home',
    accessPointActive: false,
    ip: '192.168.1.42',
    hostname: 'gizmo-ticker.local',
    signalQuality: 74
});

// The device: its details, whether it answers, and the actions it received
let device;
let isDeviceAvailable;
let restartCount;
let forgetCount;
let forgetError;

// Answer like the device
const registerDeviceEndpoints = () => {
    registerEndpoint('/api/device', () => {
        if (!isDeviceAvailable) throw createError({ statusCode: 500, data: { errorId: null, errorMessage: 'Unreachable', data: null } });
        return { errorId: null, errorMessage: null, data: device };
    });
    registerEndpoint('/api/restart', {
        method: 'POST',
        handler: () => {
            restartCount += 1;
            return { errorId: null, errorMessage: null, data: null };
        }
    });
    registerEndpoint('/api/forget-wifi', {
        method: 'POST',
        handler: event => {
            forgetCount += 1;

            // Answer the error like the firmware: the status code, and the body with the error ID
            if (forgetError) {
                setResponseStatus(event, 500);
                return { errorId: 'wifi_forget_failed', errorMessage: 'Failed to forget the network', data: null };
            }

            return { errorId: null, errorMessage: null, data: null };
        }
    });
};

// Cards mounted by the test, unmounted afterwards (a card waiting for a restarted device keeps a timer)
const mountedWrappers = [];

// Mount the card like a settings page does, and wait for the details
const mountCard = async () => {
    const wrapper = await mountSuspended(GizmoDeviceCard);
    mountedWrappers.push(wrapper);
    await vi.waitFor(() => expect(wrapper.text()).toContain('Gizmo Ticker'));
    return wrapper;
};

// Find a button by its text, in a wrapper or in the dialogs (they render in a portal, outside the card)
const findButton = (root, text) => [...root.querySelectorAll('button')].find(button => button.textContent.includes(text));
const findRowButton = (wrapper, text) => wrapper.findAll('button').find(button => button.text().includes(text));

// Wait for the confirmation dialog, and press its main button
const confirmInDialog = async title => {
    await vi.waitFor(() => expect(document.body.textContent).toContain(title));
    const dialog = document.body.querySelector('[role="dialog"]');
    dialog.querySelector('button:not([aria-label="Close"])').click();
};

// Start each test with a device on a network that answers, and a store that has not read it yet
beforeEach(() => {
    device = buildDevice();
    isDeviceAvailable = true;
    restartCount = 0;
    forgetCount = 0;
    forgetError = false;
    messages.length = 0;
    resetGizmoStore();
    vi.spyOn(console, 'error').mockImplementation(() => {});
    registerDeviceEndpoints();
});

// Unmount the cards and close the dialogs between tests
afterEach(() => {
    while (mountedWrappers.length) mountedWrappers.pop().unmount();
    document.body.innerHTML = '';
});

// Device card: what the firmware tells about itself
describe('GizmoDeviceCard', () => {
    // The card shows every field of GET /api/device
    it('shows the details of the device', async () => {
        const wrapper = await mountCard();
        expect(wrapper.text()).toContain('Gizmo Ticker');
        expect(wrapper.text()).toContain('v2026.11.04.1');
        expect(wrapper.text()).toContain('1d 2h 3m');
        expect(wrapper.text()).toContain('183 KB');
        expect(wrapper.text()).toContain('Home');
        expect(wrapper.text()).toContain('192.168.1.42');
        expect(wrapper.text()).toContain('gizmo-ticker.local');
        expect(wrapper.text()).toContain('74%');
    });

    // The groups of the card, top to bottom (the first is Info, so it doesn't repeat the title of the Device page that holds the card)
    it('shows the groups Info, Network and Actions', async () => {
        const wrapper = await mountCard();
        await vi.waitFor(() => expect(wrapper.text()).toContain('v2026.11.04.1'));
        expect(wrapper.findAll('section h2').map(title => title.text())).toEqual(['Info', 'Network', 'Actions']);
    });

    // Long values (the host name of the firmware, the name of a network) used to be cut with "..." by the row
    it('shows long values whole, wrapping them in their row', async () => {
        device = { ...buildDevice(), name: 'Gizmo Reolink Switch', ssid: 'A Very Long Network Name 5GHz', hostname: 'gizmo-reolink-switch.local' };
        const wrapper = await mountSuspended(GizmoDeviceCard);
        mountedWrappers.push(wrapper);
        await vi.waitFor(() => expect(wrapper.text()).toContain('gizmo-reolink-switch.local'));

        // Expect each long value in full, in a span that is not truncated
        for (const value of ['Gizmo Reolink Switch', 'A Very Long Network Name 5GHz', 'gizmo-reolink-switch.local']) {
            const span = wrapper.findAll('span').find(candidate => candidate.text() === value);
            expect(span, value).toBeDefined();
            expect(span.classes()).not.toContain('truncate');
        }
    });

    // The actions are one group with a title, with the destructive rows last, and an app can add its own destructive rows
    it('puts the actions in one titled group, with the rows of the app last', async () => {
        const wrapper = await mountSuspended(defineComponent({
            components: { GizmoDeviceCard },
            template: '<GizmoDeviceCard><template #danger-rows><li data-test="app-row"><button type="button">Reset saved settings</button></li></template></GizmoDeviceCard>'
        }));
        mountedWrappers.push(wrapper);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Gizmo Ticker'));

        // Expect the group titled Actions to hold restart, forget network and the row of the app, in this order
        const group = wrapper.findAll('section').find(section => section.find('h2').exists() && section.find('h2').text() === 'Actions');
        expect(group).toBeDefined();
        expect(group.findAll('li').map(row => row.text())).toEqual(['Restart', 'Forget network', 'Reset saved settings']);
    });

    // The actions of the app don't depend on what the device says about itself, so they stay when it can't be read
    it('keeps the rows of the app when the device does not answer', async () => {
        isDeviceAvailable = false;
        const wrapper = await mountSuspended(defineComponent({
            components: { GizmoDeviceCard },
            template: '<GizmoDeviceCard><template #danger-rows><li><button type="button">Reset saved settings</button></li></template></GizmoDeviceCard>'
        }));
        mountedWrappers.push(wrapper);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Couldn\'t read the device'));
        expect(wrapper.text()).toContain('Reset saved settings');
        expect(wrapper.text()).not.toContain('Forget network');
    });

    // The saved network opens the Wi-Fi page
    it('links the saved network to the Wi-Fi page', async () => {
        const wrapper = await mountCard();
        expect(wrapper.find('a[href="/wifi"]').text()).toContain('Home');
    });

    // The firmware sends the address and the signal only once connected, so the card must not show empty rows
    it('hides the address and the signal while the device is not connected', async () => {
        device = { name: 'Gizmo Ticker', version: 'dev', uptimeSeconds: 12, freeHeap: 150000, ssid: '', accessPointActive: true };
        const wrapper = await mountCard();
        expect(wrapper.text()).toContain('Not connected');
        expect(wrapper.text()).toContain('The setup hotspot is open');
        expect(wrapper.text()).not.toContain('IP address');
        expect(wrapper.text()).not.toContain('Signal quality');
        expect(wrapper.text()).not.toContain('undefined');
    });

    // Without a saved network there is nothing to forget
    it('offers to forget the network only when one is saved', async () => {
        device = { ...buildDevice(), ssid: '' };
        const wrapper = await mountCard();
        expect(findRowButton(wrapper, 'Restart')).toBeDefined();
        expect(findRowButton(wrapper, 'Forget network')).toBeUndefined();
    });

    // A device that doesn't answer gets an error state with a way to try again, not a card of empty values
    it('offers to try again when the device does not answer', async () => {
        isDeviceAvailable = false;
        const wrapper = await mountSuspended(GizmoDeviceCard);
        mountedWrappers.push(wrapper);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Couldn\'t read the device'));

        // Try again once the device is back
        isDeviceAvailable = true;
        await findRowButton(wrapper, 'Try again').trigger('click');
        await vi.waitFor(() => expect(wrapper.text()).toContain('Gizmo Ticker'));
    });

    // Restart: a confirmation first, then the request
    it('restarts the device only after the confirmation', async () => {
        const wrapper = await mountCard();

        // Open the confirmation: nothing is sent yet
        await findRowButton(wrapper, 'Restart').trigger('click');
        await vi.waitFor(() => expect(document.body.textContent).toContain('Restart the device?'));
        expect(restartCount).toBe(0);

        // Confirm: the request goes out, and the user is told the device is restarting
        await confirmInDialog('Restart the device?');
        await vi.waitFor(() => expect(restartCount).toBe(1));
        await vi.waitFor(() => expect(messages).toHaveLength(1));
        expect(messages[0].type).toBe('success');
        expect(messages[0].message).toContain('restarting');
        expect(forgetCount).toBe(0);
    });

    // Cancelling the confirmation sends nothing
    it('sends nothing when the confirmation is cancelled', async () => {
        const wrapper = await mountCard();
        await findRowButton(wrapper, 'Restart').trigger('click');
        await vi.waitFor(() => expect(document.body.textContent).toContain('Restart the device?'));
        findButton(document.body, 'Cancel').click();
        await vi.waitFor(() => expect(document.body.textContent).not.toContain('Restart the device?'));
        expect(restartCount).toBe(0);
        expect(messages).toHaveLength(0);
    });

    // While the device restarts the actions are gone, so a second tap can't restart it twice
    it('hides the actions while the device restarts', async () => {
        const wrapper = await mountCard();
        await findRowButton(wrapper, 'Restart').trigger('click');
        await confirmInDialog('Restart the device?');
        await vi.waitFor(() => expect(wrapper.text()).toContain('The device is restarting'));
        expect(findRowButton(wrapper, 'Restart')).toBeUndefined();
    });

    // Forget network: a confirmation that says the setup hotspot comes back, then the request
    it('forgets the network only after the confirmation', async () => {
        const wrapper = await mountCard();

        // Open the confirmation: the message names the hotspot, and nothing is sent yet
        await findRowButton(wrapper, 'Forget network').trigger('click');
        await vi.waitFor(() => expect(document.body.textContent).toContain('Forget the Wi-Fi network?'));
        expect(document.body.textContent).toContain('setup hotspot');
        expect(forgetCount).toBe(0);

        // Confirm: the request goes out, and the user is told to use the hotspot
        await confirmInDialog('Forget the Wi-Fi network?');
        await vi.waitFor(() => expect(forgetCount).toBe(1));
        await vi.waitFor(() => expect(messages).toHaveLength(1));
        expect(messages[0].message).toContain('hotspot');
        expect(restartCount).toBe(0);
    });

    // The firmware answers wifi_forget_failed when it can't clear the credentials: the user sees the translated error, and the card stays usable
    it('shows the error of a network that could not be forgotten', async () => {
        forgetError = true;
        const wrapper = await mountCard();
        await findRowButton(wrapper, 'Forget network').trigger('click');
        await confirmInDialog('Forget the Wi-Fi network?');
        await vi.waitFor(() => expect(messages).toHaveLength(1));
        expect(messages[0].type).toBe('error');
        expect(messages[0].message).toBe('The device could not forget the Wi-Fi network. Please try again.');
        expect(findRowButton(wrapper, 'Forget network')).toBeDefined();
    });

    // The card keeps its details in the store of the layer, not in the one of an app
    it('keeps the details in the store of the layer', async () => {
        await mountCard();
        expect(useGizmoStore().value.device.name).toBe('Gizmo Ticker');
    });
});