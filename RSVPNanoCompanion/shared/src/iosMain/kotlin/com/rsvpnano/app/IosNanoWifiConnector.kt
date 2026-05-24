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

    override val snapshot: StateFlow<NanoWifiSnapshot> = _snapshot
    override val events: SharedFlow<NanoWifiEvent> = _events

    override fun start() = Unit

    override fun stop() {
        cancelNanoRequest()
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
        val target = rememberedNano ?: return NanoWifiRequestResult.Failed("iOS requires a remembered Nano SSID.")
        if (_snapshot.value.isAttached) return NanoWifiRequestResult.AlreadyAttached

        _snapshot.update {
            it.copy(
                currentNano = target,
                isRequesting = true,
            )
        }

        val configuration = NEHotspotConfiguration(sSID = target.ssid)
        NEHotspotConfigurationManager.sharedManager.applyConfiguration(configuration) { error ->
            if (error != null) {
                _snapshot.update { it.copy(isRequesting = false) }
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
        cancelNanoRequest()
    }

    override suspend fun <T> withNanoNetwork(block: suspend () -> T): T {
        return block()
    }

}
