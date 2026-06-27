package com.rsvpnano.app

import kotlinx.datetime.TimeZone
import kotlinx.datetime.toLocalDateTime
import kotlin.time.Clock
import kotlin.time.Instant

object SharedAppUtils {
    const val DEFAULT_DEVICE_ADDRESS = "http://192.168.4.1"

    fun normalizedAddress(value: String): String {
        val trimmed = value.trim()
        if (trimmed.isEmpty()) {
            return DEFAULT_DEVICE_ADDRESS
        }
        return if (trimmed.startsWith("http://") || trimmed.startsWith("https://")) {
            trimmed
        } else {
            "http://$trimmed"
        }
    }

    fun nowIso8601(): String = Clock.System.now().toString()

    fun formatCreatedAt(iso8601: String): String {
        val instant = runCatching { Instant.parse(iso8601) }.getOrNull() ?: return iso8601
        val local = instant.toLocalDateTime(TimeZone.currentSystemDefault())
        return "${local.year}-${local.month.toString().padStart(2, '0')}-${local.day.toString().padStart(2, '0')}"
    }
}
