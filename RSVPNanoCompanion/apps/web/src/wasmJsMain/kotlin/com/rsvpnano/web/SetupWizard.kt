@file:OptIn(ExperimentalWasmJsInterop::class)

package com.rsvpnano.web

import androidx.compose.foundation.border
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsHoveredAsState
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.ExperimentalAnimationApi
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.ShoppingCart
import androidx.compose.material3.Button
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.compositeOver
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.rsvpnano.app.NanoConnectionTransport
import com.rsvpnano.ui.CompanionPresenter
import com.rsvpnano.ui.CompanionUiState
import io.github.vinceglb.filekit.BrowserFile
import io.github.vinceglb.filekit.WebFile
import io.github.vinceglb.filekit.dialogs.FileKitType
import io.github.vinceglb.filekit.dialogs.compose.rememberFilePickerLauncher
import io.github.vinceglb.filekit.name
import kotlinx.browser.window
import kotlinx.coroutines.delay
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.put
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

private data class InstallerBoard(
    val id: String,
    val name: String,
    val badge: String,
    val note: String,
    val storeUrl: String,
    val chipFamily: String,
    val bootloaderHelp: String,
)

private const val BootloaderWithReset = "Hold BOOT, tap RESET, then release BOOT."
private const val BootloaderWithPower = "Turn the Nano off, hold BOOT while turning it on, then release BOOT."

private val InstallerBoards = listOf(
    InstallerBoard("lcd349-v1", "LCD 3.49 / rev1", "RECOMMENDED", "For most 3.49-inch readers.", "https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm?&aff_id=ionutdecebal", "ESP32-S3", BootloaderWithReset),
    InstallerBoard("lcd349-v2", "LCD 3.49 / rev2", "REVISION 2", "Try this if rev1 brightness does not work.", "https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm?&aff_id=ionutdecebal", "ESP32-S3", BootloaderWithReset),
    InstallerBoard("amoled18-v1", "AMOLED 1.8 / V1", "VERSION 1", "The original 1.8-inch board.", "https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm?&aff_id=ionutdecebal", "ESP32-S3", BootloaderWithPower),
    InstallerBoard("amoled18-v2", "AMOLED 1.8 / V2", "VERSION 2", "The newer revision; still being tested.", "https://www.waveshare.com/esp32-s3-touch-amoled-1.8.htm?&aff_id=ionutdecebal", "ESP32-S3", BootloaderWithPower),
    InstallerBoard("amoled206", "AMOLED 2.06", "AMOLED", "The 2.06-inch touch board.", "https://www.waveshare.com/esp32-s3-touch-amoled-2.06.htm?&aff_id=ionutdecebal", "ESP32-S3", BootloaderWithPower),
    InstallerBoard("amoled216", "AMOLED 2.16", "3 BUTTON", "The three-button 2.16-inch board.", "https://www.waveshare.com/esp32-s3-touch-amoled-2.16.htm?&aff_id=ionutdecebal", "ESP32-S3", BootloaderWithPower),
    InstallerBoard("amoled241", "AMOLED 2.41", "AMOLED", "The 2.41-inch touch board.", "https://www.waveshare.com/esp32-s3-touch-amoled-2.41.htm?&aff_id=ionutdecebal", "ESP32-S3", BootloaderWithReset),
    InstallerBoard("lcd147-c6", "LCD 1.47 / C6", "COMPACT", "The compact 1.47-inch touch board.", "https://www.waveshare.com/esp32-c6-touch-lcd-1.47.htm?&aff_id=ionutdecebal", "ESP32-C6", BootloaderWithReset),
)

internal enum class FirmwareFilenameMatch { Match, DifferentBoard, Unknown, Ota }

private data class SelectedFirmware(val name: String, val file: BrowserFile)
private data class DeployedRelease(val version: String, val firmware: Map<String, String>)

