import { afterEach, describe, expect, it, vi } from 'vitest';
import { h } from 'vue';
import { mountSuspended, registerEndpoint } from '@nuxt/test-utils/runtime';
import GizmoPage from '~/components/gizmo/GizmoPage.vue';
import WifiPage from '~/pages/wifi.vue';
import { resetGizmoStore } from '~/composables/useGizmoStore.js';

// Close the dialogs between tests
afterEach(() => {
    document.body.innerHTML = '';
});

// The column and the title of a page
describe('GizmoPage', () => {
    // Every page is one centered column with its title as the h1
    it('shows the title in a centered column with the content under it', async () => {
        const wrapper = await mountSuspended(GizmoPage, { props: { title: 'Settings' }, slots: { default: () => h('p', 'Content') } });
        expect([...wrapper.find('h1').element.parentElement.classList]).toEqual(expect.arrayContaining(['mx-auto', 'max-w-2xl']));
        expect(wrapper.find('h1').text()).toBe('Settings');
        expect(wrapper.find('h1').classes()).not.toContain('sr-only');
        expect(wrapper.text()).toContain('Content');
    });

    // A page with a main card at the top hides the title on screen, but screen readers still need it
    it('can hide the title on screen only', async () => {
        const wrapper = await mountSuspended(GizmoPage, { props: { title: 'Home', hideTitle: true } });
        expect(wrapper.find('h1').classes()).toContain('sr-only');
        expect(wrapper.find('h1').text()).toBe('Home');
    });
});

// The Wi-Fi page of the layer
describe('Wi-Fi page', () => {
    // The page is a column with its title, and groups with titles (no icon headings, no loose cards)
    it('shows the title and the groups of networks', async () => {
        resetGizmoStore();
        registerEndpoint('/api/device', () => ({ errorId: null, errorMessage: null, data: { name: 'Gizmo', ssid: 'Home' } }));
        registerEndpoint('/api/networks', () => ({ errorId: null, errorMessage: null, data: { networks: [{ ssid: 'Office Fiber', quality: 74, secured: true }] } }));
        const wrapper = await mountSuspended(WifiPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Office Fiber'));
        expect(wrapper.find('h1').text()).toBe('Connect to WiFi');
        expect(wrapper.findAll('h2').map(title => title.text())).toEqual(['Current network', 'Available networks']);
        await vi.waitFor(() => expect(wrapper.find('ul').text()).toContain('Home'));
    });
});
