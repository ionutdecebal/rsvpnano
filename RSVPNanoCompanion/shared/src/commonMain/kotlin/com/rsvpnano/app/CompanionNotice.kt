package com.rsvpnano.app

sealed interface CompanionNotice {
    val message: String
    val showTransient: Boolean

    data class Neutral(
        override val message: String,
        override val showTransient: Boolean = false,
    ) : CompanionNotice

    data class Success(
        override val message: String,
        override val showTransient: Boolean = true,
    ) : CompanionNotice

    data class Attention(
        override val message: String,
        override val showTransient: Boolean = true,
    ) : CompanionNotice

    data class Error(
        override val message: String,
        override val showTransient: Boolean = true,
    ) : CompanionNotice
}