@OptIn(ExperimentalAnimationApi::class)
@Composable
internal fun SetupWizard(presenter: CompanionPresenter, state: CompanionUiState, modifier: Modifier = Modifier) {
    var step by remember {
        mutableIntStateOf(window.localStorage.getItem("rsvpnano.web.setupStep")?.toIntOrNull()?.coerceIn(0, 4) ?: 0)
    }
    var boardId by remember {
        mutableStateOf(window.localStorage.getItem("rsvpnano.web.board") ?: InstallerBoards.first().id)
    }
    var selectedFirmware by remember { mutableStateOf<SelectedFirmware?>(null) }
    var deployedRelease by remember { mutableStateOf<DeployedRelease?>(null) }
    var releaseError by remember { mutableStateOf<String?>(null) }
    val board = InstallerBoards.firstOrNull { it.id == boardId } ?: InstallerBoards.first()
    val serialAvailable = supportsWebSerial()
    val secure = isSecureContext()
    val installerReady = state.connectionState.transport != NanoConnectionTransport.Usb
    val firmwarePicker = rememberFilePickerLauncher(FileKitType.File(listOf("bin"))) { file ->
        val browserFile = (file?.webFile as? WebFile.FileWrapper)?.file
        selectedFirmware = if (file != null && browserFile != null) SelectedFirmware(file.name, browserFile) else null
    }

    androidx.compose.runtime.LaunchedEffect(Unit) {
        runCatching { fetchDeployedRelease() }
            .onSuccess { deployedRelease = it }
            .onFailure { releaseError = "The latest firmware is not available right now. You can still choose a firmware file." }
    }

    fun advance(next: Int) {
        step = next.coerceIn(0, 4)
        window.localStorage.setItem("rsvpnano.web.setupStep", step.toString())
    }

    androidx.compose.runtime.LaunchedEffect(step, serialAvailable, state.isConnected) {
        if (step == 2 && serialAvailable && !state.isConnected) {
            repeat(10) {
                if (reconnectAuthorizedUsb(presenter)) return@LaunchedEffect
                delay(3_000)
            }
        }
    }

    androidx.compose.runtime.LaunchedEffect(step, state.connectionState.transport) {
        if (step == 1 && state.connectionState.transport == NanoConnectionTransport.Usb) {
            BrowserSerial.releaseForInstaller(presenter)
        }
    }

    val stagedFirmware = deployedRelease?.firmware?.get(board.id)
    BoxWithConstraints(modifier.fillMaxSize()) {
        val narrow = maxWidth < 760.dp
        Column(
            Modifier.align(Alignment.TopCenter).fillMaxSize().widthIn(max = 1120.dp),
        ) {
            val frameShape = RoundedCornerShape(28.dp)
            val frameBase = MaterialTheme.colorScheme.surface
            Column(
                Modifier.fillMaxWidth().weight(1f).clip(frameShape)
                    .background(
                        Brush.linearGradient(
                            listOf(
                                frameBase,
                                MaterialTheme.colorScheme.primary.copy(alpha = 0.08f).compositeOver(frameBase),
                                MaterialTheme.colorScheme.tertiary.copy(alpha = 0.05f).compositeOver(frameBase),
                            ),
                        ),
                    )
                    .border(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.24f), frameShape),
            ) {
                if (narrow) {
                    WizardSteps(
                        step,
                        state.isConnected,
                        ::advance,
                        Modifier.padding(horizontal = 14.dp, vertical = 10.dp),
                    )
                    Box(Modifier.fillMaxWidth().weight(1f)) {
                        WizardStage(
                            step = step,
                            board = board,
                            state = state,
                            secure = secure,
                            serialAvailable = serialAvailable,
                            installerReady = installerReady,
                            stagedFirmware = stagedFirmware,
                            deployedRelease = deployedRelease,
                            releaseError = releaseError,
                            selectedFirmware = selectedFirmware,
                            onBoardSelected = { candidate ->
                                boardId = candidate.id
                                window.localStorage.setItem("rsvpnano.web.board", candidate.id)
                            },
                            onInstallLatest = {
                                stagedFirmware?.let { filename ->
                                    launchFirmware(board, deployedRelease?.version.orEmpty(), absoluteUrl("firmware/$filename"))
                                }
                            },
                            onChooseFile = { firmwarePicker.launch() },
                            onInstallFile = { firmware -> launchLocalFirmware(board, firmware) },
                            onUsbConnect = { requestUsbConnection(presenter) },
                            onStep = ::advance,
                            compact = true,
                            modifier = Modifier.fillMaxSize(),
                        )
                    }
                } else {
                    Row(Modifier.fillMaxSize()) {
                        Column(Modifier.weight(1f).fillMaxHeight()) {
                            WizardSteps(
                                step,
                                state.isConnected,
                                ::advance,
                                Modifier.padding(horizontal = 14.dp, vertical = 10.dp),
                            )
                            WizardStage(
                                step = step,
                                board = board,
                                state = state,
                                secure = secure,
                                serialAvailable = serialAvailable,
                                installerReady = installerReady,
                                stagedFirmware = stagedFirmware,
                                deployedRelease = deployedRelease,
                                releaseError = releaseError,
                                selectedFirmware = selectedFirmware,
                                onBoardSelected = { candidate ->
                                    boardId = candidate.id
                                    window.localStorage.setItem("rsvpnano.web.board", candidate.id)
                                },
                                onInstallLatest = {
                                    stagedFirmware?.let { filename ->
                                        launchFirmware(board, deployedRelease?.version.orEmpty(), absoluteUrl("firmware/$filename"))
                                    }
                                },
                                onChooseFile = { firmwarePicker.launch() },
                                onInstallFile = { firmware -> launchLocalFirmware(board, firmware) },
                                onUsbConnect = { requestUsbConnection(presenter) },
                                onStep = ::advance,
                                modifier = Modifier.weight(1f).fillMaxWidth(),
                            )
                        }
                        ReactionPanel(
                            step,
                            board,
                            state,
                            modifier = Modifier.widthIn(min = 280.dp, max = 330.dp).fillMaxHeight(),
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun WizardSteps(step: Int, connected: Boolean, onStep: (Int) -> Unit, modifier: Modifier = Modifier) {
    Row(
        modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(6.dp, Alignment.CenterHorizontally),
    ) {
        listOf("Choose device", "Install", "Connect", "Personalize", "Ready").forEachIndexed { index, label ->
            val selected = index == step
            val shape = RoundedCornerShape(50)
            val background by animateColorAsState(
                if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surface,
                animationSpec = tween(180),
            )
            Box(
                Modifier.clip(shape)
                    .background(background)
                    .border(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.28f), shape)
                    .clickable(enabled = index <= step || connected) { onStep(index) }
                    .padding(horizontal = 13.dp, vertical = 7.dp),
                contentAlignment = Alignment.Center,
            ) {
                Text(
                    label,
                    color = if (selected) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurface,
                    fontWeight = if (selected) FontWeight.Bold else FontWeight.Medium,
                    style = MaterialTheme.typography.labelMedium,
                )
            }
        }
    }
}

@OptIn(ExperimentalAnimationApi::class)
@Composable
private fun WizardStage(
    step: Int,
    board: InstallerBoard,
    state: CompanionUiState,
    secure: Boolean,
    serialAvailable: Boolean,
    installerReady: Boolean,
    stagedFirmware: String?,
    deployedRelease: DeployedRelease?,
    releaseError: String?,
    selectedFirmware: SelectedFirmware?,
    onBoardSelected: (InstallerBoard) -> Unit,
    onInstallLatest: () -> Unit,
    onChooseFile: () -> Unit,
    onInstallFile: (SelectedFirmware) -> Unit,
    onUsbConnect: () -> Unit,
    onStep: (Int) -> Unit,
    compact: Boolean = false,
    modifier: Modifier = Modifier,
) {
    Column(
        modifier.padding(
            horizontal = if (compact) 18.dp else 30.dp,
            vertical = if (compact) 14.dp else 26.dp,
        ),
    ) {
        AnimatedContent(
            targetState = step,
            modifier = Modifier.fillMaxWidth().weight(1f),
            transitionSpec = {
                val direction = if (targetState > initialState) 1 else -1
                (slideInHorizontally(tween(300, easing = FastOutSlowInEasing)) { direction * it / 3 } +
                    fadeIn(tween(220)) + scaleIn(tween(280), initialScale = 0.985f)) togetherWith
                    (slideOutHorizontally(tween(240, easing = FastOutSlowInEasing)) { -direction * it / 4 } +
                        fadeOut(tween(170)))
            },
            contentAlignment = Alignment.TopStart,
            label = "setup step",
        ) { activeStep ->
            Box(Modifier.fillMaxWidth()) {
                when (activeStep) {
                    0 -> ChooseBoardPage(board, onBoardSelected)
                    1 -> InstallPage(
                        board = board,
                        secure = secure,
                        serialAvailable = serialAvailable,
                        installerReady = installerReady,
                        stagedFirmware = stagedFirmware,
                        deployedRelease = deployedRelease,
                        releaseError = releaseError,
                        selectedFirmware = selectedFirmware,
                        onInstallLatest = onInstallLatest,
                        onChooseFile = onChooseFile,
                        onInstallFile = onInstallFile,
                    )
                    2 -> ConnectPage(state, secure, serialAvailable, onUsbConnect)
                    3 -> PersonalizePage()
                    else -> ReadyPage()
                }
            }
        }
        WizardFooter(step, board, state.isConnected, compact, onStep)
    }
}

@Composable
private fun ChooseBoardPage(board: InstallerBoard, onBoardSelected: (InstallerBoard) -> Unit) {
    Column(
        Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(bottom = 18.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        WizardIntro("YOUR READER", "Which Nano do you have?", "The installer uses this to choose the right firmware and display settings.")
        BoardGrid(board.id, onBoardSelected)
        Text(
            "Store links are affiliate links. A purchase may support RSVP Nano at no extra cost to you.",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.62f),
        )
    }
}

@Composable
private fun InstallPage(
    board: InstallerBoard,
    secure: Boolean,
    serialAvailable: Boolean,
    installerReady: Boolean,
    stagedFirmware: String?,
    deployedRelease: DeployedRelease?,
    releaseError: String?,
    selectedFirmware: SelectedFirmware?,
    onInstallLatest: () -> Unit,
    onChooseFile: () -> Unit,
    onInstallFile: (SelectedFirmware) -> Unit,
) {
    Column(
        Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(bottom = 18.dp),
        verticalArrangement = Arrangement.spacedBy(20.dp),
    ) {
        WizardIntro("FIRMWARE", "Install RSVP Nano", "Plug in ${board.name}, then choose it when your browser asks.")
        Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text("Bootloader mode for ${board.name}", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.Bold)
            Text(board.bootloaderHelp, style = MaterialTheme.typography.bodyMedium)
            Text("Existing settings are kept unless you choose to erase the device.", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.66f))
        }
        if (!secure) Text("Open this page over HTTPS before installing.", color = MaterialTheme.colorScheme.error)
        if (!serialAvailable) Text("Use Chrome or Edge on a computer to install over USB.", color = MaterialTheme.colorScheme.tertiary)
        if (!installerReady) Text("Preparing USB for the installer...", color = MaterialTheme.colorScheme.tertiary)
        if (deployedRelease != null && stagedFirmware == null) {
            Text("The latest release does not include this Nano yet.", color = MaterialTheme.colorScheme.tertiary)
        } else if (releaseError != null) {
            Text(releaseError, color = MaterialTheme.colorScheme.tertiary)
        }

        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val stacked = maxWidth < 620.dp
            if (stacked) {
                Column(verticalArrangement = Arrangement.spacedBy(18.dp)) {
                    LatestInstallChoice(deployedRelease, secure && serialAvailable && installerReady && stagedFirmware != null, onInstallLatest)
                    LocalInstallChoice(selectedFirmware, board, secure, serialAvailable && installerReady, onChooseFile, onInstallFile)
                }
            } else {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(26.dp)) {
                    Column(Modifier.weight(1f)) {
                        LatestInstallChoice(deployedRelease, secure && serialAvailable && installerReady && stagedFirmware != null, onInstallLatest)
                    }
                    Column(
                        Modifier.weight(1f).border(
                            width = 1.dp,
                            color = MaterialTheme.colorScheme.outline.copy(alpha = 0.24f),
                            shape = RoundedCornerShape(18.dp),
                        ).padding(18.dp),
                    ) {
                        LocalInstallChoice(selectedFirmware, board, secure, serialAvailable && installerReady, onChooseFile, onInstallFile)
                    }
                }
            }
        }
    }
}

@Composable
private fun LatestInstallChoice(release: DeployedRelease?, enabled: Boolean, onInstall: () -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Text("Recommended", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.Bold)
        Text("Latest stable release", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Black)
        Text(release?.version ?: "Checking for the latest version…", color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.66f))
        Button(onClick = onInstall, enabled = enabled, modifier = Modifier.fillMaxWidth().height(52.dp)) {
            Text("Install latest firmware")
        }
    }
}

