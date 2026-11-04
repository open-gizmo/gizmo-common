import { getDevice } from './useGizmoApi.js';

// Initial state object (a factory, so every reset gets a fresh copy)
const initialState = () => ({
    // Networks found by the last Wi-Fi scan
    networksList: [],

    // Details of the device (what GET /api/device answers, null until it is read)
    device: null,

    // Flag set while the details of the device are being read
    isDeviceLoading: false,

    // Flag set when the details of the device couldn't be read (the card then offers to try again)
    isDeviceLoadFailed: false
});

// Shared Wi-Fi and device state of the layer (the apps keep their own state in their own stores)
export const useGizmoStore = () => useState('gizmo', initialState);

// Reset the Wi-Fi and device state
export const resetGizmoStore = () => {
    const gizmoStore = useGizmoStore();
    gizmoStore.value = initialState();
};

// Read the details of the device into the store, and remember a failure
export const loadGizmoDevice = async () => {
    const gizmoStore = useGizmoStore();
    gizmoStore.value.isDeviceLoading = true;

    try {
        // Get the details of the device
        const device = await getDevice();

        // Keep them in the store
        gizmoStore.value.device = device;
        gizmoStore.value.isDeviceLoadFailed = false;
    } catch (error) {
        // Remember the failure, so the card offers to try again
        gizmoStore.value.isDeviceLoadFailed = true;
        throw error;
    } finally {
        gizmoStore.value.isDeviceLoading = false;
    }
};