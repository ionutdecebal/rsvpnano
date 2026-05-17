package com.rsvpnano.converters

import org.w3c.dom.Element
import java.io.ByteArrayInputStream
import java.util.zip.ZipInputStream
import javax.xml.parsers.DocumentBuilderFactory

internal actual object PlatformEpubConverter {
    override fun convert(data: ByteArray, filename: String): RsvpBookFile {
        val entries = readEntries(data)
        val containerXml = entries[EpubUtils.normalizeZipPath("META-INF/container.xml").lowercase()]
            ?: throw RsvpConversionError.unsupportedEpub
        val opfPath = containerRootfile(containerXml) ?: throw RsvpConversionError.unsupportedEpub
        val packageXml = entries[EpubUtils.normalizeZipPath(opfPath).lowercase()]
            ?: throw RsvpConversionError.unsupportedEpub

        val packageInfo = parsePackage(packageXml, opfPath)
        val paths = if (packageInfo.spinePaths.isNotEmpty()) packageInfo.spinePaths else packageInfo.manifestContentPaths
        if (paths.isEmpty()) {
            throw RsvpConversionError.unsupportedEpub
        }

        val events = mutableListOf<RsvpEvent>()
        paths.forEachIndexed { index, spinePath ->
            val chapterData = entries[EpubUtils.normalizeZipPath(spinePath).lowercase()] ?: return@forEachIndexed
            val markup = RsvpTextUtils.decodeText(chapterData) ?: return@forEachIndexed
            val chapterEvents = RsvpTextUtils.htmlEvents(markup).toMutableList()
            if (chapterEvents.none { it is RsvpEvent.Chapter }) {
                chapterEvents.add(0, RsvpEvent.Chapter(EpubUtils.fallbackChapterTitle(spinePath, index + 1)))
            }
            if (chapterEvents.any { it is RsvpEvent.Text }) {
                events += chapterEvents
            }
        }

        if (events.isEmpty()) {
            throw RsvpConversionError.unsupportedEpub
        }

        return RsvpConverter.rsvpFile(
            title = packageInfo.title.ifBlank { RsvpConverter.filenameWithoutExtension(filename) },
            author = packageInfo.author,
            source = filename,
            events = events,
        )
    }

    private fun readEntries(data: ByteArray): Map<String, ByteArray> {
        val result = linkedMapOf<String, ByteArray>()
        ZipInputStream(ByteArrayInputStream(data)).use { zip ->
            while (true) {
                val entry = zip.nextEntry ?: break
                if (!entry.isDirectory) {
                    result[EpubUtils.normalizeZipPath(entry.name).lowercase()] = zip.readBytes()
                }
                zip.closeEntry()
            }
        }
        return result
    }

    private fun containerRootfile(xml: String): String? {
        val document = parseXml(xml)
        val nodes = document.getElementsByTagName("rootfile")
        for (index in 0 until nodes.length) {
            val element = nodes.item(index) as? Element ?: continue
            val value = element.getAttribute("full-path")
            if (value.isNotBlank()) return value
        }
        return null
    }

    private fun parsePackage(xml: String, opfPath: String): EpubPackage {
        val document = parseXml(xml)
        var title = ""
        var author = ""
        val manifest = mutableMapOf<String, EpubManifestItem>()
        val spinePaths = mutableListOf<String>()
        val manifestContentPaths = mutableListOf<String>()

        val metadataNodes = document.getElementsByTagName("metadata")
        for (index in 0 until metadataNodes.length) {
            val metadata = metadataNodes.item(index) as? Element ?: continue
            if (title.isBlank()) title = textContentByTag(metadata, "title")
            if (author.isBlank()) author = textContentByTag(metadata, "creator")
        }

        val itemNodes = document.getElementsByTagName("item")
        for (index in 0 until itemNodes.length) {
            val item = itemNodes.item(index) as? Element ?: continue
            val id = item.getAttribute("id")
            val href = item.getAttribute("href")
            if (id.isBlank() || href.isBlank()) continue
            val mediaType = item.getAttribute("media-type")
            val path = EpubUtils.zipJoin(opfPath, href)
            val contentItem = EpubManifestItem(path = path, mediaType = mediaType)
            manifest[id] = contentItem
            if (isContentDocument(path, mediaType)) {
                manifestContentPaths += path
            }
        }

        val itemRefNodes = document.getElementsByTagName("itemref")
        for (index in 0 until itemRefNodes.length) {
            val itemref = itemRefNodes.item(index) as? Element ?: continue
            val idref = itemref.getAttribute("idref")
            val item = manifest[idref] ?: continue
            if (isContentDocument(item.path, item.mediaType)) {
                spinePaths += item.path
            }
        }

        return EpubPackage(
            title = title,
            author = author,
            spinePaths = spinePaths,
            manifestContentPaths = manifestContentPaths,
        )
    }

    private fun parseXml(xml: String) =
        DocumentBuilderFactory.newInstance().apply { isNamespaceAware = true }.newDocumentBuilder()
            .parse(ByteArrayInputStream(xml.toByteArray(Charsets.UTF_8)))

    private fun textContentByTag(parent: Element, tag: String): String {
        val nodes = parent.getElementsByTagName(tag)
        if (nodes.length == 0) return ""
        return RsvpTextUtils.cleanedLine(nodes.item(0).textContent ?: "")
    }

    private fun isContentDocument(path: String, mediaType: String): Boolean {
        val loweredPath = path.lowercase()
        val loweredType = mediaType.lowercase()
        return loweredType == "application/xhtml+xml" ||
            loweredType == "text/html" ||
            loweredPath.endsWith(".xhtml") ||
            loweredPath.endsWith(".html") ||
            loweredPath.endsWith(".htm")
    }

    private data class EpubManifestItem(val path: String, val mediaType: String)

    private data class EpubPackage(
        val title: String,
        val author: String,
        val spinePaths: List<String>,
        val manifestContentPaths: List<String>,
    )
}
