package com.rsvpnano.persistence

import platform.Foundation.NSFileManager
import platform.Foundation.NSString
import platform.Foundation.NSUTF8StringEncoding
import platform.Foundation.NSURL
import platform.Foundation.containerURLForSecurityApplicationGroupIdentifier
import platform.Foundation.createDirectoryAtURL
import platform.Foundation.dataUsingEncoding
import platform.Foundation.defaultManager

class FileRssFeedStorage(
    private val fileManager: NSFileManager = NSFileManager.defaultManager,
    private val appGroupIdentifier: String = "group.com.rsvpnano.companion",
) : RssFeedStorage {
    private fun fileURL(): NSURL? {
        val rootURL = fileManager.containerURLForSecurityApplicationGroupIdentifier(appGroupIdentifier) ?: return null
        val folder = rootURL.URLByAppendingPathComponent("RSSFeeds", isDirectory = true)
        fileManager.createDirectoryAtURL(folder, withIntermediateDirectories = true, attributes = null, error = null)
        return folder.URLByAppendingPathComponent("feeds.json")
    }

    override suspend fun readText(): String? {
        val url = fileURL() ?: return null
        return NSString.stringWithContentsOfURL(url, NSUTF8StringEncoding, null)
    }

    override suspend fun writeText(value: String) {
        val url = fileURL() ?: return
        val data = value.dataUsingEncoding(NSUTF8StringEncoding) ?: return
        data.writeToURL(url, atomically = true)
    }
}
