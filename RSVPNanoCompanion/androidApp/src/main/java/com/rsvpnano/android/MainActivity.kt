package com.rsvpnano.android

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.enableEdgeToEdge
import androidx.activity.compose.setContent
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import com.rsvpnano.app.createAndroidSharedApp
import com.rsvpnano.android.ui.CompanionApp
import kotlinx.coroutines.flow.MutableStateFlow

class MainActivity : ComponentActivity() {
    private val incomingShareIntent = MutableStateFlow<Intent?>(null)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        incomingShareIntent.value = intent.takeIf { it.isAndroidShareIntent() }
        setContent {
            val sharedApp = remember { createAndroidSharedApp(filesDir) }
            val shareIntent by incomingShareIntent.collectAsState()
            CompanionApp(
                sharedApp = sharedApp,
                shareIntent = shareIntent,
                onShareIntentHandled = { clearIncomingShareIntent() },
            )
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        incomingShareIntent.value = intent.takeIf { it.isAndroidShareIntent() }
    }

    private fun clearIncomingShareIntent() {
        incomingShareIntent.value = null
        setIntent(Intent(Intent.ACTION_MAIN))
    }

    private fun Intent.isAndroidShareIntent(): Boolean {
        return action == Intent.ACTION_SEND || action == Intent.ACTION_SEND_MULTIPLE
    }
}
