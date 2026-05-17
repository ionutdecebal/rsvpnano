import org.gradle.api.initialization.resolve.RepositoriesMode

pluginManagement {
    repositories {
        gradlePluginPortal()
        google()
        mavenCentral()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
        ivy {
            name = "GoogleAapt2"
            url = uri("https://dl.google.com/dl/android/maven2/")
            patternLayout {
                artifact("/com/android/tools/build/aapt2/[revision]/[artifact]-[revision](-[classifier]).[ext]")
            }
            content {
                includeModule("com.android.tools.build", "aapt2")
            }
        }
    }
}

rootProject.name = "rsvpnano"
include(":shared")