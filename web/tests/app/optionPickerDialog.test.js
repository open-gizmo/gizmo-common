import { afterEach, describe, expect, it } from 'vitest';
import { mountSuspended } from '@nuxt/test-utils/runtime';
import OptionPickerDialog from '~/components/OptionPickerDialog.vue';

// Options of the picker
const options = [
    { value: 'en', label: 'English', description: 'The default language' },
    { value: 'it', label: 'Italiano' }
];

// Mount the picker open, on English
const mountPicker = () => mountSuspended(OptionPickerDialog, { props: { visible: true, title: 'Language', options, modelValue: 'en' } });

// Find an option button in the dialog (it renders in a portal, outside the mounted component)
const findOption = label => [...document.body.querySelectorAll('button')].find(button => button.textContent.includes(label));

// Close the dialog between tests
afterEach(() => {
    document.body.innerHTML = '';
});

// Picker used for the language of the apps
describe('OptionPickerDialog', () => {
    // The options are listed with their description, and the current one is marked
    it('lists the options and marks the current one', async () => {
        await mountPicker();
        expect(document.body.textContent).toContain('Language');
        expect(document.body.textContent).toContain('The default language');
        expect(findOption('English').getAttribute('aria-pressed')).toBe('true');
        expect(findOption('Italiano').getAttribute('aria-pressed')).toBe('false');
    });

    // Picking another option sends it, and closes the dialog
    it('emits the new value and closes', async () => {
        const wrapper = await mountPicker();
        findOption('Italiano').click();
        expect(wrapper.emitted('update:modelValue')).toEqual([['it']]);
        expect(wrapper.emitted('close')).toHaveLength(1);
    });

    // A tap on the current option only closes (no change to save)
    it('only closes when the current option is picked', async () => {
        const wrapper = await mountPicker();
        findOption('English').click();
        expect(wrapper.emitted('update:modelValue')).toBeUndefined();
        expect(wrapper.emitted('close')).toHaveLength(1);
    });
});