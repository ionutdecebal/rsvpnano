package com.rsvpnano.converters

internal object RsvpTextUtils {
    private val blockTags = setOf(
        "address", "article", "aside", "blockquote", "body", "br", "dd", "div", "dl", "dt",
        "figcaption", "figure", "footer", "header", "hr", "li", "main", "ol", "p", "pre",
        "section", "table", "tbody", "td", "tfoot", "th", "thead", "tr", "ul",
    )

    private val skipTags = setOf("head", "math", "nav", "script", "style", "svg")

    private val asciiReplacements = mapOf(
        '\u00a0' to " ", '\u1680' to " ", '\u180e' to " ", '\u2000' to " ", '\u2001' to " ",
        '\u2002' to " ", '\u2003' to " ", '\u2004' to " ", '\u2005' to " ", '\u2006' to " ",
        '\u2007' to " ", '\u2008' to " ", '\u2009' to " ", '\u200a' to " ", '\u2028' to " ",
        '\u2029' to " ", '\u202f' to " ", '\u205f' to " ", '\u3000' to " ",
        '\u2018' to "'", '\u2019' to "'", '\u201a' to "'", '\u201b' to "'", '\u2032' to "'",
        '\u2035' to "'", '\u201c' to "\"", '\u201d' to "\"", '\u201e' to "\"",
        '\u201f' to "\"", '\u00ab' to "\"", '\u00bb' to "\"", '\u2039' to "'",
        '\u203a' to "'", '\u2033' to "\"", '\u2036' to "\"", '\u300c' to "\"",
        '\u300d' to "\"", '\u300e' to "\"", '\u300f' to "\"", '\u2010' to "-",
        '\u2011' to "-", '\u2012' to "-", '\u2013' to "-", '\u2014' to "-",
        '\u2015' to "-", '\u2043' to "-", '\u2212' to "-", '\u2026' to "...",
        '\u2022' to "*", '\u00b7' to "*", '\u2219' to "*", '\u00a9' to "(c)",
        '\u00ae' to "(r)", '\u2122' to "TM", '\ufb00' to "ff", '\ufb01' to "fi",
        '\ufb02' to "fl", '\ufb03' to "ffi", '\ufb04' to "ffl", '\ufb05' to "st",
        '\ufb06' to "st", '\ufffd' to "",
    )

    fun decodeText(data: ByteArray): String? {
        if (data.size >= 3 && data[0] == 0xEF.toByte() && data[1] == 0xBB.toByte() && data[2] == 0xBF.toByte()) {
            return data.copyOfRange(3, data.size).decodeUtf8()
        }
        if (data.size >= 2 && data[0] == 0xFF.toByte() && data[1] == 0xFE.toByte()) {
            return PlatformTextDecoder.decode(data.copyOfRange(2, data.size), "utf-16le")
        }
        if (data.size >= 2 && data[0] == 0xFE.toByte() && data[1] == 0xFF.toByte()) {
            return PlatformTextDecoder.decode(data.copyOfRange(2, data.size), "utf-16be")
        }

        val initialEncoding = detectUtf16WithoutBom(data) ?: "utf-8"
        val decoded = data.decodeOrNull(initialEncoding)
            ?: PlatformTextDecoder.decode(data, "windows-1252")
            ?: PlatformTextDecoder.decode(data, "ISO-8859-1")
            ?: return null

        val declared = sniffDeclaredEncoding(decoded)
        return if (declared != null && declared != initialEncoding) {
            data.decodeOrNull(declared) ?: decoded
        } else {
            decoded
        }
    }

    fun readableText(value: String): String {
        var text = value
        if (looksLikeHTML(text)) {
            text = stripHTML(text)
        }
        text = text.replace("\\r\\n", "\n")
        text = text.replace("\\n", "\n")
        text = text.replace("\\r", "\n")
        text = text.replace("\\t", " ")
        text = text.replace("\r\n", "\n")
        text = text.replace("\r", "\n")
        return paragraphs(text).joinToString("\n\n")
    }

    fun titleFromText(text: String, fallback: String): String {
        if (looksLikeHTML(text)) {
            firstMatch(text, Regex("<title[^>]*>([\\s\\S]*?)</title>", RegexOption.IGNORE_CASE))
                ?.let { cleanedLine(stripHTML(it)) }
                ?.takeIf { it.isNotEmpty() }
                ?.let { return it }
        }

        readableText(text).lineSequence().map(::cleanedLine).firstOrNull { it.isNotEmpty() }
            ?.let { return it.take(80) }

        return fallback
    }

