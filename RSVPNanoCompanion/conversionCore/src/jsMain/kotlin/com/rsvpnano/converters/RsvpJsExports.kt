@file:OptIn(ExperimentalJsExport::class)

package com.rsvpnano.converters

import org.khronos.webgl.Uint8Array
import kotlin.js.ExperimentalJsExport
import kotlin.js.JsExport
import kotlin.js.JsName

@JsExport
@JsName("rsvpSupportedExtensions")
fun rsvpSupportedExtensions(): Array<String> = arrayOf(
    ".epub",
    ".html",
    ".htm",
    ".xhtml",
    ".md",
    ".markdown",
    ".txt",
    ".rsvp",
)

@JsExport
@JsName("rsvpSideCarSuffixes")
fun rsvpSideCarSuffixes(): Array<String> = arrayOf(".rsvp.failed", ".rsvp.tmp", ".rsvp.converting")

@JsExport
@JsName("rsvpDefaultOutputMode")
fun rsvpDefaultOutputMode(): String = "rsvp"

@JsExport
@JsName("rsvpExtensionForName")
fun rsvpExtensionForName(filename: String): String = RsvpSupportedFileTypes.extensionFor(filename)

@JsExport
@JsName("rsvpStripExtension")
fun rsvpStripExtension(filename: String): String = RsvpConverter.filenameWithoutExtension(filename)

@JsExport
@JsName("rsvpConvertTextToRsvp")
fun rsvpConvertTextToRsvp(filename: String, title: String?, source: String?, text: String): dynamic {
    val resolvedTitle = title?.takeIf { it.isNotBlank() } ?: RsvpConverter.filenameWithoutExtension(filename)
    return RsvpConverter.rsvpFile(
        title = resolvedTitle,
        source = source?.takeIf { it.isNotBlank() } ?: filename,
        text = text,
    ).toJsResult()
}

@JsExport
@JsName("rsvpConvertHtmlToRsvp")
fun rsvpConvertHtmlToRsvp(filename: String, title: String?, source: String?, markup: String): dynamic {
    val resolvedTitle = title?.takeIf { it.isNotBlank() }
        ?: RsvpConverter.titleFromText(markup, fallback = RsvpConverter.filenameWithoutExtension(filename))
    return RsvpConverter.rsvpFile(
        title = resolvedTitle,
        source = source?.takeIf { it.isNotBlank() } ?: filename,
        text = markup,
    ).toJsResult()
}

@JsExport
@JsName("rsvpConvertBytesToRsvp")
fun rsvpConvertBytesToRsvp(filename: String, bytes: Uint8Array): dynamic {
    return RsvpConverter.bookFile(bytes.toByteArray(), filename).toJsResult()
}

private fun Uint8Array.toByteArray(): ByteArray {
    val result = ByteArray(length)
    for (index in 0 until length) {
        result[index] = asDynamic()[index].unsafeCast<Int>().toByte()
    }
    return result
}

private fun RsvpBookFile.toJsResult(): dynamic {
    val result = js("{}")
    result.filename = filename
    result.title = title
    result.text = data.decodeToString()
    result.wordCount = wordCount
    result.chapterCount = chapterCount
    return result
}
