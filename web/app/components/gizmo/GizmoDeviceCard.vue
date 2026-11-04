<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from 'vue';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import Cancel01Icon from '@hugeicons/core-free-icons/Cancel01Icon';
import CheckmarkCircle02Icon from '@hugeicons/core-free-icons/CheckmarkCircle02Icon';
import Clock01Icon from '@hugeicons/core-free-icons/Clock01Icon';
import CpuIcon from '@hugeicons/core-free-icons/CpuIcon';
import Globe02Icon from '@hugeicons/core-free-icons/Globe02Icon';
import Link01Icon from '@hugeicons/core-free-icons/Link01Icon';
import RamMemoryIcon from '@hugeicons/core-free-icons/RamMemoryIcon';
import ReloadIcon from '@hugeicons/core-free-icons/ReloadIcon';
import RefreshIcon from '@hugeicons/core-free-icons/RefreshIcon';
import SignalFull02Icon from '@hugeicons/core-free-icons/SignalFull02Icon';
import Tag01Icon from '@hugeicons/core-free-icons/Tag01Icon';
import WifiFullSignalIcon from '@hugeicons/core-free-icons/WifiFullSignalIcon';
import WifiOff01Icon from '@hugeicons/core-free-icons/WifiOff01Icon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { setBusy } from '@brumaombra/ui-vintage/busy-indicator';
import { Button } from '@brumaombra/ui-vintage/button';
import { EmptyStateCard } from '@brumaombra/ui-vintage/empty-state-card';
import { ListGroup, ListRow } from '@brumaombra/ui-vintage/list';
import { LoadingStateCard } from '@brumaombra/ui-vintage/loading-state-card';
import { showMessageDialog } from '@brumaombra/ui-vintage/message-dialog';
import { ResponsiveDialog } from '@brumaombra/ui-vintage/responsive-dialog';
import { forgetWifi, getDevice, restartDevice } from '../../composables/useGizmoApi.js';
import { loadGizmoDevice, useGizmoStore } from '../../composables/useGizmoStore.js';
import { handleBackendErrors, splitUptime } from '../../composables/useGizmoUtils.js';

const FIRST_CHECK_DELAY = 4000; // Milliseconds the device needs to go down and boot again
const CHECK_INTERVAL = 2000; // Milliseconds between the checks of a device that is starting again
const MAX_CHECKS = 20; // Checks before giving up (the setup hotspot has another address, so the device may never answer)
const { t } = useI18n();
const gizmoStore = useGizmoStore();
const isDialogOpen = ref(false);
const selectedAction = ref('restart');
const isRestarting = ref(false);

// What each action asks, sends and says (the dialog and the messages read it, so the two actions share one flow)
const actions = {
    restart: {
        icon: ReloadIcon,
        buttonVariant: 'primary',
        request: restartDevice
    },
    forgetWifi: {
        icon: WifiOff01Icon,
        buttonVariant: 'red',
        request: forgetWifi
    }
};

// Details of the device, as the firmware reports them
const device = computed(() => gizmoStore.value.device);

// Action the dialog is about
const action = computed(() => actions[selectedAction.value]);

// Time since the last boot, as days, hours and minutes
const uptimeLabel = computed(() => t('gizmo.device.uptimeValue', splitUptime(device.value.uptimeSeconds)));

// Free memory in kilobytes
const freeMemoryLabel = computed(() => t('gizmo.device.memoryValue', { kilobytes: Math.round((device.value.freeHeap || 0) / 1024) }));

// Network the device is set up for
const savedNetworkLabel = computed(() => device.value.ssid || t('gizmo.device.notConnected'));

// Whether the device is on a network (the firmware sends the address and the signal only then)
const isConnected = computed(() => Boolean(device.value.ip));

// Open the confirmation of an action
const handleActionPress = actionName => {
    selectedAction.value = actionName;
    isDialogOpen.value = true;
};

// Wait for the device to answer again after a restart, then show its new details
let checkTimer = null;
let checkCount = 0;
const checkDevice = async () => {
    checkCount += 1;

    try {
        // Read the details straight away: a failed check must not turn the card into the error state
        gizmoStore.value.device = await getDevice();
        isRestarting.value = false;
        return;
    } catch {
        // The device is still starting: check again, unless it is time to give up
        if (checkCount < MAX_CHECKS) {
            checkTimer = setTimeout(checkDevice, CHECK_INTERVAL);
        } else {
            isRestarting.value = false;
        }
    }
};

