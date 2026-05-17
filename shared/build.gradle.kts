plugins {
	kotlin("multiplatform") version "2.0.21"
	kotlin("plugin.serialization") version "2.0.21"
	id("com.android.library") version "8.5.2"
}

kotlin {
	androidTarget()

	iosX64()
	iosArm64()
	iosSimulatorArm64()

	jvmToolchain(17)

	sourceSets {

		commonMain.dependencies {
			implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.8.1")

			implementation("org.jetbrains.kotlinx:kotlinx-serialization-core:1.7.3")
			implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.7.3")

			implementation("io.ktor:ktor-client-core:2.3.12")
			implementation("io.ktor:ktor-client-content-negotiation:2.3.12")
			implementation("io.ktor:ktor-serialization-kotlinx-json:2.3.12")
		}

		commonTest.dependencies {
			implementation(kotlin("test"))
		}

		androidMain.dependencies {
			implementation("io.ktor:ktor-client-okhttp:2.3.12")
		}

		iosMain.dependencies {
			implementation("io.ktor:ktor-client-darwin:2.3.12")
		}
	}

	targets.withType<org.jetbrains.kotlin.gradle.plugin.mpp.KotlinNativeTarget>().configureEach {
		binaries.framework {
			baseName = "shared"
			isStatic = true
		}
	}
}

android {
	namespace = "com.rsvpnano.shared"

	compileSdk = 34

	defaultConfig {
		minSdk = 24
	}

	compileOptions {
		sourceCompatibility = JavaVersion.VERSION_17
		targetCompatibility = JavaVersion.VERSION_17
	}
}