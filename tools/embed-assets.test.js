// Tests of the gizmo-embed tool (run with: npm test)
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { gunzipSync } from 'node:zlib';
import assert from 'node:assert/strict';
import { afterEach, beforeEach, describe, it } from 'node:test';

import { buildAssets, buildHeader, buildSource, getContentType, getRoutePath } from './embed-assets.js';

// Routes: index pages are served at their folder, everything else at its path
describe('getRoutePath', () => {
	// The home page is the root
	it('serves the root index at /', () => {
		assert.equal(getRoutePath('index.html'), '/');
	});

	// A page folder is served without index.html
	it('serves a folder index at the folder', () => {
		assert.equal(getRoutePath(path.join('settings', 'index.html')), '/settings');
	});

	// Other files keep their path
	it('serves other files at their own path', () => {
		assert.equal(getRoutePath(path.join('_nuxt', 'entry.js')), '/_nuxt/entry.js');
	});
});

// Content types come from the extension, with a binary fallback
describe('getContentType', () => {
	// Known extensions, in any case
	it('knows the web file types', () => {
		assert.equal(getContentType('app.JS'), 'application/javascript; charset=utf-8');
		assert.equal(getContentType('font.woff2'), 'font/woff2');
	});

	// Unknown files are sent as binary
	it('falls back to binary', () => {
		assert.equal(getContentType('data.bin'), 'application/octet-stream');
	});
});

// The whole site: gzipped files, sorted, and C++ that lists them
describe('buildAssets', () => {
	let siteDirectory;

	// Create a small site in a temporary folder
	beforeEach(() => {
		siteDirectory = mkdtempSync(path.join(tmpdir(), 'gizmo-embed-'));
		mkdirSync(path.join(siteDirectory, 'settings'));
		writeFileSync(path.join(siteDirectory, 'index.html'), '<h1>Home</h1>');
		writeFileSync(path.join(siteDirectory, 'settings', 'index.html'), '<h1>"Settings"</h1>');
	});

	// Delete the temporary site
	afterEach(() => {
		rmSync(siteDirectory, { recursive: true, force: true });
	});

	// Every file is gzipped and can be read back
	it('gzips every file', () => {
		const assets = buildAssets(siteDirectory);
		assert.deepEqual(assets.map(asset => asset.routePath), ['/', '/settings']);
		assert.equal(gunzipSync(assets[0].contents).toString(), '<h1>Home</h1>');
	});

	// The source defines one array per file and lists them with their routes
	it('generates C++ for every file', () => {
		const source = buildSource(buildAssets(siteDirectory));
		assert.match(source, /static const uint8_t embeddedAsset0\[\] PROGMEM = \{\n\t0x1f, 0x8b,/);
		assert.match(source, /\{ "\/settings", "text\/html; charset=utf-8", embeddedAsset1, sizeof\(embeddedAsset1\) \},/);
		assert.match(buildHeader(), /#include <gizmo\/embedded_assets.h>/);
	});

	// A missing build stops with a clear error
	it('rejects a missing site folder', () => {
		assert.throws(() => buildAssets(path.join(siteDirectory, 'missing')), /Static site not found/);
	});
});