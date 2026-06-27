import SwiftUI
import shared

@main
struct RSVPNanoCompanionApp: App {
    var body: some Scene {
        WindowGroup {
            ComposeCompanionRoot()
                .ignoresSafeArea(.keyboard)
        }
    }
}

private struct ComposeCompanionRoot: UIViewControllerRepresentable {
    func makeUIViewController(context: Context) -> UIViewController {
        RsvpNanoComposeKt.RsvpNanoComposeViewController()
    }

    func updateUIViewController(_ uiViewController: UIViewController, context: Context) {
    }
}
