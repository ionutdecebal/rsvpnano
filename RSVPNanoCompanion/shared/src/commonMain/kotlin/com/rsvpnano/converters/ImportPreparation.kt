package com.rsvpnano.converters

import com.rsvpnano.models.PendingUpload

object ImportPreparation {
    fun titleForText(preferredTitle: String, text: String, fallback: String): String {
        return preferredTitle.trim().ifEmpty {
            RsvpConverter.titleFromText(text = text, fallback = fallback)
        }
    }

    fun titleForSharedUrl(preferredTitle: String, source: String, host: String): String {
        val cleanedTitle = preferredTitle.trim()
        if (
            cleanedTitle.isNotEmpty() &&
            cleanedTitle != host &&
            cleanedTitle != "www.$host" &&
            !source.contains(cleanedTitle)
        ) {
            return cleanedTitle
        }
        return source
    }

    fun rsvpFileForText(title: String, source: String, text: String, fallbackTitle: String): RsvpBookFile {
        return RsvpConverter.rsvpFile(
            title = titleForText(preferredTitle = title, text = text, fallback = fallbackTitle),
            source = source,
            text = text,
        )
    }

    fun pendingUploadForText(
        id: String,
        title: String,
        source: String,
        text: String,
        createdAt: String,
        fallbackTitle: String,
    ): PendingUpload {
        val cleanedText = text.trim()
        return PendingUpload(
            id = id,
            title = titleForText(preferredTitle = title, text = cleanedText, fallback = fallbackTitle),
            sourceUrl = source.trim().ifEmpty { null },
            body = cleanedText,
            createdAt = createdAt,
        )
    }

    fun pendingUploadForUrl(
        id: String,
        title: String,
        source: String,
        host: String,
        createdAt: String,
    ): PendingUpload {
        val cleanedSource = source.trim()
        return PendingUpload(
            id = id,
            title = titleForSharedUrl(preferredTitle = title, source = cleanedSource, host = host),
            sourceUrl = cleanedSource.ifEmpty { null },
            body = cleanedSource,
            createdAt = createdAt,
        )
    }

    fun prepareSharedImport(
        id: String,
        title: String,
        text: String,
        source: String,
        createdAt: String,
    ): PendingUpload? {
        val body = text.trim()
        if (body.isEmpty()) return null

        val cleanedSource = source.trim()
        val isUrl = body.startsWith("http://") || body.startsWith("https://")
        val sourceIsUrl = cleanedSource.startsWith("http://") || cleanedSource.startsWith("https://")

        return if (isUrl && (cleanedSource.isEmpty() || sourceIsUrl)) {
            val url = cleanedSource.ifEmpty { body }
            pendingUploadForUrl(
                id = id,
                title = title,
                source = url,
                host = hostName(url),
                createdAt = createdAt,
            )
        } else {
            pendingUploadForText(
                id = id,
                title = title,
                source = cleanedSource,
                text = body,
                createdAt = createdAt,
                fallbackTitle = "Shared Content",
            )
        }
    }

    private fun hostName(url: String): String {
        val noProtocol = url.substringAfter("://")
        return noProtocol.substringBefore("/")
    }
}
