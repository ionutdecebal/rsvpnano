package com.rsvpnano.api

import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoInfo

/**
 * Lightweight API client interface for device interactions. Implement with Ktor in commonMain
 * or provide a platform-backed implementation if preferred.
 */
interface NanoClient {
    suspend fun fetchInfo(baseUrl: String): NanoInfo
    suspend fun listBooks(baseUrl: String): List<NanoBook>
    suspend fun uploadBook(baseUrl: String, name: String, data: ByteArray, category: String? = null): NanoBook
}
