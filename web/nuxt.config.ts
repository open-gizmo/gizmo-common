// Nuxt layer shared by the OpenGizmo web apps: the Wi-Fi pages, the device card, the dashboard layout and the mock device routes
export default defineNuxtConfig({
    // Same source folder as the apps
    srcDir: 'app/',

    // The apps are served by the device, so there is no server rendering
    ssr: false,

    // Address of the device while developing (empty means the same origin, served by the mock routes of Nitro)
    runtimeConfig: {
        public: {
            espBaseUrl: ''
        }
    },

    // Modules the layer's components need (the app has them too, as peer dependencies)
    modules: [
        '@brumaombra/ui-vintage',
        '@nuxtjs/i18n'
    ],

    // Languages of the layer: its strings live under the "gizmo" key, and are merged with the ones of the app
    i18n: {
        defaultLocale: 'en',
        langDir: 'locales',
        detectBrowserLanguage: false,
        locales: [
            { code: 'en', name: 'English', language: 'en-US', iso: 'en-US', file: 'en.json' },
            { code: 'it', name: 'Italiano', language: 'it-IT', iso: 'it-IT', file: 'it.json' }
        ]
    },

    vite: {
        // One copy of these packages, the one of the app (a layer linked from a folder, like in a local development setup, has its own node_modules)
        resolve: {
            dedupe: [
                '@brumaombra/ui-vintage',
                '@hugeicons/vue',
                'reka-ui',
                'vue',
                'vue-i18n',
                'vue-router'
            ]
        },
        optimizeDeps: {
            include: [
                '@brumaombra/ui-vintage',
                '@hugeicons/vue'
            ]
        }
    },

    // The pages of the layer have to be in the static build
    nitro: {
        prerender: {
            routes: [
                '/wifi',
                '/ap-setup'
            ]
        }
    },

    devtools: {
        enabled: false
    },

    compatibilityDate: '2026-06-18'
});