// Send the confirmed action to the device, and wait for it to come back
const handleConfirmPress = async () => {
    const confirmedAction = selectedAction.value;
    isDialogOpen.value = false;

    try {
        setBusy(true);
        await actions[confirmedAction].request();

        // The device restarts about a second after answering
        isRestarting.value = true;
        checkCount = 0;
        clearTimeout(checkTimer);
        checkTimer = setTimeout(checkDevice, FIRST_CHECK_DELAY);
        showMessageDialog({
            type: 'success',
            icon: CheckmarkCircle02Icon,
            title: t(`gizmo.device.${confirmedAction}.successTitle`),
            message: t(`gizmo.device.${confirmedAction}.successMessage`),
            closeText: t('gizmo.common.close'),
            closeButtonIcon: Cancel01Icon
        });
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t(`gizmo.device.${confirmedAction}.error`), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Read the details of the device again after a failed read
const handleRetryPress = async () => {
    try {
        await loadGizmoDevice();
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('gizmo.device.loadErrorTitle'), showDialog: true });
    }
};

// Read the details of the device when the card opens (they may be from an earlier visit)
onMounted(() => loadGizmoDevice().catch(() => {}));

// Stop waiting for the device when leaving the page
onBeforeUnmount(() => clearTimeout(checkTimer));
</script>

<template>
    <div>
        <!-- The details couldn't be read: offer to try again -->
        <EmptyStateCard v-if="!device && gizmoStore.isDeviceLoadFailed" :icon="Alert02Icon" :title="t('gizmo.device.loadErrorTitle')" :description="t('gizmo.device.loadErrorDescription')">
            <template #action>
                <Button variant="primary" @click="handleRetryPress">
                    <HugeiconsIcon :icon="RefreshIcon" class="size-4" />
                    {{ t('gizmo.device.retryAction') }}
                </Button>
            </template>
        </EmptyStateCard>

        <!-- Still reading the details from the device -->
        <LoadingStateCard v-else-if="!device" :title="t('gizmo.device.loading')" />

        <div v-else class="space-y-6">
            <!-- Device -->
            <ListGroup :title="t('gizmo.device.title')" :hint="isRestarting ? t('gizmo.device.restartingHint') : ''">
                <ListRow :label="t('gizmo.device.name')" :icon="CpuIcon" :value="device.name" />
                <ListRow :label="t('gizmo.device.version')" :icon="Tag01Icon" :value="device.version" />
                <ListRow :label="t('gizmo.device.uptime')" :icon="Clock01Icon" :value="uptimeLabel" />
                <ListRow :label="t('gizmo.device.freeMemory')" :icon="RamMemoryIcon" :value="freeMemoryLabel" />
            </ListGroup>

            <!-- Network (the address and the signal exist only while connected) -->
            <ListGroup :title="t('gizmo.device.networkTitle')" :hint="device.accessPointActive ? t('gizmo.device.hotspotActive') : ''">
                <ListRow :label="t('gizmo.device.savedNetwork')" :icon="WifiFullSignalIcon" :value="savedNetworkLabel" to="/wifi" />
                <ListRow v-if="isConnected" :label="t('gizmo.device.ip')" :icon="Globe02Icon" :value="device.ip" />
                <ListRow v-if="isConnected && device.hostname" :label="t('gizmo.device.hostname')" :icon="Link01Icon" :value="device.hostname" />
                <ListRow v-if="isConnected" :label="t('gizmo.device.signal')" :icon="SignalFull02Icon" :value="`${device.signalQuality}%`" />
            </ListGroup>

            <!-- Actions (not while the device is restarting) -->
            <ListGroup v-if="!isRestarting">
                <ListRow :label="t('gizmo.device.restart.action')" :icon="ReloadIcon" action @click="handleActionPress('restart')" />
                <ListRow v-if="device.ssid" :label="t('gizmo.device.forgetWifi.action')" :icon="WifiOff01Icon" action danger @click="handleActionPress('forgetWifi')" />
            </ListGroup>
        </div>

        <!-- Confirmation of the action (a sheet on phones, a dialog on larger screens) -->
        <ResponsiveDialog v-model:open="isDialogOpen" :title="t(`gizmo.device.${selectedAction}.confirmTitle`)" :description="t(`gizmo.device.${selectedAction}.confirmMessage`)">
            <!-- Actions (the main one first: on top in the sheet, rightmost in the dialog) -->
            <template #footer>
                <Button type="button" :variant="action.buttonVariant" @click="handleConfirmPress">
                    <HugeiconsIcon :icon="action.icon" class="size-4" />
                    {{ t(`gizmo.device.${selectedAction}.confirmAction`) }}
                </Button>
                <Button type="button" variant="secondary" @click="isDialogOpen = false">
                    <HugeiconsIcon :icon="Cancel01Icon" class="size-4" />
                    {{ t('gizmo.common.cancel') }}
                </Button>
            </template>
        </ResponsiveDialog>
    </div>
</template>