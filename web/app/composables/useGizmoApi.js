import { delay, translate } from './useGizmoUtils.js';

// Wi-Fi connection status codes returned by the device
const connectionStatus = Object.freeze({
    WIFI_TRY: 2,
    WIFI_OK: 1,
    WIFI_KO: 0
});

// Refuse a successful answer that isn't JSON: ofetch's parser never fails, it hands back the raw text, which then read as an empty answer (an error status is left to ofetch, so the error keeps its status for handleBackendErrors)
const refuseNonJsonAnswer = ({ response }) => {
    if (response.ok && typeof response._data === 'string') {
        throw new Error(translate('gizmo.api.invalidResponse'));
    }
};

// Read the data of an answer of the device, which must be an object: anything else is an answer the app can't use, so it fails with a message instead of carrying on with nothing
export const readData = response => {
    const data = response?.data;
    if (data === null || typeof data !== 'object' || Array.isArray(data)) {
        throw new Error(translate('gizmo.api.invalidResponse'));
    }
    return data;
};

// Fetch either the real ESP endpoint or the local Nitro mock endpoint (the base address comes from the runtime config: espBaseUrl)
export const fetchEsp = (path, options = {}) => {
    const runtimeConfig = useRuntimeConfig();
    const baseUrl = (runtimeConfig.public.espBaseUrl || '').trim();
    const normalizedBaseUrl = baseUrl ? baseUrl.replace(/\/$/, '') : '';
    const normalizedPath = path.startsWith('/') ? path : `/${path}`;
    const resolvedUrl = normalizedBaseUrl ? `${normalizedBaseUrl}${normalizedPath}` : normalizedPath;

    // Answers must be JSON, unless the caller asks for another type (the camera's snapshot is a blob); the caller's own onResponse hooks still run, after this check
    if (options.responseType) {
        return $fetch(resolvedUrl, options);
    }
    const onResponse = [refuseNonJsonAnswer, ...[options.onResponse ?? []].flat()];
    return $fetch(resolvedUrl, { ...options, onResponse });
};

// Fetch the list of visible Wi-Fi networks
export const getNetworks = async () => {
    const response = await fetchEsp('/api/networks');
    return readData(response);
};

// Fetch the details of the device: name, firmware, uptime, memory, saved network, address and signal
export const getDevice = async () => {
    const response = await fetchEsp('/api/device', { retry: 0 });
    return readData(response);
};

// Ask the device to restart (it answers first, and restarts about a second later)
export const restartDevice = async () => {
    await fetchEsp('/api/restart', { method: 'POST' });
};

// Ask the device to forget the saved network (it answers first, then restarts into its setup hotspot)
export const forgetWifi = async () => {
    await fetchEsp('/api/forget-wifi', { method: 'POST' });
};

// Poll the device until the Wi-Fi connection attempt completes
const checkWiFiConnectionPolling = async () => {
    const maxAttempts = 45;

    // Poll the device every second to check the Wi-Fi connection status
    for (let attempt = 1; attempt <= maxAttempts; attempt += 1) {
        try {
            const response = await fetchEsp('/api/check-connection', { retry: 0 });
            const data = readData(response);
            const status = Number(data.connectionStatus);

            // If the device is still trying to connect, continue polling
            switch (status) {
                // Trying to connect
                case connectionStatus.WIFI_TRY:
                    break;

                // Connection successful
                case connectionStatus.WIFI_OK:
                    return {
                        ssid: data.ssid || '',
                        ip: data.ip || '',
                        hostname: data.hostname || ''
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

// Send Wi-Fi credentials and wait for the final connection result
export const connectToWiFi = async (ssid, password) => {
    try {
        // Call the device API to start the Wi-Fi connection process
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