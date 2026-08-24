package com.rsvpnano.ui

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val LightColors = lightColorScheme(
    primary = Color(0xFF35675F),
    onPrimary = Color.White,
    primaryContainer = Color(0xFFD5E9E3),
    onPrimaryContainer = Color(0xFF153D37),
    secondary = Color(0xFF62766F),
    tertiary = Color(0xFF9A5B24),
    background = Color(0xFFFAF8F2),
    onBackground = Color(0xFF252724),
    surface = Color(0xFFFFFDF7),
    onSurface = Color(0xFF252724),
    surfaceVariant = Color(0xFFE5E9E3),
    onSurfaceVariant = Color(0xFF555C57),
    outline = Color(0xFF747874),
    error = Color(0xFFB3261E),
)

private val DarkColors = darkColorScheme(
    primary = Color(0xFFA5CEC4),
    onPrimary = Color(0xFF0B3731),
    primaryContainer = Color(0xFF244E47),
    onPrimaryContainer = Color(0xFFD5E9E3),
    secondary = Color(0xFFB5CCC3),
    tertiary = Color(0xFFFFB77A),
    background = Color(0xFF191C1A),
    onBackground = Color(0xFFE2E4DF),
    surface = Color(0xFF202421),
    onSurface = Color(0xFFE2E4DF),
    surfaceVariant = Color(0xFF3F4945),
    onSurfaceVariant = Color(0xFFBFC9C4),
    outline = Color(0xFF8D938E),
)

@Composable
fun RsvpNanoTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = if (isSystemInDarkTheme()) DarkColors else LightColors,
        content = content,
    )
}
