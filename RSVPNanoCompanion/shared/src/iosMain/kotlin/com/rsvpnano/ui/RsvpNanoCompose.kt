package com.rsvpnano.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.window.ComposeUIViewController
import com.rsvpnano.app.IosNanoWifiConnector
import com.rsvpnano.app.createIosSharedApp
import kotlinx.coroutines.MainScope
import platform.UIKit.UIViewController

fun RsvpNanoComposeViewController(): UIViewController = ComposeUIViewController {
    val presenter = remember {
        val sharedApp = createIosSharedApp()
        CompanionPresenter(
            sharedApp = sharedApp,
            nanoNetworkController = IosNanoWifiConnector(),
            settingsStore = sharedApp.appSettingsStore,
            scope = MainScope(),
        )
    }
    RsvpNanoComposeApp(presenter)
}

@Composable
private fun RsvpNanoComposeApp(presenter: CompanionPresenter) {
    val uiState by presenter.uiState.collectAsState()
    RsvpNanoSharedApp(
        uiState = uiState,
        hasPermissions = true,
        onRefresh = presenter::refresh,
        onConnect = presenter::connect,
        onShowHelp = presenter::showHelpNotice,
        onUpdateSettings = presenter::updateSettings,
        onAddressChange = presenter::setAddress,
        onConnectDefault = presenter::connectDefault,
        onWifiSsidChange = presenter::setWifiSsidDraft,
        onWifiPasswordChange = presenter::setWifiPasswordDraft,
        onSaveWifi = presenter::saveWifiSettings,
        onClearWifi = presenter::clearWifiSettings,
        onForgetRememberedNano = presenter::forgetRememberedNano,
        onGrantPermissions = presenter::requestWifiPermissions,
        needsArticleFetch = presenter::needsArticleFetch,
        onEditDraft = presenter::editDraft,
        onCancelDraftEdit = presenter::cancelDraftEdit,
        onDraftTitleChange = presenter::setDraftTitle,
        onDraftSourceChange = presenter::setDraftSourceUrl,
        onDraftBodyChange = presenter::setDraftBody,
        onSaveTextDraft = presenter::saveTextDraft,
        onSaveLinkDraft = presenter::saveLinkDraft,
        onDeleteDraft = presenter::deleteDraft,
        onSyncArticles = presenter::syncSavedArticles,
        onDeleteBook = presenter::deleteDeviceBook,
        onPickBook = presenter::uploadSelectedFile,
        onRssFeedChange = presenter::setRssFeedDraft,
        onAddRssFeed = presenter::addRssFeed,
        onRefreshRssFeeds = presenter::refreshRssFeeds,
        onDeleteFeed = presenter::deleteRssFeed,
    )
}
