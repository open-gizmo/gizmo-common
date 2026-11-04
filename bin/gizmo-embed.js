#!/usr/bin/env node
// Embed a static web site in the firmware: gizmo-embed <site folder> <firmware folder>
import { mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';

import { buildAssets, buildHeader, buildSource } from '../tools/embed-assets.js';

// Read the folders from the command line
const [siteDirectory, outputDirectory] = process.argv.slice(2);
if (!siteDirectory || !outputDirectory) {
	console.error('Usage: gizmo-embed <site folder> <firmware folder>');
	process.exit(1);
}

try {
	// Gzip the files of the site
	const assets = buildAssets(path.resolve(siteDirectory));

	// Write the header and the source next to the firmware
	const firmwareDirectory = path.resolve(outputDirectory);
	mkdirSync(firmwareDirectory, { recursive: true });
	writeFileSync(path.join(firmwareDirectory, 'generated_assets.h'), buildHeader(), 'utf8');
	writeFileSync(path.join(firmwareDirectory, 'generated_assets.cpp'), buildSource(assets), 'utf8');

	// Print the files and their total size
	const totalBytes = assets.reduce((total, asset) => total + asset.contents.length, 0);
	console.log(`Embedded ${assets.length} files (${(totalBytes / 1024).toFixed(1)} KB gzipped):`);
	for (const asset of assets) {
		console.log(`  ${asset.routePath} <= ${asset.sourcePath}`);
	}
} catch (error) {
	console.error(error.message);
	process.exit(1);
}