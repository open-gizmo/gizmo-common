import { afterEach, describe, expect, it, vi } from 'vitest';
import { defineComponent, h } from 'vue';
import { mountSuspended, registerEndpoint } from '@nuxt/test-utils/runtime';
import { ListGroup } from '@brumaombra/ui-vintage/list';
import GizmoInputRow from '~/components/gizmo/GizmoInputRow.vue';
import GizmoPage from '~/components/gizmo/GizmoPage.vue';
import GizmoSliderRow from '~/components/gizmo/GizmoSliderRow.vue';
import GizmoValueRow from '~/components/gizmo/GizmoValueRow.vue';
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

// A slider inside a group
describe('GizmoSliderRow', () => {
    // Mount the row inside a group, like the pages do
    const mountSlider = props => mountSuspended(defineComponent({
        setup: () => () => h(ListGroup, { title: 'Matrix' }, () => h(GizmoSliderRow, props))
    }));

    // The label and the value are on the first line, the slider under them
    it('shows the label and the value above the slider', async () => {
        const wrapper = await mountSlider({ label: 'Brightness', modelValue: 9, min: 0, max: 15, valueText: '60%' });
        const row = wrapper.find('li');
        expect(row.text()).toContain('Brightness');
        expect(row.text()).toContain('60%');
        expect(row.find('[role="slider"]').exists()).toBe(true);
        expect(row.find('[role="slider"]').attributes('aria-label')).toBe('Brightness');
        expect(row.find('[role="slider"]').attributes('aria-valuenow')).toBe('9');
    });

    // Without a text for the value the number is shown
    it('shows the number when there is no value text', async () => {
        const wrapper = await mountSlider({ label: 'Speed', modelValue: 4 });
        expect(wrapper.find('li').text()).toContain('4');
    });

    // Moving the slider updates the value (a key press on the thumb, like a drag, goes through the same event)
    it('emits the new value when the slider moves', async () => {
        const updates = [];
        const wrapper = await mountSlider({ label: 'Brightness', modelValue: 9, min: 0, max: 15, 'onUpdate:modelValue': value => updates.push(value) });
        await wrapper.find('[role="slider"]').trigger('keydown', { key: 'ArrowRight' });
        expect(updates).toEqual([10]);
    });
});

// A text field inside a group
describe('GizmoInputRow', () => {
    // Mount the row inside a group, like the pages do
    const mountInput = props => mountSuspended(defineComponent({
        setup: () => () => h(ListGroup, { title: 'New key' }, () => h(GizmoInputRow, props))
    }));

    // The label is on the first line and points to the field under it
    it('shows the label above a field that the label points to', async () => {
        const wrapper = await mountInput({ id: 'apiKey', label: 'API key', modelValue: 'abc', placeholder: 'Paste it', maxlength: 34 });
        expect(wrapper.find('label').attributes('for')).toBe('apiKey');
        const input = wrapper.find('input#apiKey');
        expect(input.element.value).toBe('abc');
        expect(input.attributes('placeholder')).toBe('Paste it');
        expect(input.attributes('maxlength')).toBe('34');
    });

    // Typing updates the value
    it('emits what the user types', async () => {
        const updates = [];
        const wrapper = await mountInput({ id: 'apiKey', label: 'API key', 'onUpdate:modelValue': value => updates.push(value) });
        await wrapper.find('input').setValue('new-key');
        expect(updates).toEqual(['new-key']);
    });
});

// A row whose value wraps
describe('GizmoValueRow', () => {
    // Mount the row inside a group
    const mountRow = props => mountSuspended(defineComponent({
        setup: () => () => h(ListGroup, { title: 'Network' }, () => h(GizmoValueRow, props))
    }));

    // The ListRow cut the value with "..." at 45% of its width, so a host name was lost
    it('shows a long value whole', async () => {
        const wrapper = await mountRow({ label: 'Hostname', value: 'gizmo-reolink-switch.local' });
        const value = wrapper.findAll('span').find(span => span.text() === 'gizmo-reolink-switch.local');
        expect(value.classes()).toContain('wrap-anywhere');
        expect(value.classes()).not.toContain('truncate');
    });

    // It is still a ListRow: the props go through, so the row can be a button
    it('passes the props and the click to the row', async () => {
        const clicks = [];
        const wrapper = await mountRow({ label: 'Hostname', value: 'x.local', action: true, onClick: () => clicks.push('click') });
        await wrapper.find('button').trigger('click');
        expect(clicks).toEqual(['click']);
    });

    // A row without a value shows no empty span
    it('shows no value when there is none', async () => {
        const wrapper = await mountRow({ label: 'Hostname' });
        expect(wrapper.find('li').findAll('span').filter(span => span.text() === '')).toHaveLength(0);
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