@Composable
private fun LocalInstallChoice(
    firmware: SelectedFirmware?,
    board: InstallerBoard,
    secure: Boolean,
    serialAvailable: Boolean,
    onChooseFile: () -> Unit,
    onInstallFile: (SelectedFirmware) -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Text("Use your own file", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
        Text("For development builds or an older release.", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.66f))
        OutlinedButton(onClick = onChooseFile, modifier = Modifier.fillMaxWidth()) { Text("Choose firmware file") }
        firmware?.let {
            val match = firmwareFilenameMatch(board.id, it.name)
            Text(it.name, fontWeight = FontWeight.Bold, style = MaterialTheme.typography.bodySmall)
            Text(
                firmwareMessage(match),
                style = MaterialTheme.typography.bodySmall,
                color = if (match == FirmwareFilenameMatch.Match) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.tertiary,
            )
            Button(
                onClick = { onInstallFile(it) },
                enabled = secure && serialAvailable && match != FirmwareFilenameMatch.Ota,
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Install selected file") }
        }
    }
}

@Composable
private fun ConnectPage(state: CompanionUiState, secure: Boolean, serialAvailable: Boolean, onUsbConnect: () -> Unit) {
    Column(
        Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(bottom = 18.dp),
        verticalArrangement = Arrangement.spacedBy(22.dp),
    ) {
        WizardIntro("CONNECTION", "Meet your Nano", "Connect once so the site can verify your reader and finish setup.")
        if (state.isConnected) {
            Text("Connected", style = MaterialTheme.typography.headlineSmall, color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.Black)
        }
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val stacked = maxWidth < 560.dp
            val usb: @Composable () -> Unit = {
                ConnectionChoice(
                    title = "USB",
                    description = "Best while the Nano is beside you.",
                    action = "Connect with USB",
                    enabled = secure && serialAvailable,
                    onClick = onUsbConnect,
                )
            }
            val network: @Composable () -> Unit = {
                ConnectionChoice(
                    title = "Local network",
                    description = "Use the connection controls at the top of the page.",
                    action = "Use network",
                    enabled = true,
                    onClick = { window.location.hash = "#/device" },
                )
            }
            if (stacked) Column(verticalArrangement = Arrangement.spacedBy(16.dp)) { usb(); network() }
            else Row(horizontalArrangement = Arrangement.spacedBy(28.dp)) {
                Column(Modifier.weight(1f)) { usb() }
                Column(Modifier.weight(1f)) { network() }
            }
        }
    }
}

