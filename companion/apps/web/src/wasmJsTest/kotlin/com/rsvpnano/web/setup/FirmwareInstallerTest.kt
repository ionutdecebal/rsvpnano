@file:OptIn(ExperimentalWasmJsInterop::class)

package com.rsvpnano.web.setup

import kotlin.coroutines.resume
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.fail
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.test.runTest

class FirmwareInstallerTest {
    @Test
    fun bundledInstallerLoadsAndReachesPortSelection() = runTest {
        val originalSerial = replaceSerialPortPicker()
        val firmwareUrl = createInstallerFirmware()
        try {
            val error = suspendCancellableCoroutine<String> { continuation ->
                launchInlineEspInstaller(
                    """{"builds":[{"chipFamily":"ESP32-S3","parts":[{"offset":0},{"offset":32768},{"offset":57344},{"offset":65536}]}]}""",
                    firmwareUrl,
                    false,
                    { _, _ -> fail("Flashing must not start without a selected port") },
                    { fail("Installation must not finish without a selected port") },
                    { continuation.resume(it) },
                )
            }
            assertEquals("Installer reached port selection", error)
        } finally {
            restoreSerialPortPicker(originalSerial)
            revokeInstallerFirmware(firmwareUrl)
        }
    }
}

// Keep the real installer and its dependency graph; replace only the hardware picker.
@JsFun("""() => { const original = Object.getOwnPropertyDescriptor(navigator, 'serial'); Object.defineProperty(navigator, 'serial', { configurable: true, value: { requestPort: async () => { throw new Error('Installer reached port selection'); } } }); return original; }""")
private external fun replaceSerialPortPicker(): JsAny?

@JsFun("""original => { if (original) Object.defineProperty(navigator, 'serial', original); else delete navigator.serial; }""")
private external fun restoreSerialPortPicker(original: JsAny?)

@JsFun("() => URL.createObjectURL(new Blob([new Uint8Array(65537)]))")
private external fun createInstallerFirmware(): String

@JsFun("url => URL.revokeObjectURL(url)")
private external fun revokeInstallerFirmware(url: String)
