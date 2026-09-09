package com.rsvpnano.ui

import androidx.compose.material3.AlertDialog
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import com.rsvpnano.presentation.DeviceDeletion

@Composable
fun DeviceDeletionDialog(deletion: DeviceDeletion?, onConfirm: () -> Unit, onDismiss: () -> Unit) {
    deletion ?: return
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(if (deletion.inUse) "In use — delete anyway?" else "Delete from reader?") },
        text = {
            Text(buildString {
                append(deletion.target.name)
                if (deletion.inUse) append(
                    when (deletion.target) {
                        is DeviceDeletion.Target.Book -> " is currently open. Deleting it will close the book."
                        is DeviceDeletion.Target.Asset -> " is currently selected. Deleting it will switch the reader to the built-in option."
                    },
                )
            })
        },
        confirmButton = {
            FilledTonalButton(onClick = onConfirm) { Text(if (deletion.inUse) "Delete anyway" else "Delete") }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancel") } },
    )
}
