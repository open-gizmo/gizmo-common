import { computed, toRaw } from 'vue';
import WifiFullSignalIcon from '@hugeicons/core-free-icons/WifiFullSignalIcon';

// Pages the layer provides, that an app adds to its navigation by name (nav: ['wifi'] in the app.config.ts)
const NAV_PRESETS = {
    wifi: { id: 'wifi', labelKey: 'gizmo.nav.wifi.label', descriptionKey: 'gizmo.nav.wifi.description', icon: WifiFullSignalIcon, to: '/wifi' }
};

// Pages of the dashboard from the app.config.ts of the app (the sidebar on larger screens and the bottom navigation on phones show the same items)
export const useGizmoNavItems = () => {
    const { t } = useI18n();
    const route = useRoute();
    const localePath = useLocalePath();
    const appConfig = useAppConfig();

    // Items, top to bottom in the sidebar and left to right in the bottom navigation, with the current one marked (plain paths: the shell adds the language prefix)
    return computed(() => (appConfig.gizmo.nav || [])
        .map(item => typeof item === 'string' ? NAV_PRESETS[item] : item)
        .filter(Boolean)
        .map(item => ({
            id: item.id,
            label: t(item.labelKey),
            description: item.descriptionKey ? t(item.descriptionKey) : '',
            icon: toRaw(item.icon),
            to: item.to,
            active: route.path === localePath(item.to)
        })));
};