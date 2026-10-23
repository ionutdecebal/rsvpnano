package com.rsvpnano.web.setup

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive

internal data class DeployedRelease(
    val version: String,
    val firmware: Map<String, String>,
    val id: String = "classic",
    val label: String = "RSVP Nano — Classic",
)

internal fun List<DeployedRelease>.forBoard(board: String): List<DeployedRelease> =
    filter { board in it.firmware }

internal fun parseDeployedReleases(body: String): List<DeployedRelease> {
    val root = Json.parseToJsonElement(body).jsonObject
    fun parse(item: JsonObject, preview: Boolean): DeployedRelease = DeployedRelease(
        version = item.getValue("version").jsonPrimitive.content,
        firmware = item.getValue("firmware").jsonObject.mapValues { it.value.jsonPrimitive.content },
        id = if (preview) "new-ui" else "classic",
        label = if (preview) "RSVP Nano — New UI (preview)" else "RSVP Nano — Classic",
    )
    // Each board can ship independently. Older deployments contain only newUi.
    val previews = root["newUiVariants"]?.jsonArray?.map { parse(it.jsonObject, true) }
        ?: listOfNotNull(root["newUi"]?.jsonObject?.let { parse(it, true) })
    return listOf(parse(root, false)) + previews
}