@Composable
private fun ConnectionChoice(title: String, description: String, action: String, enabled: Boolean, onClick: () -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(9.dp)) {
        Text(title, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Black)
        Text(description, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.68f))
        OutlinedButton(onClick = onClick, enabled = enabled, modifier = Modifier.fillMaxWidth()) { Text(action) }
    }
}

@Composable
private fun PersonalizePage() {
    Column(
        Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(bottom = 18.dp),
        verticalArrangement = Arrangement.spacedBy(22.dp),
    ) {
        WizardIntro("OPTIONAL", "Make it yours", "You can set the reading experience now or come back whenever you like.")
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            OutlinedButton(onClick = { window.location.hash = "#/appearance/themes" }, modifier = Modifier.weight(1f).height(52.dp)) {
                Text("Appearance")
            }
            OutlinedButton(onClick = { window.location.hash = "#/settings/reading" }, modifier = Modifier.weight(1f).height(52.dp)) {
                Text("Reading")
            }
        }
    }
}

@Composable
private fun ReadyPage() {
    Column(
        Modifier.fillMaxWidth().fillMaxHeight(),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Box(Modifier.size(14.dp).clip(CircleShape).background(MaterialTheme.colorScheme.primary))
        Spacer(Modifier.height(18.dp))
        Text("Your Nano is ready", style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Black)
        Text("Add a book or keep exploring the site.", color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.68f))
    }
}

