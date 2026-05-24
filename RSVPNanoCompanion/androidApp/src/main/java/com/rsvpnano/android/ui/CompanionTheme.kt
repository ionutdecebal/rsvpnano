package com.rsvpnano.android.ui

import android.app.Activity
import android.content.Context
import android.content.ContextWrapper
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.provider.OpenableColumns
import android.provider.Settings
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.pullrefresh.PullRefreshIndicator
import androidx.compose.material.pullrefresh.pullRefresh
import androidx.compose.material.pullrefresh.rememberPullRefreshState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.MenuBook
import androidx.compose.material.icons.outlined.CheckCircle
import androidx.compose.material.icons.outlined.CloudUpload
import androidx.compose.material.icons.outlined.Delete
import androidx.compose.material.icons.outlined.Edit
import androidx.compose.material.icons.outlined.Newspaper
import androidx.compose.material.icons.outlined.RssFeed
import androidx.compose.material.icons.outlined.Sync
import androidx.compose.material.icons.outlined.UploadFile
import androidx.compose.material.icons.outlined.WarningAmber
import androidx.compose.material.icons.outlined.Wifi
import androidx.compose.material3.AssistChip
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.SnackbarDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.dp
import com.rsvpnano.app.CompanionNotice
import com.rsvpnano.app.CompanionNotice.Attention
import com.rsvpnano.app.CompanionNotice.Error
import com.rsvpnano.app.CompanionNotice.Success
import com.rsvpnano.converters.RsvpSupportedFileTypes
import com.rsvpnano.models.NanoBook
import com.rsvpnano.models.NanoSettings
import com.rsvpnano.models.PendingUpload

val CompanionLightColors = lightColorScheme(
    primary = Color(0xFF315C72),
    onPrimary = Color.White,
    primaryContainer = Color(0xFFCBE6F3),
    onPrimaryContainer = Color(0xFF071F2B),
    secondary = Color(0xFF5C5B70),
    onSecondary = Color.White,
    secondaryContainer = Color(0xFFE3DFF8),
    onSecondaryContainer = Color(0xFF1D1A2C),
    tertiary = Color(0xFF7A503B),
    tertiaryContainer = Color(0xFFFFDCCC),
    onTertiaryContainer = Color(0xFF311303),
    error = Color(0xFFBA1A1A),
    background = Color(0xFFF8F9FB),
    surface = Color(0xFFFFFFFF),
    surfaceVariant = Color(0xFFE1E5EA),
    onSurfaceVariant = Color(0xFF42474D),
)

val CompanionDarkColors = darkColorScheme(
    primary = Color(0xFF9FCFE5),
    onPrimary = Color(0xFF003545),
    primaryContainer = Color(0xFF17495E),
    onPrimaryContainer = Color(0xFFCBE6F3),
    secondary = Color(0xFFC8C3DC),
    onSecondary = Color(0xFF302D43),
    secondaryContainer = Color(0xFF47445B),
    onSecondaryContainer = Color(0xFFE4DFF8),
    tertiary = Color(0xFFE8BDA7),
    tertiaryContainer = Color(0xFF5F3826),
    onTertiaryContainer = Color(0xFFFFDCCC),
    error = Color(0xFFFFB4AB),
    background = Color(0xFF101417),
    surface = Color(0xFF171C20),
    surfaceVariant = Color(0xFF42474D),
    onSurfaceVariant = Color(0xFFC2C7CE),
)

@Composable
fun snackbarColor(notice: CompanionNotice): Color {
    return when {
        notice is Error -> MaterialTheme.colorScheme.errorContainer
        notice is Success -> MaterialTheme.colorScheme.primaryContainer
        notice is Attention -> MaterialTheme.colorScheme.secondaryContainer
        else -> SnackbarDefaults.color
    }
}

@Composable
fun snackbarContentColor(notice: CompanionNotice): Color {
    return when {
        notice is Error -> MaterialTheme.colorScheme.onErrorContainer
        notice is Success -> MaterialTheme.colorScheme.onPrimaryContainer
        notice is Attention -> MaterialTheme.colorScheme.onSecondaryContainer
        else -> SnackbarDefaults.contentColor
    }
}

@Composable
fun snackbarActionColor(notice: CompanionNotice): Color {
    return when {
        notice is Error -> MaterialTheme.colorScheme.onErrorContainer
        notice is Success -> MaterialTheme.colorScheme.primary
        notice is Attention -> MaterialTheme.colorScheme.secondary
        else -> SnackbarDefaults.actionColor
    }
}
