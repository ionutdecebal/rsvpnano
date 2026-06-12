package com.rsvpnano.app

import com.rsvpnano.models.RememberedNano
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import platform.NetworkExtension.NEHotspotConfiguration
import platform.NetworkExtension.NEHotspotConfigurationManager
import platform.NetworkExtension.NEHotspotNetwork

class IosNanoWifiConnector : NanoWifiConnector {
    private val _snapshot = MutableStateFlow(NanoWifiSnapshot())
    private val _events = MutableSharedFlow<NanoWifiEvent>(extraBufferCapacity = 4)
    private var requestedSsid: String? = null

    override val snapshot: StateFlow<NanoWifiSnapshot> = _snapshot
    override val events: SharedFlow<NanoWifiEvent> = _events

    override fun start() {
        refreshSnapshot()
    }

    override fun stop() {
        releaseRequestedNanoNetwork()
    }

    override fun refreshSnapshot() {
        NEHotspotNetwork.fetchCurrentWithCompletionHandler { network ->
            val ssid = network?.SSID
            val nano = NanoWifiIdentity.rememberedNanoOrNull(ssid)
            _snapshot.value = if (nano != null) {
                NanoWifiSnapshot(
                    currentNano = nano,
                    isAttached = true,
                    isRequesting = false,
                )
            } else {
                NanoWifiSnapshot()
            }
        }
    }

    override fun hasRequiredPermissions(): Boolean = true

    override fun requestNanoNetwork(
        rememberedNano: RememberedNano?,
    ): NanoWifiRequestResult {
        val targetSsid = rememberedNano?.ssid ?: NanoWifiIdentity.SSID_PREFIX
        if (_snapshot.value.isAttached) return NanoWifiRequestResult.AlreadyAttached
        if (_snapshot.value.isRequesting) return NanoWifiRequestResult.AlreadyRequesting

        val target = rememberedNano ?: RememberedNano(ssid = targetSsid)
        requestedSsid = rememberedNano?.ssid
        _snapshot.update {
            it.copy(
                currentNano = target,
                isRequesting = true,
            )
        }

        val configuration = if (rememberedNano != null) {
            NEHotspotConfiguration(sSID = targetSsid)
        } else {
            NEHotspotConfiguration(sSIDPrefix = targetSsid)
        }
        configuration.joinOnce = true
        NEHotspotConfigurationManager.sharedManager.applyConfiguration(configuration) { error ->
            val alreadyAssociated = error?.code == HOTSPOT_ALREADY_ASSOCIATED_ERROR_CODE.toLong()
            if (error != null && !alreadyAssociated) {
                _snapshot.value = NanoWifiSnapshot()
                _events.tryEmit(NanoWifiEvent.RequestUnavailable)
                return@applyConfiguration
            }
            refreshSnapshot()
        }
        return NanoWifiRequestResult.Started
    }

    override fun cancelNanoRequest() {
        _snapshot.update { it.copy(isRequesting = false) }
    }

    override fun releaseRequestedNanoNetwork() {
        val ssid = requestedSsid ?: _snapshot.value.currentNano?.ssid
        if (ssid != null && NanoWifiIdentity.isNanoSsid(ssid)) {
            NEHotspotConfigurationManager.sharedManager.removeConfigurationForSSID(ssid)
        }
        requestedSsid = null
        cancelNanoRequest()
    }

    override suspend fun <T> withNanoNetwork(block: suspend () -> T): T {
        return block()
    }

    private companion object {
        const val HOTSPOT_ALREADY_ASSOCIATED_ERROR_CODE = 13
    }
}