@Composable
private fun WizardIntro(eyebrow: String, title: String, description: String) {
    Column(verticalArrangement = Arrangement.spacedBy(7.dp)) {
        Text(eyebrow, style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.tertiary, fontWeight = FontWeight.Bold)
        Text(title, style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Black)
        Text(description, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f))
    }
}

@Composable
private fun WizardFooter(step: Int, board: InstallerBoard, connected: Boolean, compact: Boolean, onStep: (Int) -> Unit) {
    Row(
        Modifier.fillMaxWidth().padding(top = if (compact) 8.dp else 14.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (step in 1..4) OutlinedButton(onClick = { onStep(step - 1) }) { Text("Back") }
        else Spacer(Modifier.size(1.dp))
        when (step) {
            0 -> Button(onClick = { onStep(1) }) { Text("Continue with ${board.name}") }
            1 -> Button(onClick = { onStep(2) }) { Text("Installation finished") }
            2 -> Button(onClick = { onStep(3) }, enabled = connected) { Text("Continue") }
            3 -> Button(onClick = { onStep(4) }) { Text("Finish") }
            else -> Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = { onStep(0) }) { Text("Set up another") }
                Button(onClick = { window.location.hash = "#/library" }) { Text("Open library") }
            }
        }
    }
}

@Composable
private fun ReactionPanel(
    step: Int,
    board: InstallerBoard,
    state: CompanionUiState,
    modifier: Modifier = Modifier,
) {
    val captions = listOf(
        "Choose the hardware in front of you.",
        "Firmware brings the reader to life.",
        "Verify the Nano before moving on.",
        "Tune the reader to your habits.",
        "Everything is ready for your library.",
    )
    val panelBase = MaterialTheme.colorScheme.surface
    Box(
        modifier.background(
            Brush.linearGradient(
                listOf(
                    MaterialTheme.colorScheme.primary.copy(alpha = 0.24f).compositeOver(panelBase),
                    panelBase,
                    MaterialTheme.colorScheme.tertiary.copy(alpha = 0.2f).compositeOver(panelBase),
                ),
            ),
        ),
    ) {
        ReactionBackdrop(Modifier.matchParentSize())
        Box(
            Modifier.matchParentSize().background(
                Brush.verticalGradient(
                    listOf(Color.Transparent, panelBase.copy(alpha = 0.9f)),
                    startY = 40f,
                ),
            ),
        )
        Column(
            Modifier.align(Alignment.BottomStart).padding(22.dp),
            verticalArrangement = Arrangement.spacedBy(5.dp),
        ) {
            Text("REACTION", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.tertiary, fontWeight = FontWeight.Black)
            Text(
                captions[step],
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold,
            )
            Text(
                if (state.isConnected) "Nano connected" else board.name,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.66f),
            )
        }
    }
}

