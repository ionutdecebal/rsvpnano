import org.jetbrains.kotlin.gradle.ExperimentalKotlinGradlePluginApi

plugins {
	kotlin("multiplatform")
	id("com.android.library")
}

@OptIn(ExperimentalKotlinGradlePluginApi::class)
kotlin {
	compilerOptions {
		freeCompilerArgs.add("-Xexpect-actual-classes")
	}

	androidTarget()

	js(IR) {
		compilerOptions {
			outputModuleName.set("rsvpnano_converter")
		}
		useEsModules()
		browser()
		nodejs()
		binaries.executable()
	}

	listOf(
		iosX64(),
		iosArm64(),
		iosSimulatorArm64(),
	)

	jvmToolchain(17)

	sourceSets {
		commonMain.dependencies {
			implementation("com.soywiz:korlibs-compression:6.0.0")
			implementation("com.fleeksoft.ksoup:ksoup:0.2.6")
		}

		commonTest.dependencies {
			implementation(kotlin("test"))
		}
	}
}

android {
	namespace = "com.rsvpnano.conversioncore"

	compileSdk = 34

	defaultConfig {
		minSdk = 24
	}

	compileOptions {
		sourceCompatibility = JavaVersion.VERSION_17
		targetCompatibility = JavaVersion.VERSION_17
	}
}

tasks.register<Sync>("publishWebConverterJs") {
	dependsOn("compileProductionExecutableKotlinJs")
	from(layout.buildDirectory.dir("compileSync/js/main/productionExecutable/kotlin")) {
		include("*.mjs")
		include("*.mjs.map")
	}
	into(rootProject.layout.projectDirectory.dir("RSVPNanoCompanion/web/generated/converter"))
}
