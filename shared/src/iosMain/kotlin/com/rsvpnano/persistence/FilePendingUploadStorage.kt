package com.rsvpnano.persistence

import platform.Foundation.NSData
import platform.Foundation.NSFileManager
import platform.Foundation.NSString
import platform.Foundation.NSUTF8StringEncoding
import platform.Foundation.NSURL
import platform.Foundation.containerURLForSecurityApplicationGroupIdentifier
import platform.Foundation.createDirectoryAtURL
import platform.Foundation.defaultManager
import platform.Foundation.dataUsingEncoding
import platform.Foundation.stringWithContentsOfURL
import platform.Foundation.writeToURL

class FilePendingUploadStorage(
    private val fileManager: NSFileManager = NSFileManager.defaultManager,
    private val appGroupIdentifier: String = "group.com.rsvpnano.companion",
) : PendingUploadStorage {
    private fun fileURL(): NSURL? {
        val rootURL = fileManager.containerURLForSecurityApplicationGroupIdentifier(appGroupIdentifier) ?: return null
        val inbox = rootURL.URLByAppendingPathComponent("PendingUploads", isDirectory = true)
        fileManager.createDirectoryAtURL(inbox, withIntermediateDirectories = true, attributes = null, error = null)
        return inbox.URLByAppendingPathComponent("drafts.json")
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