@Composable
private fun BoardGrid(selectedId: String, onSelect: (InstallerBoard) -> Unit) {
    BoxWithConstraints(Modifier.fillMaxWidth()) {
        val columns = when {
            maxWidth >= 600.dp -> 4
            maxWidth >= 500.dp -> 3
            maxWidth >= 340.dp -> 2
            else -> 1
        }
        Column(verticalArrangement = Arrangement.spacedBy(9.dp)) {
            InstallerBoards.chunked(columns).forEach { boards ->
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(9.dp)) {
                    boards.forEach { board ->
                        BoardTile(board, board.id == selectedId, { onSelect(board) }, Modifier.weight(1f))
                    }
                    repeat(columns - boards.size) { Spacer(Modifier.weight(1f)) }
                }
            }
        }
    }
}

@Composable
private fun BoardTile(board: InstallerBoard, selected: Boolean, onSelect: () -> Unit, modifier: Modifier = Modifier) {
    val shape = RoundedCornerShape(16.dp)
    val interactions = remember { MutableInteractionSource() }
    val hovered by interactions.collectIsHoveredAsState()
    val pressed by interactions.collectIsPressedAsState()
    val scale by animateFloatAsState(
        targetValue = when {
            pressed -> 0.985f
            hovered -> 1.012f
            else -> 1f
        },
        animationSpec = tween(120),
    )
    val border = if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.28f)
    val background by animateColorAsState(
        if (selected) MaterialTheme.colorScheme.primary.copy(alpha = 0.13f) else MaterialTheme.colorScheme.surface,
        animationSpec = tween(180),
    )
    Surface(
        modifier.graphicsLayer(scaleX = scale, scaleY = scale).clip(shape)
            .border(if (selected) 2.dp else 1.dp, border, shape)
            .clickable(interactionSource = interactions, indication = null, onClick = onSelect),
        color = background,
        shape = shape,
    ) {
        Column(Modifier.fillMaxWidth().height(122.dp).padding(11.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.Top) {
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text(board.name, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
                    Text(board.badge, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.tertiary)
                }
                IconButton(onClick = { window.open(board.storeUrl, "_blank") }, modifier = Modifier.size(36.dp)) {
                    Icon(Icons.Outlined.ShoppingCart, "Open store page", Modifier.size(19.dp))
                }
            }
            Text(board.note, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.72f))
        }
    }
}

