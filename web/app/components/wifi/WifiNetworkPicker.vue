<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from 'vue';
import Cancel01Icon from '@hugeicons/core-free-icons/Cancel01Icon';
import CheckmarkCircle02Icon from '@hugeicons/core-free-icons/CheckmarkCircle02Icon';
import LockIcon from '@hugeicons/core-free-icons/LockIcon';
import RefreshIcon from '@hugeicons/core-free-icons/RefreshIcon';
import WifiFullSignalIcon from '@hugeicons/core-free-icons/WifiFullSignalIcon';
import WifiOff01Icon from '@hugeicons/core-free-icons/WifiOff01Icon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { setBusy } from '@brumaombra/ui-vintage/busy-indicator';
import { Button } from '@brumaombra/ui-vintage/button';
import { showConfirmDialog } from '@brumaombra/ui-vintage/confirm-dialog';
import { Field, FieldContent, FieldLabel } from '@brumaombra/ui-vintage/field';
import { Input } from '@brumaombra/ui-vintage/input';
import { ListGroup, ListRow } from '@brumaombra/ui-vintage/list';
import { showMessageDialog } from '@brumaombra/ui-vintage/message-dialog';
import { ResponsiveDialog } from '@brumaombra/ui-vintage/responsive-dialog';
import { connectToWiFi, getNetworks } from '../../composables/useGizmoApi.js';
import { useGizmoStore } from '../../composables/useGizmoStore.js';
import { handleBackendErrors } from '../../composables/useGizmoUtils.js';

// Props: the copy of the confirmation (the setup hotspot page says the hotspot closes)
const props = defineProps({
    confirmTitle: { type: String, required: true },
    confirmMessageKey: { type: String, required: true }
});

const AUTO_RETRY_DELAY = 3000; // Time the device needs to finish its first scan
const { t } = useI18n();
const gizmoStore = useGizmoStore();
const selectedNetwork = ref(null);
const password = ref('');
const isPasswordDialogVisible = ref(false);
const isLoading = ref(false);

// Networks found by the device, strongest first
const networks = computed(() => {
    const scannedNetworks = Array.isArray(gizmoStore.value.networksList) ? gizmoStore.value.networksList : [];
    return [...scannedNetworks].sort((first, second) => second.quality - first.quality);
});

// Refresh the list of networks found by the device
const scanNetworks = async () => {
    isLoading.value = true;

    try {
        const networksData = await getNetworks();
        gizmoStore.value.networksList = networksData.networks;
    } finally {
        isLoading.value = false;
    }
};

// Scan again when the user asks for it
const handleRefreshPress = async () => {
    try {
        await scanNetworks();
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('gizmo.wifi.refreshError'), showDialog: true });
    }
};

// Scan without an error dialog (the user didn't ask for it, and can still scan again)
const scanNetworksQuietly = async () => {
    try {
        await scanNetworks();
    } catch (error) {
        console.warn('Automatic Wi-Fi scan failed', error);
    }
};

// Scan when the page opens, then once more by itself if the list is still empty (the first scan after boot comes back empty while the device is still scanning)
let autoRetryTimer = null;
let isUnmounted = false;
onMounted(async () => {
    // Scan right away if no network is known yet
    if (!networks.value.length) await scanNetworksQuietly();

    // Try once more after a few seconds, unless the page was left meanwhile
    if (networks.value.length || isUnmounted) return;
    autoRetryTimer = setTimeout(scanNetworksQuietly, AUTO_RETRY_DELAY);
});

// Stop the retry when leaving the page
onBeforeUnmount(() => {
    isUnmounted = true;
    clearTimeout(autoRetryTimer);
});

// Send the credentials to the device and wait for the result
const connectDeviceToWiFi = async () => {
    const ssid = selectedNetwork.value.ssid;

    try {
        setBusy(true);
        const result = await connectToWiFi(ssid, password.value);
        gizmoStore.value.device = { ...gizmoStore.value.device, ssid: result.ssid || ssid };
        showMessageDialog({
            type: 'success',
            icon: CheckmarkCircle02Icon,
            title: t('gizmo.wifi.connectedTitle'),
            message: t('gizmo.wifi.connectedMessage', { ssid: result.ssid || ssid, hostname: result.hostname, ip: result.ip }),
            closeText: t('gizmo.common.close'),
            closeButtonIcon: Cancel01Icon
        });
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('gizmo.wifi.connectError'), showDialog: true });
    } finally {
        setBusy(false);
        password.value = '';
    }
};

