package com.rsvpnano.converters

internal object EpubUtils {
    fun normalizeZipPath(path: String): String {
        return path.replace('\\', '/')
            .replace(Regex("^/+"), "")
    }

    fun zipJoin(base: String, href: String): String {
        val withoutFragment = href.substringBefore('#').substringBefore('?')
        val decoded = withoutFragment
        if (decoded.startsWith('/')) {
            return collapseZipPath(decoded)
        }
        return collapseZipPath(zipDirname(base) + decoded)
    }

    fun fallbackChapterTitle(path: String, index: Int): String {
        val name = path.substringAfterLast('/').substringBeforeLast('.').replace(Regex("[_-]+"), " ")
        val cleaned = RsvpTextUtils.cleanedLine(name)
        return if (cleaned.isEmpty()) "Chapter $index" else cleaned
    }

    private fun zipDirname(path: String): String {
        val normalized = normalizeZipPath(path)
        val slash = normalized.lastIndexOf('/')
        return if (slash < 0) "" else normalized.substring(0, slash + 1)
    }

    private fun collapseZipPath(path: String): String {
        val parts = mutableListOf<String>()
        normalizeZipPath(path).split('/').forEach { part ->
            when (part) {
                "", "." -> Unit
                ".." -> if (parts.isNotEmpty()) parts.removeAt(parts.lastIndex)
                else -> parts += part
            }
        }
        return parts.joinToString("/")
    }
}
