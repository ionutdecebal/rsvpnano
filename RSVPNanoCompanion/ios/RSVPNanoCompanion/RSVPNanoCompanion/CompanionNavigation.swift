import SwiftUI

enum CompanionPage: String, CaseIterable, Identifiable {
    case library = "Library"
    case settings = "Settings"

    var id: String { rawValue }

    var iconName: String {
        switch self {
        case .library: return "books.vertical"
        case .settings: return "slider.horizontal.3"
        }
    }
}
