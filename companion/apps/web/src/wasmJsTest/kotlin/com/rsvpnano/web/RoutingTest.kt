package com.rsvpnano.web

import kotlin.test.Test
import kotlin.test.assertEquals

class RoutingTest {
    @Test
    fun routePrefixesOpenTheirWebWorkspace() {
        assertEquals(WebRoute.Setup, routeForHash(""))
        assertEquals(WebRoute.Device, routeForHash("#/device"))
        assertEquals(WebRoute.Appearance, routeForHash("#/appearance/fonts"))
        assertEquals(WebRoute.Settings, routeForHash("#/settings/display"))
        assertEquals(WebRoute.Timers, routeForHash("#/timers"))
        assertEquals(WebRoute.About, routeForHash("#/about"))
        assertEquals(WebRoute.Setup, routeForHash("#/unknown"))
    }

    @Test
    fun persistedThemeNamesRemainStable() {
        assertEquals(WebTheme.Light, WebTheme.valueOf("Light"))
        assertEquals(WebTheme.Dark, WebTheme.valueOf("Dark"))
    }
}
