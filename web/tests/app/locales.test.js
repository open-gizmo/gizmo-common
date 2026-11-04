import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

// Flatten a locale into { "a.b.c": "text" }
const flatten = (object, prefix = '') => Object.entries(object).flatMap(([key, value]) => {
    return value && typeof value === 'object' ? flatten(value, `${prefix}${key}.`) : [[`${prefix}${key}`, value]];
});

// Placeholders used in a translation, e.g. "{ssid}"
const readPlaceholders = text => [...String(text).matchAll(/\{(\w+)\}/g)].map(match => match[1]).sort();

// All the source files under a folder
const readSourceFiles = directory => readdirSync(directory, { withFileTypes: true }).flatMap(entry => {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) return readSourceFiles(path);
    return /\.(vue|js|cpp|h)$/.test(entry.name) ? [path] : [];
});

// Read a locale file as plain JSON (importing it would go through the i18n module, which compiles the messages)
const readLocale = code => JSON.parse(readFileSync(join(process.cwd(), 'i18n', 'locales', `${code}.json`), 'utf8'));

// The locales of the layer, from the files in the locales folder
const LANGUAGE_CODES = readdirSync(join(process.cwd(), 'i18n', 'locales')).map(file => file.replace(/\.json$/, ''));

const englishTexts = Object.fromEntries(flatten(readLocale('en')));
const otherLocales = Object.fromEntries(LANGUAGE_CODES.filter(code => code !== 'en').map(code => [code, readLocale(code)]));

// Translations of the layer: a missing key shows the raw key (e.g. "gizmo.wifi.scanAgain") to the user
describe('locales', () => {
    // The apps keep their own strings, so the layer may only use its own namespace (otherwise it overwrites theirs)
    it('keeps every string under the gizmo key', () => {
        for (const code of LANGUAGE_CODES) {
            expect(Object.keys(readLocale(code))).toEqual(['gizmo']);
        }
    });

    // Every locale has exactly the English keys
    for (const [code, locale] of Object.entries(otherLocales)) {
        it(`${code} has the same keys as en`, () => {
            const texts = Object.fromEntries(flatten(locale));
            expect(Object.keys(englishTexts).filter(key => !(key in texts)), 'missing keys').toEqual([]);
            expect(Object.keys(texts).filter(key => !(key in englishTexts)), 'extra keys').toEqual([]);
        });

        // A placeholder with a different name is shown as "{ssid}" instead of the value
        it(`${code} uses the same placeholders as en`, () => {
            const texts = Object.fromEntries(flatten(locale));
            const mismatches = Object.keys(englishTexts)
                .filter(key => typeof texts[key] === 'string')
                .filter(key => readPlaceholders(texts[key]).join() !== readPlaceholders(englishTexts[key]).join());
            expect(mismatches).toEqual([]);
        });

        // Empty strings render as blank buttons and labels
        it(`${code} has no empty translations`, () => {
            expect(flatten(locale).filter(([, text]) => text === '').map(([key]) => key)).toEqual([]);
        });
    }

    // Italian is written with its accents: an apostrophe in place of a grave accent ("e" for "è") is a typo
    it('it uses accents where the language needs them', () => {
        const texts = flatten(otherLocales.it).map(([, text]) => text);
        expect(texts.filter(text => /\b(non e|piu|puo|gia|e riuscito|e stato)\b/.test(text))).toEqual([]);
    });

    // Keys written literally in the code (t('a.b') or translate('a.b')) must exist, dynamic keys cannot be checked
    it('has every key used in the code', () => {
        const root = process.cwd();
        const missing = new Set();
        for (const file of readSourceFiles(join(root, 'app'))) {
            const source = readFileSync(file, 'utf8');
            for (const match of source.matchAll(/\b(?:t|translate)\(\s*['"]([a-zA-Z0-9_.]+)['"]/g)) {
                if (!(match[1] in englishTexts)) missing.add(`${match[1]} (${file.slice(root.length + 1)})`);
            }
        }
        expect([...missing]).toEqual([]);
    });

    // The device card builds its keys from the action ("gizmo.device.restart.confirmTitle"), so they are checked here
    it('has the texts of both actions of the device card', () => {
        const missing = [];
        for (const action of ['restart', 'forgetWifi']) {
            for (const field of ['action', 'confirmTitle', 'confirmMessage', 'confirmAction', 'successTitle', 'successMessage', 'error']) {
                if (!(`gizmo.device.${action}.${field}` in englishTexts)) missing.push(`gizmo.device.${action}.${field}`);
            }
        }
        expect(missing).toEqual([]);
    });

    // Every error the mock device sends has a translation, except its own mock_ ones that the real device never sends (otherwise the dialog shows the raw message)
    it('translates every error ID of the mock device', () => {
        const errorIds = new Set();
        for (const file of readSourceFiles(join(process.cwd(), 'server'))) {
            const source = readFileSync(file, 'utf8');
            for (const match of source.matchAll(/new EspMockError\(\d+,\s*'([a-z_]+)'/g)) errorIds.add(match[1]);
            for (const match of source.matchAll(/errorId:\s*'([a-z_]+)'/g)) errorIds.add(match[1]);
        }
        expect(errorIds.size).toBeGreaterThan(0);
        expect([...errorIds].filter(errorId => !errorId.startsWith('mock_')).filter(errorId => !(`gizmo.api.backendErrors.${errorId}` in englishTexts))).toEqual([]);
    });

    // Every error the firmware of gizmo-common can send has a translation too (the library is the parent folder of the layer)
    it('translates every error ID of the firmware of gizmo-common', () => {
        const errorIds = new Set();
        for (const file of readSourceFiles(join(process.cwd(), '..', 'src')).filter(file => /\.(cpp|h)$/.test(file))) {
            const source = readFileSync(file, 'utf8');
            for (const match of source.matchAll(/sendErrorResponse\([^,]+,\s*\d+,\s*"([a-z_]+)"/g)) errorIds.add(match[1]);
        }
        expect(errorIds.size).toBeGreaterThan(0);
        expect([...errorIds].filter(errorId => !(`gizmo.api.backendErrors.${errorId}` in englishTexts))).toEqual([]);
    });
});