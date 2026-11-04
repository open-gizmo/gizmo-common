import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { defineComponent } from 'vue';
import { mountSuspended } from '@nuxt/test-utils/runtime';
import DashboardLayout from '~/layouts/dashboard.vue';
import ErrorPage from '~/error.vue';
import ApSetupPage from '~/pages/ap-setup.vue';
import { useGizmoNavItems } from '~/composables/useGizmoNavItems.js';

// What an app gives the layer in its app.config.ts
const homeItem = { id: 'home', labelKey: 'gizmo.layout.controlPanel', descriptionKey: 'gizmo.nav.wifi.description', icon: [], to: '/' };

// Items of the navigation, read inside a component (the composable needs the Nuxt context)
const readNavItems = async (route = '/') => {
    let items;
    await mountSuspended(defineComponent({
        setup() {
            items = useGizmoNavItems();
            return () => null;
        }
    }), { route });
    return items.value;
};

// The app.config.ts of the layer, as it is before a test changes it
let appConfig;
let defaultGizmoConfig;

// Remember the defaults of the layer, and put them back after each test
beforeEach(() => {
    appConfig = useAppConfig();
    defaultGizmoConfig = JSON.parse(JSON.stringify(appConfig.gizmo));
});
afterEach(() => {
    appConfig.gizmo = { ...defaultGizmoConfig };
});

// Navigation of the dashboard: the app lists its pages in app.config.ts
describe('useGizmoNavItems', () => {
    // An app with no navigation gets an empty list, not an error
    it('is empty until the app lists its pages', async () => {
        expect(await readNavItems()).toEqual([]);
    });

    // The Wi-Fi page is added with one word
    it('adds the Wi-Fi page by name', async () => {
        appConfig.gizmo.nav = [homeItem, 'wifi'];
        const items = await readNavItems();
        expect(items.map(item => item.id)).toEqual(['home', 'wifi']);
        expect(items[1]).toMatchObject({ label: 'WiFi', description: 'Scan networks and connect the device', to: '/wifi' });
    });

    // A name the layer does not provide is skipped instead of breaking the sidebar
    it('skips a page name it does not know', async () => {
        appConfig.gizmo.nav = [homeItem, 'unknown'];
        expect((await readNavItems()).map(item => item.id)).toEqual(['home']);
    });

    // The current page is marked, so the sidebar and the bottom navigation highlight it
    it('marks the current page', async () => {
        appConfig.gizmo.nav = [homeItem, 'wifi'];
        const items = await readNavItems('/wifi');
        expect(items.map(item => item.active)).toEqual([false, true]);
    });
});

// The layout and the pages of the layer must not hard-code the app
describe('app name and logo', () => {
    // The sidebar shows the name and the pages the app gave
    it('shows the name and the pages of the app in the dashboard layout', async () => {
        appConfig.gizmo.name = 'Test Strip';
        appConfig.gizmo.logo = '/test-logo.svg';
        appConfig.gizmo.nav = [homeItem, 'wifi'];
        const wrapper = await mountSuspended(DashboardLayout);
        expect(wrapper.text()).toContain('Test Strip');
        expect(wrapper.text()).toContain('WiFi');
        expect(wrapper.html()).toContain('/test-logo.svg');
    });

    // The setup page of the hotspot has no dashboard, but the same brand
    it('shows the name and the logo of the app on the setup page', async () => {
        appConfig.gizmo.name = 'Test Strip';
        appConfig.gizmo.logo = '/test-logo.svg';
        const wrapper = await mountSuspended(ApSetupPage);
        expect(wrapper.text()).toContain('Test Strip');
        expect(wrapper.find('img[src="/test-logo.svg"]').exists()).toBe(true);
    });

    // The error page shows the brand too
    it('shows the name of the app on the error page', async () => {
        appConfig.gizmo.name = 'Test Strip';
        const wrapper = await mountSuspended(ErrorPage, { props: { error: { statusCode: 404 } } });
        expect(wrapper.text()).toContain('Test Strip');
        expect(wrapper.text()).toContain('Page not found');
    });
});