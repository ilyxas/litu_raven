import Foundation

struct UINode: Decodable, Identifiable, Sendable {
    let id: String?
    let type: String
    let props: [String: DynamicValue]?
    let children: [UINode]?
    let event: [String: String]?
    let items: [ListItem]?

    var stableId: String {
        id ?? UUID().uuidString
    }
}

// MARK: - ListItem

struct ListItem: Decodable, Identifiable, Sendable {
    let id: String
    let title: String
    let event: [String: String]?
}
