<script setup>
import { computed } from 'vue';
import { DashboardShell } from '@brumaombra/ui-vintage/dashboard-shell';
import { LanguageSelector } from '@brumaombra/ui-vintage/language-selector';
import { ThemeSelector } from '@brumaombra/ui-vintage/theme-selector';
import { useGizmoNavItems } from '../composables/useGizmoNavItems.js';

const { locale, locales, setLocale, t } = useI18n();
const appConfig = useAppConfig();

// Pages of the dashboard (the shell shows them in the sidebar, and in the bottom navigation on phones)
const navItems = useGizmoNavItems();

// Sidebar sections
const sidebarSections = computed(() => [{
    id: 'control-panel',
    label: t('gizmo.layout.controlPanel'),
    items: navItems.value
}]);

// Bottom navigation items on phones (the same pages, without descriptions)
const bottomNavItems = computed(() => navItems.value.map(({ id, label, icon, to, active }) => ({ id, label, icon, to, active })));

// Available language codes for the selector
const languageOptions = computed(() => {
    return locales.value.map(language => language.code);
});

// Apply a selected language
const handleSelectLanguage = async language => {
    await setLocale(language);
};
</script>

<template>
    <DashboardShell :app-name="appConfig.gizmo.name"
        :app-logo="appConfig.gizmo.logo"
        :app-logo-dark="appConfig.gizmo.logoDark"
        :app-link-to="appConfig.gizmo.homePath"
        :compact="true"
        :sidebar-sections="sidebarSections"
        :bottom-nav-items="bottomNavItems">
        <!-- Sidebar footer: language and theme -->
        <template #sidebar-footer>
            <div class="flex items-center gap-2">
                <LanguageSelector :model-value="locale" :languages="languageOptions" @select="handleSelectLanguage" />
                <ThemeSelector />
            </div>
        </template>

        <!-- Topbar actions: language and theme on phones (the bottom navigation replaces the sidebar there) -->
        <template #topbar-trailing>
            <div class="flex items-center gap-2 md:hidden">
                <LanguageSelector :model-value="locale" :languages="languageOptions" @select="handleSelectLanguage" />
                <ThemeSelector />
            </div>
        </template>

        <!-- Main content -->
        <slot />
    </DashboardShell>
</template>