package com.rsvpnano.ui

enum class BookJobStep(val activeLabel: String, val doneLabel: String) {
    Convert("Converting", "Converted"),
    Prepare("Preparing", "Prepared"),
    Upload("Uploading", "Uploaded"),
}

data class BookJob(
    val active: BookJobStep,
    val name: String,
    val done: List<BookJobStep> = emptyList(),
    val progress: Float? = null,
) {
    val percent: Int?
        get() = progress?.coerceIn(0f, 1f)?.let { (it * 100).toInt() }
}
