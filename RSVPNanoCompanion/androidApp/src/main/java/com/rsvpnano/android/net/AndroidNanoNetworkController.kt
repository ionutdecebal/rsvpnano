package com.rsvpnano.android.net

import android.content.Context
import android.content.pm.PackageManager
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiInfo
import android.net.wifi.WifiNetworkSpecifier
import android.os.Build
import android.os.PatternMatcher
import com.rsvpnano.models.RememberedNano
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update

data class NanoNetworkSnapshot(
    val network: Network? = null,
    val ssid: String? = null,
    val bssid: String? = null,
    val hasInternet: Boolean = false,
    val isNanoWifi: Boolean = false,
    val isRequestingNano: Boolean = false,
    val requestFailed: Boolean = false,
    val requestFailureReason: String? = null,
) {
    val identity: RememberedNano?
        get() = ssid?.takeIf { isNanoWifi }?.let { RememberedNano(ssid = it, bssid = bssid) }
}

class AndroidNanoNetworkController(
    context: Context,
) {
    private val appContext = context.applicationContext
    private val connectivityManager = appContext.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
    private val _snapshot = MutableStateFlow(NanoNetworkSnapshot())
    val snapshot: StateFlow<NanoNetworkSnapshot> = _snapshot
    private var monitorCallback: ConnectivityManager.NetworkCallback? = null
    private var requestCallback: ConnectivityManager.NetworkCallback? = null
    private var requestedNetwork: Network? = null

    fun start() {
        if (monitorCallback != null) return
        val callback = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) {
                updateFromNetwork(network)
            }

            override fun onLost(network: Network) {
                _snapshot.update {
                    if (it.network == network) {
                        NanoNetworkSnapshot()
                    } else {
                        it
                    }
                }
            }

            override fun onCapabilitiesChanged(network: Network, networkCapabilities: NetworkCapabilities) {
                updateFromCapabilities(network, networkCapabilities)
            }
        }
        monitorCallback = callback
        connectivityManager.registerNetworkCallback(
            NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .build(),
            callback,
        )
    }

    fun stop() {
        monitorCallback?.let { runCatching { connectivityManager.unregisterNetworkCallback(it) } }
        monitorCallback = null
        cancelNanoRequest()
    }

    fun requestNanoNetwork(rememberedNano: RememberedNano? = null) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            markRequestFailed("Android 10 or newer is required for app Wi-Fi scan.")
            return
        }
        if (!hasRequiredPermissions()) {
            markRequestFailed("Nearby Wi-Fi and location permissions are required for app Wi-Fi scan.")
            return
        }

        cancelNanoRequest()
        runCatching {
            val request = nanoNetworkRequest(rememberedNano)
            val callback = nanoRequestCallback()
            requestCallback = callback
            markRequestStarted()
            connectivityManager.requestNetwork(request, callback, REQUEST_TIMEOUT_MS)
        }.onFailure { error ->
            requestCallback = null
            requestedNetwork = null
            markRequestFailed(error.message ?: error::class.simpleName ?: "Android rejected the Wi-Fi scan request.")
        }
    }

    private fun nanoNetworkRequest(rememberedNano: RememberedNano?): NetworkRequest {
        val specifier = WifiNetworkSpecifier.Builder().apply {
            if (rememberedNano != null) {
                setSsid(rememberedNano.ssid)
            } else {
                setSsidPattern(PatternMatcher(NANO_SSID_PREFIX, PatternMatcher.PATTERN_PREFIX))
            }
        }.build()
        return NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
            .setNetworkSpecifier(specifier)
            .build()
    }

    private fun nanoRequestCallback(): ConnectivityManager.NetworkCallback {
        return object : ConnectivityManager.NetworkCallback() {
                override fun onAvailable(network: Network) {
                    requestedNetwork = network
                    updateFromNetwork(network, isRequesting = true)
                }

                override fun onCapabilitiesChanged(network: Network, networkCapabilities: NetworkCapabilities) {
                    updateFromCapabilities(network, networkCapabilities, isRequesting = true)
                }

                override fun onUnavailable() {
                    markRequestFailed("Android did not find a matching RSVP-Nano Wi-Fi network.")
                }

                override fun onLost(network: Network) {
                    if (requestedNetwork == network) {
                        requestedNetwork = null
                    }
                    _snapshot.update {
                        if (it.network == network) {
                            NanoNetworkSnapshot()
                        } else {
                            it
                        }
                    }
                }
        }
    }

    private fun markRequestStarted() {
        _snapshot.update {
            it.copy(
                isRequestingNano = true,
                requestFailed = false,
                requestFailureReason = null,
            )
        }
    }

    private fun markRequestFailed(reason: String) {
        _snapshot.update {
            it.copy(
                isRequestingNano = false,
                requestFailed = true,
                requestFailureReason = reason,
            )
        }
    }

    fun cancelNanoRequest() {
        requestCallback?.let { runCatching { connectivityManager.unregisterNetworkCallback(it) } }
        requestCallback = null
        requestedNetwork = null
        _snapshot.update { it.copy(isRequestingNano = false) }
    }

    fun releaseRequestedNanoNetwork() {
        val networkToRelease = requestedNetwork
        cancelNanoRequest()
        _snapshot.update {
            if (networkToRelease != null && it.network == networkToRelease) {
                NanoNetworkSnapshot()
            } else {
                it
            }
        }
    }

    suspend fun <T> withNanoNetwork(block: suspend () -> T): T {
        val network = snapshot.value.network ?: return block()
        val previous = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            connectivityManager.boundNetworkForProcess
        } else {
            null
        }
        return try {
            connectivityManager.bindProcessToNetwork(network)
            block()
        } finally {
            connectivityManager.bindProcessToNetwork(previous)
        }
    }

    fun hasRequiredPermissions(): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return false
        val hasFineLocation = appContext.checkSelfPermission(android.Manifest.permission.ACCESS_FINE_LOCATION) ==
            PackageManager.PERMISSION_GRANTED
        val hasNearbyWifi = Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU ||
            appContext.checkSelfPermission(android.Manifest.permission.NEARBY_WIFI_DEVICES) == PackageManager.PERMISSION_GRANTED
        return hasFineLocation && hasNearbyWifi
    }

    private fun updateFromNetwork(network: Network, isRequesting: Boolean = snapshot.value.isRequestingNano) {
        val capabilities = connectivityManager.getNetworkCapabilities(network) ?: return
        updateFromCapabilities(network, capabilities, isRequesting = isRequesting)
    }

    private fun updateFromCapabilities(
        network: Network,
        capabilities: NetworkCapabilities,
        isRequesting: Boolean = snapshot.value.isRequestingNano,
    ) {
        if (!capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) return
        val wifiInfo = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            capabilities.transportInfo as? WifiInfo
        } else {
            null
        }
        val ssid = wifiInfo?.ssid?.cleanSsid()
        val bssid = wifiInfo?.bssid?.takeIf { it.isNotBlank() && it != "02:00:00:00:00:00" }
        val wasNano = snapshot.value.network == network && snapshot.value.isNanoWifi
        val isNano = ssid?.isNanoSsid() == true || isRequesting || wasNano
        if (!isNano && snapshot.value.network != network) return

        _snapshot.value = NanoNetworkSnapshot(
            network = network,
            ssid = ssid,
            bssid = bssid,
            hasInternet = capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET),
            isNanoWifi = isNano,
            isRequestingNano = false,
            requestFailed = false,
            requestFailureReason = null,
        )
    }

    private fun String.cleanSsid(): String? {
        val value = trim().trim('"')
        return value.takeIf { it.isNotBlank() && it != UNKNOWN_SSID }
    }

    companion object {
        const val NANO_SSID_PREFIX = "RSVP-Nano-"
        private const val LEGACY_NANO_SSID_PREFIX = "RSVP_Nano-"
        private const val UNKNOWN_SSID = "<unknown ssid>"
        private const val REQUEST_TIMEOUT_MS = 15_000

        fun String.isNanoSsid(): Boolean {
            return startsWith(NANO_SSID_PREFIX) || startsWith(LEGACY_NANO_SSID_PREFIX)
        }
    }
}
