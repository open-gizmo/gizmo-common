<script setup>
import { computed } from 'vue';
import { ErrorPage } from '@brumaombra/ui-vintage/error-page';
import { LanguageSelector } from '@brumaombra/ui-vintage/language-selector';
import { ThemeSelector } from '@brumaombra/ui-vintage/theme-selector';
import GizmoAppLogo from './components/gizmo/GizmoAppLogo.vue';

// Props
const props = defineProps({
    error: { type: Object, default: () => ({}) }
});

const { locale, locales, setLocale, t } = useI18n();
const localePath = useLocalePath();
const appConfig = useAppConfig();

// Displayed status code
const statusCode = computed(() => props.error?.statusCode ?? 500);

// Page title
const errorTitle = computed(() => statusCode.value === 404 ? t('gizmo.errors.pageNotFound') : t('gizmo.errors.genericTitle'));

// Supporting message
const errorMessage = computed(() => statusCode.value === 404 ? t('gizmo.errors.pageNotFoundMessage') : (props.error?.statusMessage || t('gizmo.errors.genericMessage')));

// Available language codes for the selector
const languageOptions = computed(() => locales.value.map(language => language.code));

// Apply a selected language
const handleSelectLanguage = async language => {
    await setLocale(language);
};

// Clear the error and return to the home page of the app
const handleAction = async () => {
    await clearError({ redirect: localePath(appConfig.gizmo.homePath) });
};
</script>

<template>
    <ErrorPage :status-code="statusCode" :label="t('gizmo.errors.label')" :title="errorTitle" :message="errorMessage" :action-label="t('gizmo.errors.backToHome')" @action="handleAction">
        <!-- Language and theme -->
        <template #toolbar>
            <LanguageSelector :model-value="locale" :languages="languageOptions" @select="handleSelectLanguage" />
            <ThemeSelector />
        </template>

        <!-- Brand -->
        <template #brand>
            <GizmoAppLogo />
        </template>
    </ErrorPage>
</template>