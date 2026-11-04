import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import Cancel01Icon from '@hugeicons/core-free-icons/Cancel01Icon';
import { showMessageDialog } from '@brumaombra/ui-vintage/message-dialog';

// Translate a key through the active i18n instance when available
export const translate = (key, params = {}) => {
    const nuxtApp = useNuxtApp();

    // If the function is available, return the translated string
    if (typeof nuxtApp?.$i18n?.t === 'function') {
        return nuxtApp.$i18n.t(key, params);
    }

    // Otherwise, return the key as a fallback
    return key;
};

// Check if a translation exists in the active i18n instance
const hasTranslation = key => {
    const nuxtApp = useNuxtApp();
    return typeof nuxtApp?.$i18n?.te === 'function' && nuxtApp.$i18n.te(key);
};

// Pause execution for a short delay
export const delay = timeout => {
    return new Promise(resolve => setTimeout(resolve, timeout));
};

// Split a number of seconds in days, hours and minutes
export const splitUptime = seconds => {
    const totalMinutes = Math.floor(Math.max(0, Number(seconds) || 0) / 60);
    return {
        days: Math.floor(totalMinutes / 1440),
        hours: Math.floor((totalMinutes % 1440) / 60),
        minutes: totalMinutes % 60
    };
};

// Translate the errorId of a device response: the layer's errors first, then the ones of the app (api.backendErrors.<errorId>)
const translateErrorId = errorId => {
    if (!errorId) return '';

    // Look for the error in the layer, then in the app
    const translationKey = [`gizmo.api.backendErrors.${errorId}`, `api.backendErrors.${errorId}`].find(hasTranslation);
    return translationKey ? translate(translationKey) : '';
};

// Handle backend errors with a shared message strategy
export const handleBackendErrors = ({ error, defaultMessage = '', showDialog = false }) => {
    const backendErrorPayload = error?.response?._data || error?.data || error?.response?.data || null;
    const backendErrorMessage = backendErrorPayload?.errorMessage || '';
    const backendErrorTranslated = translateErrorId(backendErrorPayload?.errorId);
    const runtimeErrorMessage = error instanceof Error ? error.message : '';
    const resolvedMessage = backendErrorTranslated || backendErrorMessage || runtimeErrorMessage || defaultMessage || translate('gizmo.common.genericError');
    const resolvedLogMessage = backendErrorMessage || runtimeErrorMessage || defaultMessage || translate('gizmo.common.genericError');

    // Show a user-friendly error dialog if requested
    if (showDialog) {
        showMessageDialog({
            type: 'error',
            icon: Alert02Icon,
            title: translate('gizmo.common.errorTitle'),
            message: resolvedMessage,
            closeText: translate('gizmo.common.close'),
            closeButtonIcon: Cancel01Icon
        });
    }

    // Log the error (this is the last stop: the callers only show it, so throwing it again left an unhandled error after the dialog)
    console.error(resolvedLogMessage, error);
};