internal fun firmwareFilenameMatch(selectedBoardId: String, filename: String): FirmwareFilenameMatch {
    val normalized = filename.lowercase()
    if (normalized.endsWith("-ota.bin")) return FirmwareFilenameMatch.Ota

    val fileBoardId = when {
        "touch-lcd-3.49-rev2" in normalized || normalized == "rsvp-nano-rev2.bin" -> "lcd349-v2"
        "touch-lcd-3.49" in normalized || normalized == "rsvp-nano.bin" -> "lcd349-v1"
        "touch-amoled-1.8-v2" in normalized -> "amoled18-v2"
        "touch-amoled-1.8" in normalized -> "amoled18-v1"
        "touch-amoled-2.06" in normalized -> "amoled206"
        "touch-amoled-2.16" in normalized -> "amoled216"
        "touch-amoled-2.41" in normalized -> "amoled241"
        "esp32-c6-touch-lcd-1.47" in normalized -> "lcd147-c6"
        else -> null
    }
    return when (fileBoardId) {
        null -> FirmwareFilenameMatch.Unknown
        selectedBoardId -> FirmwareFilenameMatch.Match
        else -> FirmwareFilenameMatch.DifferentBoard
    }
}

private fun firmwareMessage(match: FirmwareFilenameMatch): String = when (match) {
    FirmwareFilenameMatch.Match -> "This filename matches the Nano you selected."
    FirmwareFilenameMatch.DifferentBoard -> "This filename appears to be for a different Nano. Check the board before continuing."
    FirmwareFilenameMatch.Unknown -> "This is not a recognized release filename. Only continue if you trust this custom firmware."
    FirmwareFilenameMatch.Ota -> "This appears to be an update-only file. Choose the full installer file without “-ota” in its name."
}

private fun launchLocalFirmware(board: InstallerBoard, firmware: SelectedFirmware) {
    val firmwareUrl = createObjectUrl(firmware.file)
    launchFirmware(board, "custom", firmwareUrl)
}

