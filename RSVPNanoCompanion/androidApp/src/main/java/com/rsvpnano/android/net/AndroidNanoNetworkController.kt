package com.rsvpnano.android.net

import android.content.Context
import android.content.pm.PackageManager
import android.net.ConnectivityManager
import android.net.MacAddress
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiInfo
import android.net.wifi.WifiNetworkSpecifier
import android.os.Build
import android.os.PatternMatcher
import com.rsvpnano.app.NanoWifiConnector
import com.rsvpnano.app.NanoWifiEvent
import com.rsvpnano.app.NanoWifiIdentity
import com.rsvpnano.app.NanoWifiRequestResult
import com.rsvpnano.app.NanoWifiSnapshot
import com.rsvpnano.models.RememberedNano
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update

class AndroidNanoNetworkController(
    context: Context,
) : NanoWifiConnector {
    private val appContext = context.applicationContext
    private val connectivityManager = appContext.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
    private val _snapshot = MutableStateFlow(NanoWifiSnapshot())
    private val _events = MutableSharedFlow<NanoWifiEvent>(extraBufferCapacity = 4)
    override val snapshot: StateFlow<NanoWifiSnapshot> = _snapshot
    override val events: SharedFlow<NanoWifiEvent> = _events
    private var monitorCallback: ConnectivityManager.NetworkCallback? = null
    private var requestCallback: ConnectivityManager.NetworkCallback? = null
    private var requestedNetwork: Network? = null
    private var currentNetwork: Network? = null

    override fun start() {
        if (monitorCallback != null) return
        val callback = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) {
                updateFromNetwork(network, source = NetworkEventSource.Monitor)
            }

            override fun onLosing(network: Network, maxMsToLive: Int) {
                clearIfCurrent(network)
            }

            override fun onLost(network: Network) {
                clearIfCurrent(network)
            }

            override fun onCapabilitiesChanged(network: Network, networkCapabilities: NetworkCapabilities) {
                updateFromCapabilities(network, networkCapabilities, source = NetworkEventSource.Monitor)
            }
        }
        monitorCallback = callback
        connectivityManager.registerNetworkCallback(
            NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .build(),
            callback,
        )
        refreshSnapshot()
    }

    override fun stop() {
        monitorCallback?.let { runCatching { connectivityManager.unregisterNetworkCallback(it) } }
        monitorCallback = null
        cancelNanoRequest()
    }

    override fun requestNanoNetwork(
        rememberedNano: RememberedNano?,
    ): NanoWifiRequestResult {
        val current = snapshot.value
        if (current.isAttached) return NanoWifiRequestResult.AlreadyAttached
        if (requestCallback != null) return NanoWifiRequestResult.AlreadyRequesting
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            return NanoWifiRequestResult.Unsupported
        }
        if (!hasRequiredPermissions()) {
            return NanoWifiRequestResult.MissingPermissions
        }

        cancelNanoRequest()
        return runCatching {
            val request = nanoNetworkRequest(rememberedNano)
            val callback = nanoRequestCallback()
            requestCallback = callback
            markRequestStarted(rememberedNano = rememberedNano)
            connectivityManager.requestNetwork(request, callback, REQUEST_TIMEOUT_MS)
            NanoWifiRequestResult.Started
        }.onFailure { error ->
            requestCallback = null
            requestedNetwork = null
            markRequestStopped()
        }.getOrElse { error ->
            NanoWifiRequestResult.Failed(error.message ?: error::class.simpleName ?: "Android rejected the Wi-Fi scan request.")
        }
    }

    private fun nanoNetworkRequest(rememberedNano: RememberedNano?): NetworkRequest {
        val specifier = WifiNetworkSpecifier.Builder().apply {
            if (rememberedNano != null) {
                setSsid(rememberedNano.ssid)
                rememberedNano.bssid?.toMacAddressOrNull()?.let(::setBssid)
            } else {
                setSsidPattern(PatternMatcher(NanoWifiIdentity.SSID_PREFIX, PatternMatcher.PATTERN_PREFIX))
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
                    updateFromRequestedNetwork(network)
                }

                override fun onLosing(network: Network, maxMsToLive: Int) {
                    clearIfCurrent(network)
                }

                override fun onCapabilitiesChanged(network: Network, networkCapabilities: NetworkCapabilities) {
                    updateFromCapabilities(network, networkCapabilities, source = NetworkEventSource.Request)
                }

                override fun onUnavailable() {
                    requestCallback = null
                    requestedNetwork = null
                    markRequestStopped()
                    publish(NanoWifiEvent.RequestUnavailable)
                }

                override fun onLost(network: Network) {
                    if (requestedNetwork == network) {
                        requestedNetwork = null
                        requestCallback = null
                    }
                    clearIfCurrent(network)
                }
        }
    }

    private fun markRequestStarted(rememberedNano: RememberedNano?) {
        _snapshot.update {
            it.copy(
                currentNano = rememberedNano,
                isRequesting = true,
            )
        }
    }

    private fun markRequestStopped() {
        _snapshot.update {
            it.copy(
                isRequesting = false,
            )
        }
    }

    private fun publish(event: NanoWifiEvent) {
        _events.tryEmit(event)
    }

    override fun cancelNanoRequest() {
        requestCallback?.let { runCatching { connectivityManager.unregisterNetworkCallback(it) } }
        requestCallback = null
        requestedNetwork = null
        _snapshot.update { it.copy(isRequesting = false) }
    }

    override fun releaseRequestedNanoNetwork() {
        val networkToRelease = requestedNetwork
        cancelNanoRequest()
        _snapshot.update {
            if (networkToRelease != null && currentNetwork == networkToRelease) {
                currentNetwork = null
                NanoWifiSnapshot()
            } else {
                it
            }
        }
    }

    override fun refreshSnapshot() {
        val network = currentNetwork
        if (network == null) {
            _snapshot.update {
                if (it.isAttached) NanoWifiSnapshot() else it
            }
            return
        }

        val capabilities = connectivityManager.getNetworkCapabilities(network)
        if (capabilities == null) {
            _snapshot.update {
                currentNetwork = null
                NanoWifiSnapshot()
            }
            return
        }

        updateFromCapabilities(network, capabilities, source = NetworkEventSource.Monitor)
    }

    override suspend fun <T> withNanoNetwork(block: suspend () -> T): T {
        val network = currentNetwork ?: return block()
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

    override fun hasRequiredPermissions(): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return false
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            appContext.checkSelfPermission(android.Manifest.permission.NEARBY_WIFI_DEVICES) ==
                PackageManager.PERMISSION_GRANTED
        } else {
            appContext.checkSelfPermission(android.Manifest.permission.ACCESS_FINE_LOCATION) ==
                PackageManager.PERMISSION_GRANTED
        }
    }

    private fun updateFromNetwork(network: Network, source: NetworkEventSource) {
        val capabilities = connectivityManager.getNetworkCapabilities(network) ?: return
        updateFromCapabilities(network, capabilities, source = source)
    }

    private fun updateFromRequestedNetwork(network: Network) {
        val capabilities = connectivityManager.getNetworkCapabilities(network)
        if (capabilities != null) {
            updateFromCapabilities(network, capabilities, source = NetworkEventSource.Request)
            return
        }

        currentNetwork = network
        _snapshot.value = NanoWifiSnapshot(
            currentNano = snapshot.value.currentNano,
            isAttached = true,
            isRequesting = false,
        )
    }

    private fun updateFromCapabilities(
        network: Network,
        capabilities: NetworkCapabilities,
        source: NetworkEventSource,
    ) {
        if (!capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) return
        val ssid = capabilities.nanoSsidOrNull()
        val bssid = capabilities.nanoBssidOrNull()
        val wasNano = currentNetwork == network && snapshot.value.isAttached
        val isNano = NanoWifiIdentity.isNanoSsid(ssid) || source == NetworkEventSource.Request || wasNano
        if (!isNano && currentNetwork != network) return

        currentNetwork = network
        val identity = NanoWifiIdentity.rememberedNanoOrNull(ssid, bssid) ?: snapshot.value.currentNano
        _snapshot.value = NanoWifiSnapshot(
            currentNano = identity,
            isAttached = isNano,
            isRequesting = false,
        )
    }

    private fun clearIfCurrent(network: Network) {
        if (currentNetwork == network) {
            currentNetwork = null
            _snapshot.value = NanoWifiSnapshot()
        }
    }

    private fun NetworkCapabilities.nanoSsidOrNull(): String? {
        return wifiInfoOrNull()?.ssid?.cleanSsid()
    }

    private fun NetworkCapabilities.nanoBssidOrNull(): String? {
        return wifiInfoOrNull()?.bssid?.takeIf { it.isNotBlank() && it != "02:00:00:00:00:00" }
    }

    private fun NetworkCapabilities.wifiInfoOrNull(): WifiInfo? {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            transportInfo as? WifiInfo
        } else {
            null
        }
    }

    private fun String.cleanSsid(): String? {
        return NanoWifiIdentity.cleanSsid(this)
    }

    private fun String.toMacAddressOrNull(): MacAddress? {
        return runCatching { MacAddress.fromString(this) }.getOrNull()
    }

    companion object {
        private const val REQUEST_TIMEOUT_MS = 6_000
    }

    private enum class NetworkEventSource {
        Monitor,
        Request,
    }
}
