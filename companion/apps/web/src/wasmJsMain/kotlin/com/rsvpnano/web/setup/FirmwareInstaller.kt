@file:OptIn(ExperimentalWasmJsInterop::class)
@file:JsModule("./flasher.js")

package com.rsvpnano.web.setup

internal external fun launchInlineEspInstaller(
    manifestJson: String,
    firmwareUrl: String,
    eraseFirst: Boolean,
    onState: (String, Int) -> Unit,
    onFinished: () -> Unit,
    onError: (String) -> Unit,
)
