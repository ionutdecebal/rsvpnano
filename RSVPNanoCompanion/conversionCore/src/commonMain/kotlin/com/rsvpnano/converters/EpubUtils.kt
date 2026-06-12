package com.rsvpnano.converters

internal object EpubUtils {
    fun normalizeZipPath(path: String): String {
        return path.replace('\\', '/')
            .replace(Regex("^/+"), "")
    }

    fun zipJoin(base: String, href: String): String {
        val withoutFragment = href.substringBefore('#').substringBefore('?')
        val decoded = percentDecode(withoutFragment)
        if (decoded.startsWith('/')) {
            return collapseZipPath(decoded)
        }
        return collapseZipPath(zipDirname(base) + decoded)
    }

    fun percentDecode(value: String): String {
        val bytes = mutableListOf<Byte>()
        val output = StringBuilder()
        var index = 0
        fun flushBytes() {
            if (bytes.isNotEmpty()) {
                output.append(bytes.toByteArray().decodeToString())
                bytes.clear()
            }
        }
        while (index < value.length) {
            val character = value[index]
            if (character == '%' && index + 2 < value.length) {
                val byte = value.substring(index + 1, index + 3).toIntOrNull(16)
                if (byte != null) {
                    bytes += byte.toByte()
                    index += 3
                    continue
                }
            }
            flushBytes()
            output.append(character)
            index += 1
        }
        flushBytes()
        return output.toString()
    }

    fun fallbackChapterTitle(path: String, index: Int): String {
        val name = path.substringAfterLast('/').substringBeforeLast('.').replace(Regex("[_-]+"), " ")
        val cleaned = RsvpTextUtils.cleanedLine(name)
        return if (cleaned.isEmpty()) {
            "Chapter $index"
        } else {
            cleaned.replaceFirstChar { if (it.isLowerCase()) it.titlecase() else it.toString() }
        }
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