// Ask for confirmation before switching the device to the selected network
const confirmConnection = () => {
    showConfirmDialog({
        icon: WifiFullSignalIcon,
        title: props.confirmTitle,
        message: t(props.confirmMessageKey, { ssid: selectedNetwork.value.ssid }),
        confirmText: t('gizmo.wifi.connectAction'),
        cancelText: t('gizmo.common.cancel'),
        confirmButtonType: 'primary',
        cancelButtonType: 'secondary',
        confirmButtonIcon: WifiFullSignalIcon,
        cancelButtonIcon: Cancel01Icon,
        onConfirm: connectDeviceToWiFi
    });
};

// Pick a network: secured networks ask for the password first, open ones go straight to the confirmation
const handleNetworkPress = network => {
    selectedNetwork.value = network;
    password.value = '';

    // Ask for the password of a secured network
    if (network.secured) {
        isPasswordDialogVisible.value = true;
        return;
    }

    // Open networks have no password
    confirmConnection();
};

// Continue with the password typed in the dialog
const handlePasswordSubmit = () => {
    if (!password.value) return;
    isPasswordDialogVisible.value = false;
    confirmConnection();
};
</script>

<template>
    <div class="space-y-6">
        <!-- Networks found by the device (until the first scan finds one, a single row says so) -->
        <ListGroup :title="t('gizmo.wifi.availableNetworks')" :hint="networks.length ? t('gizmo.wifi.note') : t('gizmo.wifi.noNetworksDescription')">
            <ListRow v-for="network in networks"
                :key="network.ssid"
                :label="network.ssid"
                :icon="network.secured ? LockIcon : WifiFullSignalIcon"
                :value="`${network.quality}%`"
                action
                @click="handleNetworkPress(network)" />
            <ListRow v-if="!networks.length" :label="t('gizmo.wifi.noNetworksTitle')" :icon="WifiOff01Icon" />
        </ListGroup>

        <!-- Scan again -->
        <Button variant="secondary" class="w-full" :loading="isLoading" @click="handleRefreshPress">
            <HugeiconsIcon :icon="RefreshIcon" class="size-4" />
            {{ isLoading ? t('gizmo.common.refreshing') : t('gizmo.wifi.scanAgain') }}
        </Button>

        <!-- Password of a secured network (a sheet on phones, a dialog on larger screens) -->
        <ResponsiveDialog v-model:open="isPasswordDialogVisible" :title="t('gizmo.wifi.passwordDialogTitle', { ssid: selectedNetwork?.ssid })" :description="t('gizmo.wifi.passwordDialogDescription')">
            <!-- Password -->
            <form id="wifi-password-form" @submit.prevent="handlePasswordSubmit">
                <Field>
                    <FieldLabel for="wifi-password">{{ t('gizmo.wifi.password') }}</FieldLabel>
                    <FieldContent>
                        <Input id="wifi-password" v-model="password" type="password" autocomplete="current-password" :placeholder="t('gizmo.wifi.passwordPlaceholder')" />
                    </FieldContent>
                </Field>
            </form>

            <!-- Actions (the main one first: on top in the sheet, rightmost in the dialog) -->
            <template #footer>
                <Button type="submit" form="wifi-password-form" variant="primary" :disabled="!password">
                    <HugeiconsIcon :icon="WifiFullSignalIcon" class="size-4" />
                    {{ t('gizmo.wifi.connectAction') }}
                </Button>
                <Button type="button" variant="secondary" @click="isPasswordDialogVisible = false">
                    <HugeiconsIcon :icon="Cancel01Icon" class="size-4" />
                    {{ t('gizmo.common.cancel') }}
                </Button>
            </template>
        </ResponsiveDialog>
    </div>
</template>