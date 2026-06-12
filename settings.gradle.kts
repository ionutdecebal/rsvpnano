pluginManagement {
	repositories {
		google()
		mavenCentral()
		gradlePluginPortal()
	}
}

dependencyResolutionManagement {
	repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)

	repositories {
		google()
		mavenCentral()
	}
}

rootProject.name = "rsvpnano"

include(":shared")
include(":conversionCore")
include(":androidApp")

project(":shared").projectDir = file("RSVPNanoCompanion/shared")
project(":conversionCore").projectDir = file("RSVPNanoCompanion/conversionCore")
project(":androidApp").projectDir = file("RSVPNanoCompanion/androidApp")
