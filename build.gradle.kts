plugins {
	alias(libs.plugins.kotlin.multiplatform) apply false
	alias(libs.plugins.kotlin.serialization) apply false
	alias(libs.plugins.kotlin.android) apply false
	alias(libs.plugins.kotlin.compose) apply false
	alias(libs.plugins.jetbrains.compose) apply false
	alias(libs.plugins.android.library) apply false
	alias(libs.plugins.android.application) apply false
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

tasks.register("checkWeb") {
	group = "verification"
	description = "Runs web converter checks and publishes the generated JavaScript bundle."

	dependsOn(
		":conversionCore:jsNodeTest",
		":conversionCore:publishWebConverterJs",
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
