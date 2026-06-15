package com.rsvpnano.app

import com.rsvpnano.models.RememberedNano

object NanoWifiIdentity {
    const val SSID_PREFIX = "RSVP-Nano-"
    const val LEGACY_SSID_PREFIX = "RSVP_Nano-"

    fun cleanSsid(value: String?): String? {
        val clean = value?.trim()?.trim('"').orEmpty()
        return clean.takeIf { it.isNotBlank() && it != UNKNOWN_SSID }
    }

    fun isNanoSsid(value: String?): Boolean {
        val clean = cleanSsid(value) ?: return false
        return clean.startsWith(SSID_PREFIX) || clean.startsWith(LEGACY_SSID_PREFIX)
    }

    fun rememberedNanoOrNull(ssid: String?, bssid: String? = null): RememberedNano? {
        val clean = cleanSsid(ssid) ?: return null
        if (!isNanoSsid(clean)) return null
        return RememberedNano(ssid = clean, bssid = bssid?.takeIf { it.isNotBlank() })
    }

    private const val UNKNOWN_SSID = "<unknown ssid>"
}