private fun launchFirmware(board: InstallerBoard, version: String, firmwareUrl: String) {
    val manifest = buildJsonObject {
        put("name", "RSVP Nano ${board.name}")
        put("version", version)
        put("new_install_prompt_erase", true)
        put("new_install_improv_wait_time", 30)
        put("builds", buildJsonArray {
            add(buildJsonObject {
                put("chipFamily", board.chipFamily)
                put("improv", true)
                put("parts", buildJsonArray {
                    for (offset in listOf(0, 0x8000, 0xE000, 0x10000)) {
                        add(buildJsonObject {
                            put("path", firmwareUrl)
                            put("offset", offset)
                        })
                    }
                })
            })
        })
    }.toString()
    launchDynamicEspInstaller(manifest, firmwareUrl)
}

private suspend fun fetchDeployedRelease(): DeployedRelease = suspendCancellableCoroutine { continuation ->
    fetchDeployedReleaseJson(
        { body ->
            runCatching {
                val root = Json.parseToJsonElement(body).jsonObject
                val version = root.getValue("version").jsonPrimitive.content
                val firmware = root.getValue("firmware").jsonObject.mapValues { (_, value) ->
                    value.jsonPrimitive.content
                }
                DeployedRelease(version, firmware)
            }.onSuccess(continuation::resume)
                .onFailure(continuation::resumeWithException)
        },
        { message -> continuation.resumeWithException(IllegalStateException(message)) },
    )
}

@JsFun("() => window.isSecureContext")
private external fun isSecureContext(): Boolean

@JsFun("() => window.isSecureContext && ('serial' in navigator)")
internal external fun supportsWebSerial(): Boolean

@JsFun("(file) => URL.createObjectURL(file)")
private external fun createObjectUrl(file: BrowserFile): String

@JsFun("(path) => new URL(path, document.baseURI).href")
private external fun absoluteUrl(path: String): String

@JsFun("(ok, fail) => fetch(new URL('firmware/release.json', document.baseURI), { cache: 'no-store' }).then(response => { if (!response.ok) throw new Error('HTTP ' + response.status); return response.text(); }).then(ok).catch(error => fail(error?.message || String(error)))")
private external fun fetchDeployedReleaseJson(ok: (String) -> Unit, fail: (String) -> Unit)

@JsFun("(manifestJson, firmwareUrl) => { (async () => { const response = await fetch(firmwareUrl); if (!response.ok) throw new Error('Could not load the firmware file'); const firmware = await response.blob(); if (firmwareUrl.startsWith('blob:')) URL.revokeObjectURL(firmwareUrl); if (firmware.size <= 65536) throw new Error('The full installer firmware is incomplete'); const manifest = JSON.parse(manifestJson); const ranges = new Map([[0, [0, 32768]], [32768, [32768, 36864]], [57344, [57344, 65536]], [65536, [65536, firmware.size]]]); if (globalThis.rsvpNanoManifestUrl) URL.revokeObjectURL(globalThis.rsvpNanoManifestUrl); for (const url of globalThis.rsvpNanoPartUrls || []) URL.revokeObjectURL(url); const partUrls = []; for (const part of manifest.builds[0].parts) { const range = ranges.get(part.offset); if (!range) throw new Error('Unsupported firmware offset'); const url = URL.createObjectURL(firmware.slice(range[0], range[1])); part.path = url; partUrls.push(url); } const manifestUrl = URL.createObjectURL(new Blob([JSON.stringify(manifest)], { type: 'application/json' })); globalThis.rsvpNanoManifestUrl = manifestUrl; globalThis.rsvpNanoPartUrls = partUrls; const host = document.getElementById('rsvp-installer'); host.overrides = { checkSameFirmware: (_manifest, info) => info?.name === 'RSVP Nano' }; host.manifest = manifestUrl; document.getElementById('rsvp-installer-trigger').click(); })().catch(error => window.alert(error?.message || String(error))); }")
private external fun launchDynamicEspInstaller(manifestJson: String, firmwareUrl: String)
