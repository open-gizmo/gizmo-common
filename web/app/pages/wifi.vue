<script setup>
import { computed, onMounted } from 'vue';
import WifiFullSignalIcon from '@hugeicons/core-free-icons/WifiFullSignalIcon';
import { ListGroup, ListRow } from '@brumaombra/ui-vintage/list';
import GizmoPage from '../components/gizmo/GizmoPage.vue';
import WifiNetworkPicker from '../components/wifi/WifiNetworkPicker.vue';
import { loadGizmoDevice, useGizmoStore } from '../composables/useGizmoStore.js';

const { t } = useI18n();
const gizmoStore = useGizmoStore();

// Network the device is set up for
const currentNetworkLabel = computed(() => {
    return gizmoStore.value.device?.ssid || t('gizmo.wifi.notConnected');
});

// Read the saved network from the device when the page opens (the list of networks below does not depend on it, so a failure is not shown)
onMounted(() => loadGizmoDevice().catch(() => {}));

// Page metadata
definePageMeta({
    layout: 'dashboard'
});
</script>

<template>
    <GizmoPage :title="t('gizmo.wifi.title')">
        <!-- Current network (the name wraps, a network name can be long) -->
        <ListGroup :title="t('gizmo.wifi.currentNetworkLabel')">
            <ListRow :label="currentNetworkLabel" :icon="WifiFullSignalIcon" />
        </ListGroup>

        <!-- Networks found by the device, and the connection -->
        <WifiNetworkPicker :confirm-title="t('gizmo.wifi.connectConfirmTitle')" confirm-message-key="gizmo.wifi.connectConfirmMessage" />
    </GizmoPage>
</template>