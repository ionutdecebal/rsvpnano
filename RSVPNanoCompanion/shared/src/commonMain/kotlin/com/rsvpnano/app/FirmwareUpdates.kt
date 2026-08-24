package com.rsvpnano.app

import com.rsvpnano.api.RepositoryClient
import com.rsvpnano.models.CompanionAppSettings
import com.rsvpnano.models.FirmwareUpdate
import com.rsvpnano.models.FirmwareUpdateTarget
import com.rsvpnano.models.FirmwareRelease
import com.rsvpnano.models.NanoInfo
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.persistence.AppSettingsStore

class FirmwareUpdates(
    private val repository: RepositoryClient,
    private val settingsStore: AppSettingsStore,
) {
    suspend fun rememberDevice(info: NanoInfo, settings: NanoSettings) {
        rememberDevice(info.firmwareVersion, info.otaAsset, settings)
    }

    suspend fun rememberDevice(currentVersion: String, otaAsset: String, settings: NanoSettings) {
        if (currentVersion.isBlank() || otaAsset.isBlank()) return
        updateSettings {
            it.copy(
                firmwareUpdateTarget = FirmwareUpdateTarget(
                    currentVersion = currentVersion,
                    otaAsset = otaAsset,
                    owner = settings.updates.repositoryOwner,
                    tag = settings.updates.releaseTag,
                ),
            )
        }
    }

    suspend fun setNotificationsEnabled(enabled: Boolean) {
        updateSettings { it.copy(firmwareNotificationsEnabled = enabled) }
    }

    suspend fun pendingNotification(): FirmwareUpdate? {
        val settings = settingsStore.load()
        val target = settings.firmwareUpdateTarget ?: return null
        if (!settings.firmwareNotificationsEnabled) return null

        val source = releaseSource(target.owner, target.tag) ?: return null
        val release = repository.fetchFirmwareRelease(source.owner, source.repository, source.tag)
        return pendingFirmwareUpdate(target, release, settings.lastNotifiedFirmwareVersion)
    }

    suspend fun markNotified(version: String) {
        updateSettings { it.copy(lastNotifiedFirmwareVersion = version) }
    }

    private suspend fun updateSettings(transform: (CompanionAppSettings) -> CompanionAppSettings) {
        settingsStore.save(transform(settingsStore.load()))
    }
}

internal fun pendingFirmwareUpdate(
    target: FirmwareUpdateTarget,
    release: FirmwareRelease,
    lastNotifiedVersion: String?,
): FirmwareUpdate? {
    if (release.version == target.currentVersion || target.otaAsset !in release.assets) return null
    val currentCommit = target.currentVersion.substringAfter('+', "")
    if (currentCommit.isNotEmpty() && currentCommit == release.version.substringAfter('+', "")) return null
    if (release.version == lastNotifiedVersion) return null
    return FirmwareUpdate(target.currentVersion, release.version)
}

internal data class FirmwareReleaseSource(
    val owner: String,
    val repository: String,
    val tag: String,
)

internal const val DefaultFirmwareRepositoryOwner = "ionutdecebal"

internal fun FirmwareReleaseSource.catalogContentUrl(path: String): String =
    "https://raw.githubusercontent.com/$owner/$repository/${tag.ifBlank { "main" }}/$path"

internal fun releaseSource(ownerValue: String, tagValue: String): FirmwareReleaseSource? {
    var owner = ownerValue.trim().ifBlank { DefaultFirmwareRepositoryOwner }
    var repository = "rsvpnano"
    var tag = tagValue.trim()

    fun applyRepository(value: String): Boolean {
        val parts = value.trim().split('/', limit = 2)
        if (parts.size != 2 || parts.any(String::isBlank)) return false
        owner = parts[0].trim()
        repository = parts[1].trim()
        return true
    }

    applyRepository(owner)
    val at = tag.indexOf('@')
    if (at in 1 until tag.lastIndex) {
        val repositoryPart = tag.substring(0, at).trim()
        tag = tag.substring(at + 1).trim()
        if (!applyRepository(repositoryPart) && repositoryPart.isNotBlank()) {
            repository = repositoryPart
        }
    }

    return if (owner.isBlank() || repository.isBlank()) null else FirmwareReleaseSource(owner, repository, tag)
}
