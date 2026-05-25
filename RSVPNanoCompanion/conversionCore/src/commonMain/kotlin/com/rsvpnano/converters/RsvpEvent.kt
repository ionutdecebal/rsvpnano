package com.rsvpnano.converters

sealed class RsvpEvent {
    data class Chapter(val title: String) : RsvpEvent()
    data class Text(val text: String) : RsvpEvent()
}
