package com.rsvpnano.app

import com.rsvpnano.models.RememberedNano
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.StateFlow

interface NanoWifiConnector {
    val snapshot: StateFlow<NanoWifiSnapshot>
    val events: Flow<NanoWifiEvent>

    fun start()
    fun stop()
    fun refreshSnapshot()
    fun hasRequiredPermissions(): Boolean
    fun requestNanoNetwork(
        rememberedNano: RememberedNano? = null,
    ): NanoWifiRequestResult
    fun cancelNanoRequest()
    fun releaseRequestedNanoNetwork()
    suspend fun <T> withNanoNetwork(block: suspend () -> T): T
}

sealed interface NanoConnectionState {
    val currentNano: RememberedNano?

    data object Disconnected : NanoConnectionState {
        override val currentNano: RememberedNano? = null
    }

    data class Requesting(
        val rememberedNano: RememberedNano?,
    ) : NanoConnectionState {
        override val currentNano: RememberedNano? = rememberedNano
    }

    data class WifiAttached(
        override val currentNano: RememberedNano?,
    ) : NanoConnectionState

    data class CheckingReader(
        override val currentNano: RememberedNano?,
    ) : NanoConnectionState

    data class ReaderConnected(
        override val currentNano: RememberedNano?,
    ) : NanoConnectionState
}

data class NanoWifiSnapshot(
    val currentNano: RememberedNano? = null,
    val isAttached: Boolean = false,
    val isRequesting: Boolean = false,
) {
    fun toConnectionState(previous: NanoConnectionState): NanoConnectionState {
        val identity = currentNano ?: previous.currentNano
        return when {
            isRequesting -> NanoConnectionState.Requesting(identity)
            isAttached -> NanoConnectionState.WifiAttached(identity)
            else -> NanoConnectionState.Disconnected
        }
    }
}

sealed interface NanoWifiRequestResult {
    data object Started : NanoWifiRequestResult
    data object AlreadyAttached : NanoWifiRequestResult
    data object AlreadyRequesting : NanoWifiRequestResult
    data object MissingPermissions : NanoWifiRequestResult
    data object Unsupported : NanoWifiRequestResult
    data class Failed(val reason: String) : NanoWifiRequestResult
}

sealed interface NanoWifiEvent {
    data object RequestUnavailable : NanoWifiEvent
}

val NanoConnectionState.isConnected: Boolean
    get() = this is NanoConnectionState.ReaderConnected

val NanoConnectionState.isWifiAttached: Boolean
    get() = this is NanoConnectionState.WifiAttached ||
        this is NanoConnectionState.CheckingReader ||
        this is NanoConnectionState.ReaderConnected

val NanoConnectionState.isCheckingReader: Boolean
    get() = this is NanoConnectionState.CheckingReader

val NanoConnectionState.isRequesting: Boolean
    get() = this is NanoConnectionState.Requesting
