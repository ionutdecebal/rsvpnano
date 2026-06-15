import org.jetbrains.kotlin.gradle.ExperimentalKotlinGradlePluginApi

plugins {
	alias(libs.plugins.kotlin.multiplatform)
	alias(libs.plugins.android.library)
}

@OptIn(ExperimentalKotlinGradlePluginApi::class)
kotlin {
	compilerOptions {
		freeCompilerArgs.add("-Xexpect-actual-classes")
	}

	androidTarget()

	js {
		compilerOptions {
			outputModuleName.set("rsvpnano_converter")
		}
		useEsModules()
		browser()
		nodejs()
		binaries.executable()
	}

	iosArm64()
	iosSimulatorArm64()

	jvmToolchain(17)

	sourceSets {
		commonMain.dependencies {
			implementation(libs.korlibs.compression)
			implementation(libs.ksoup)
		}

		commonTest.dependencies {
			implementation(kotlin("test"))
		}
	}
}

android {
	namespace = "com.rsvpnano.conversioncore"

	compileSdk = 36

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
	into(rootProject.layout.projectDirectory.dir("web/generated/converter"))
}
