package com.rsvpnano.models

import kotlinx.serialization.Serializable

@Serializable
data class NanoBook(
    val id: String? = null,
    val title: String,
    val author: String? = null,
    val progressPercent: Int? = null,
    val category: String? = null
)

@Serializable
data class PendingUpload(
    val id: String,
    val title: String,
    val sourceUrl: String? = null,
    val body: String,
    val createdAt: String // ISO-8601 timestamp string; keep simple for portability
)

@Serializable
data class NanoInfo(
    val name: String,
    val mode: String? = null,
    val baseUrl: String? = null,
    val networkSsid: String? = null,
    val pairingCode: String? = null
)
