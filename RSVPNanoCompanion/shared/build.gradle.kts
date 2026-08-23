import org.jetbrains.kotlin.gradle.ExperimentalKotlinGradlePluginApi
import org.jetbrains.kotlin.gradle.ExperimentalWasmDsl

plugins {
	alias(libs.plugins.kotlin.multiplatform)
	alias(libs.plugins.kotlin.serialization)
	alias(libs.plugins.kotlin.compose)
	alias(libs.plugins.jetbrains.compose)
	alias(libs.plugins.android.library)
}

@OptIn(ExperimentalKotlinGradlePluginApi::class)
kotlin {
	compilerOptions {
		freeCompilerArgs.add("-Xexpect-actual-classes")
	}

	androidTarget()

	listOf(iosArm64(), iosSimulatorArm64()).forEach { iosTarget ->
		iosTarget.binaries.framework {
			baseName = "shared"
			isStatic = true
		}
	}

	@OptIn(ExperimentalWasmDsl::class)
	wasmJs {
		browser()
	}

	jvmToolchain(17)

	sourceSets {

		commonMain.dependencies {
			implementation(libs.kotlinx.coroutines.core)

			implementation(libs.kotlinx.serialization.core)
			implementation(libs.kotlinx.serialization.json)

			implementation(libs.kotlinx.datetime)
			implementation(libs.okio)

			implementation(libs.ktor.client.core)
			implementation(libs.ktor.client.content.negotiation)
			implementation(libs.ktor.serialization.kotlinx.json)

			implementation(libs.compose.runtime)
			implementation(libs.compose.foundation)
			implementation(libs.compose.material3)
			implementation(libs.compose.material.icons.extended)
			implementation(libs.compose.components.resources)
			implementation(libs.navigationevent.compose)
			implementation(libs.filekit.dialogs.compose)

			api(project(":conversionCore"))
		}

		commonTest.dependencies {
			implementation(kotlin("test"))
		}

		androidMain.dependencies {
			implementation(libs.ktor.client.okhttp)
		}

		androidUnitTest.dependencies {
			implementation(libs.ktor.client.mock)
		}

		iosMain.dependencies {
			implementation(libs.ktor.client.darwin)
		}

		wasmJsMain.dependencies {
			implementation(libs.ktor.client.js)
		}
	}

}

android {
	namespace = "com.rsvpnano.shared"

	compileSdk = 36

	defaultConfig {
		minSdk = 24
	}

	compileOptions {
		sourceCompatibility = JavaVersion.VERSION_17
		targetCompatibility = JavaVersion.VERSION_17
	}
}
