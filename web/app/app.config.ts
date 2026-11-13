// Defaults of the layer: each app overrides them in its own app.config.ts, under the "gizmo" key
export default defineAppConfig({
    gizmo: {
        // Name shown next to the logo (sidebar, setup page, error page)
        name: 'Gizmo',

        // mDNS name of the firmware, without .local (always the project name, for example "gizmo-ticker"; the mock answers with it)
        hostname: '',

        // Logo shown next to the name (a path of the public folder)
        logo: '/logo.svg',

        // Logo for the dark theme (optional: without it, the same logo shows in both themes)
        logoDark: '',

        // Page the logo and the "back" button of the error page go to
        homePath: '/',

        // Pages of the dashboard, in order: an object ({ id, labelKey, descriptionKey, icon, to }), or the name of a page the layer provides ('wifi')
        nav: []
    }
});
