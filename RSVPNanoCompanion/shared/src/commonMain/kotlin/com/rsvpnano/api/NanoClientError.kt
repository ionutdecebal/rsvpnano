package com.rsvpnano.api

class NanoClientError(message: String, cause: Throwable? = null) : IllegalStateException(message, cause)
