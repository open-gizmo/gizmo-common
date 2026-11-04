import { delay, translate } from './useGizmoUtils.js';

// Wi-Fi connection status codes returned by the device
const connectionStatus = Object.freeze({
    WIFI_TRY: 2,
    WIFI_OK: 1,
    WIFI_KO: 0
});

// Fetch either the real ESP endpoint or the local Nitro mock endpoint (the base address comes from the runtime config: espBaseUrl)
export const fetchEsp = (path, options = {}) => {
    const runtimeConfig = useRuntimeConfig();
    const baseUrl = (runtimeConfig.public.espBaseUrl || '').trim();
    const normalizedBaseUrl = baseUrl ? baseUrl.replace(/\/$/, '') : '';
    const normalizedPath = path.startsWith('/') ? path : `/${path}`;
    const resolvedUrl = normalizedBaseUrl ? `${normalizedBaseUrl}${normalizedPath}` : normalizedPath;
    return $fetch(resolvedUrl, options);
};

// Fetch the list of visible WiFi networks
export const getNetworks = async () => {
    const response = await fetchEsp('/api/networks');
    return response.data || {};
};

// Fetch the details of the device: name, firmware, uptime, memory, saved network, address and signal
export const getDevice = async () => {
    const response = await fetchEsp('/api/device', { retry: 0 });
    return response.data || {};
};

// Ask the device to restart (it answers first, and restarts about a second later)
export const restartDevice = async () => {
    await fetchEsp('/api/restart', { method: 'POST' });
};

// Ask the device to forget the saved network (it answers first, then restarts into its setup hotspot)
export const forgetWifi = async () => {
    await fetchEsp('/api/forget-wifi', { method: 'POST' });
};

// Poll the device until the WiFi connection attempt completes
const checkWiFiConnectionPolling = async () => {
    const maxAttempts = 45;

    // Poll the device every second to check the WiFi connection status
    for (let attempt = 1; attempt <= maxAttempts; attempt += 1) {
        try {
            const response = await fetchEsp('/api/check-connection', { retry: 0 });
            const status = Number(response.data?.connectionStatus);

            // If the device is still trying to connect, continue polling
            switch (status) {
                // Trying to connect
                case connectionStatus.WIFI_TRY:
                    break;

                // Connection successful
                case connectionStatus.WIFI_OK:
                    return {
                        ssid: response.data?.ssid || '',
                        ip: response.data?.ip || '',
                        hostname: response.data?.hostname || ''
                    };

                // Connection failed
                case connectionStatus.WIFI_KO:
                    throw new Error(translate('gizmo.api.wifiWrongCredentials'));

                // Unknown status code
                default:
                    throw new Error(translate('gizmo.api.wifiUnknownStatus'));
            }
        } catch (error) {
            if (error instanceof Error && error.message !== 'fetch failed') {
                throw error;
            }
        }

        // Wait 1 second before the next polling attempt
        await delay(1000);
    }

    // If we exhaust all attempts without a successful connection, throw a timeout error
    throw new Error(translate('gizmo.api.wifiTimeout'));
};

// Send WiFi credentials and wait for the final connection result
export const connectToWiFi = async (ssid, password) => {
    try {
        // Call the device API to start the WiFi connection process
        const response = await fetchEsp('/api/connect', {
            method: 'POST',
            body: {
                ssid,
                password
            }
        });

        // If the device is trying to connect, start polling for the result
        if (Number(response.data?.connectionStatus) === connectionStatus.WIFI_TRY) {
            return await checkWiFiConnectionPolling();
        }

        // If the device returned an immediate failure, throw an error
        throw new Error(translate('gizmo.api.wifiFailed'));
    } catch (error) {
        if (error instanceof Error) throw error;
        throw new Error(translate('gizmo.api.wifiConnectError'));
    }
};