    fun htmlEvents(markup: String): List<RsvpEvent> {
        val blockBreak = "\u0000"
        var text = markup
        skipTags.forEach { tag ->
            text = text.replace(Regex("<$tag\\b[\\s\\S]*?</$tag>", RegexOption.IGNORE_CASE), " ")
        }
        text = Regex("<h[1-6][^>]*>([\\s\\S]*?)</h[1-6]>", RegexOption.IGNORE_CASE).replace(text) { match ->
            val heading = match.groupValues[1].replace(Regex("<br\\b[^>]*>", RegexOption.IGNORE_CASE), " ")
            "$blockBreak@chapter $heading$blockBreak"
        }
        text = text.replace(Regex("<br\\b[^>]*>", RegexOption.IGNORE_CASE), blockBreak)
        text = text.replace(Regex("</(${blockTags.joinToString("|")})\\b[^>]*>", RegexOption.IGNORE_CASE), blockBreak)
        text = text.replace(Regex("<(${blockTags.joinToString("|")})\\b[^>]*>", RegexOption.IGNORE_CASE), " ")
        text = text.replace(Regex("<[^>]+>", RegexOption.IGNORE_CASE), " ")
        text = decodeEntities(text)
        text = text.replace(Regex("[\\r\\n\\t]+"), " ").replace(blockBreak, "\n")

        return text.lineSequence().map(::cleanedLine).mapNotNull { line ->
            when {
                line.isEmpty() -> null
                line.lowercase().startsWith("@chapter ") -> {
                    val value = cleanedLine(line.removePrefix("@chapter "))
                    if (value.isEmpty()) null else RsvpEvent.Chapter(value)
                }
                else -> RsvpEvent.Text(line)
            }
        }.toList()
    }

    fun textEvents(text: String): List<RsvpEvent> {
        val events = mutableListOf<RsvpEvent>()
        val paragraph = mutableListOf<String>()

        fun flushParagraph() {
            val value = cleanedLine(paragraph.joinToString(" "))
            paragraph.clear()
            if (value.isNotEmpty()) {
                events += RsvpEvent.Text(value)
            }
        }

        text.lineSequence().forEach { rawLine ->
            val line = cleanedLine(rawLine)
            val chapter = chapterTitle(line)
            when {
                chapter != null -> {
                    flushParagraph()
                    events += RsvpEvent.Chapter(chapter)
                }
                line.isEmpty() -> flushParagraph()
                else -> paragraph += line
            }
        }
        flushParagraph()
        return events
    }

    fun filenameSafe(value: String): String {
        val mapped = value.map { char ->
            if (char.isLetterOrDigit() || char == ' ' || char == '-' || char == '_') char else '-'
        }
        val collapsed = mapped.joinToString("").replace(Regex("\\s+"), " ")
        val trimmed = collapsed.trim()
        return if (trimmed.isEmpty()) "Untitled" else trimmed.take(80)
    }

    fun cleanedLine(value: String): String = normalizeText(value).trim()

    fun directiveValue(value: String): String = cleanedLine(value).replace("\n", " ")

    fun cleanWordTokens(text: String): List<String> {
        return cleanedLine(text)
            .split(Regex("[\\s]+"))
            .filter { token -> token.isNotEmpty() && token.any(Char::isLetterOrDigit) }
    }

    fun outputTokens(text: String): List<String> {
        return cleanedLine(text)
            .split(Regex("[\\s]+"))
            .filter { token -> token.isNotEmpty() }
    }

    fun chapterTitle(line: String): String? {
        val trimmed = cleanedLine(line)
        if (trimmed.isEmpty() || trimmed.length > 64) {
            return null
        }
        if (trimmed.startsWith("#")) {
            val title = cleanedLine(trimmed.replace(Regex("^#+"), ""))
            return if (title.isEmpty()) null else title
        }

        val lowered = trimmed.lowercase()
        return if (lowered.startsWith("chapter ") || lowered.startsWith("part ") || lowered.startsWith("book ")) {
            trimmed
        } else {
            null
        }
    }

