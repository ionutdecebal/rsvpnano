pluginManagement {
	repositories {
		google()
		mavenCentral()
		gradlePluginPortal()
	}
}

dependencyResolutionManagement {
	repositoriesMode.set(RepositoriesMode.PREFER_SETTINGS)

	repositories {
		google()
		mavenCentral()
		ivy("https://nodejs.org/dist") {
			name = "Node.js"
			patternLayout {
				artifact("v[revision]/[artifact]-v[revision]-[classifier].[ext]")
			}
			metadataSources {
				artifact()
			}
			content {
				includeModule("org.nodejs", "node")
			}
		}
		ivy("https://github.com/yarnpkg/yarn/releases/download") {
			name = "Yarn"
			patternLayout {
				artifact("v[revision]/[artifact]-v[revision].[ext]")
			}
			metadataSources {
				artifact()
			}
			content {
				includeModule("com.yarnpkg", "yarn")
			}
		}
	}
}

rootProject.name = "rsvpnano"

include(":shared")
include(":conversionCore")
include(":androidApp")

project(":shared").projectDir = file("RSVPNanoCompanion/shared")
project(":conversionCore").projectDir = file("RSVPNanoCompanion/conversionCore")
project(":androidApp").projectDir = file("RSVPNanoCompanion/androidApp")
