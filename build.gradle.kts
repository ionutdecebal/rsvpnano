import org.jetbrains.kotlin.gradle.targets.wasm.yarn.WasmYarnPlugin
import org.jetbrains.kotlin.gradle.targets.wasm.yarn.WasmYarnRootExtension

plugins {
	alias(libs.plugins.kotlin.multiplatform) apply false
	alias(libs.plugins.kotlin.serialization) apply false
	alias(libs.plugins.kotlin.android) apply false
	alias(libs.plugins.kotlin.compose) apply false
	alias(libs.plugins.jetbrains.compose) apply false
	alias(libs.plugins.android.library) apply false
	alias(libs.plugins.android.application) apply false
}

plugins.withType<WasmYarnPlugin> {
	the<WasmYarnRootExtension>().reportNewYarnLock = true
}

tasks.register("checkAndroid") {
	group = "verification"
	description = "Runs Android companion checks and release assembly."

	dependsOn(
		":conversionCore:testReleaseUnitTest",
		":shared:testReleaseUnitTest",
		":androidApp:assembleRelease",
	)
}

val assembleWebSite by tasks.registering(Sync::class) {
	group = "distribution"
	description = "Stages the Compose/Wasm application and firmware for GitHub Pages."
	val siteDirectory = layout.buildDirectory.dir("webSite")

	dependsOn(":webApp:wasmJsBrowserDistribution")
	from(project(":webApp").layout.buildDirectory.dir("dist/wasmJs/productionExecutable"))
	from(layout.buildDirectory.dir("firmware")) {
		into("firmware")
	}
	into(siteDirectory)
	doLast {
		val site = siteDirectory.get().asFile
		val assets = site.walkTopDown()
			.filter { it.isFile && !it.relativeTo(site).invariantSeparatorsPath.startsWith("firmware/") }
			.map { "./${it.relativeTo(site).invariantSeparatorsPath}" }
			.filterNot {
				it == "./asset-manifest.json" ||
					it.endsWith(".map") ||
					it.endsWith(".LICENSE.txt")
			}
			.sorted()
			.joinToString(prefix = "[\n  \"", separator = "\",\n  \"", postfix = "\"\n]\n")
		site.resolve("asset-manifest.json").writeText(assets)
	}
}

tasks.register("checkWeb") {
	group = "verification"
	description = "Runs Wasm conversion and browser checks, then stages the Pages site."

	dependsOn(
		":conversionCore:wasmJsBrowserTest",
		":webApp:wasmJsBrowserTest",
		assembleWebSite,
	)
}

tasks.register("checkIos") {
	group = "verification"
	description = "Runs iOS companion checks for CI and device framework builds."

	dependsOn(
		":conversionCore:compileKotlinIosArm64",
		":conversionCore:iosSimulatorArm64Test",
		":shared:compileKotlinIosArm64",
	)
}