    private fun decodeEntities(value: String): String {
        var text = value
        mapOf(
            "&amp;" to "&",
            "&lt;" to "<",
            "&gt;" to ">",
            "&quot;" to "\"",
            "&#39;" to "'",
            "&apos;" to "'",
            "&nbsp;" to " ",
            "&ldquo;" to "\"",
            "&rdquo;" to "\"",
            "&lsquo;" to "'",
            "&rsquo;" to "'",
            "&ndash;" to "-",
            "&mdash;" to "-",
            "&hellip;" to "...",
        ).forEach { (entity, replacement) ->
            text = text.replace(entity, replacement)
        }
        return replaceNumericEntities(text)
    }

    private fun replaceNumericEntities(text: String): String {
        return Regex("&#(x[0-9a-fA-F]+|\\d+);").replace(text) { match ->
            val token = match.groupValues[1]
            val scalarValue = if (token.startsWith("x", ignoreCase = true)) {
                token.drop(1).toUIntOrNull(16)
            } else {
                token.toUIntOrNull(10)
            }
            scalarValue?.toInt()?.toChar()?.toString().orEmpty()
        }
    }

    private fun detectUtf16WithoutBom(data: ByteArray): String? {
        if (data.size < 4) {
            return null
        }
        return when {
            data[0] == 0x3c.toByte() && data[1] == 0x00.toByte() && data[2] == 0x3f.toByte() && data[3] == 0x00.toByte() -> "utf-16le"
            data[0] == 0x00.toByte() && data[1] == 0x3c.toByte() && data[2] == 0x00.toByte() && data[3] == 0x3f.toByte() -> "utf-16be"
            else -> null
        }
    }

    private fun sniffDeclaredEncoding(text: String): String? {
        val head = text.take(512)
        val value = firstMatch(
            head,
            Regex("(?:encoding|charset)\\s*=\\s*[\"']?\\s*([^\"'>\\s/]+)", RegexOption.IGNORE_CASE)
        ) ?: return null

        return when (value.lowercase()) {
            "utf-8", "utf8" -> "utf-8"
            "utf-16", "utf16" -> "utf-16"
            "utf-16le" -> "utf-16le"
            "utf-16be" -> "utf-16be"
            "windows-1252", "cp1252" -> null
            "iso-8859-1", "latin1" -> null
            else -> null
        }
    }

    private fun stripHTML(value: String): String {
        var text = value
        skipTags.forEach { tag ->
            text = text.replace(Regex("<$tag\\b[\\s\\S]*?</$tag>", RegexOption.IGNORE_CASE), " ")
        }
        text = text.replace(Regex("</?(${blockTags.joinToString("|")}|h[1-6])\\b[^>]*>", RegexOption.IGNORE_CASE), "\n")
        text = text.replace(Regex("<[^>]+>", RegexOption.IGNORE_CASE), " ")
        return decodeEntities(text)
    }

    private fun paragraphs(text: String): List<String> {
        return text.lineSequence().map(::cleanedLine).filter { it.isNotEmpty() }.toList()
    }

    fun looksLikeHTML(value: String): Boolean {
        val lowered = value.lowercase()
        return lowered.contains("<html") || lowered.contains("<body") || lowered.contains("<p") ||
            lowered.contains("<article") || lowered.contains("<main") || lowered.contains("<br") ||
            lowered.contains("<!doctype")
    }

    private fun normalizeText(text: String): String {
        val sb = StringBuilder(text.length)
        var lastWasWhitespace = false

        text.forEach { char ->
            val mapped = asciiReplacements[char]
            if (mapped != null) {
                mapped.forEach { mappedChar ->
                    if (mappedChar.isWhitespace()) {
                        if (!lastWasWhitespace) {
                            sb.append(' ')
                            lastWasWhitespace = true
                        }
                    } else {
                        sb.append(mappedChar)
                        lastWasWhitespace = false
                    }
                }
            } else if (char.isWhitespace()) {
                if (!lastWasWhitespace) {
                    sb.append(' ')
                    lastWasWhitespace = true
                }
            } else {
                sb.append(char)
                lastWasWhitespace = false
            }
        }
        return sb.toString().trim()
    }

    private fun firstMatch(value: String, regex: Regex): String? = regex.find(value)?.groupValues?.getOrNull(1)

    private fun ByteArray.decodeOrNull(charsetName: String): String? {
        return try {
            if (charsetName == "utf-8") decodeUtf8() else PlatformTextDecoder.decode(this, charsetName)
        } catch (_: Exception) {
            null
        }
    }

    private fun ByteArray.decodeUtf8(): String = decodeToString()
}
