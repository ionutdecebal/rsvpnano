package com.rsvpnano.converters

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
}
