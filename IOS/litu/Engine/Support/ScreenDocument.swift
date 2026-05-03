import Foundation

struct ScreenDocument: Decodable, Sendable {
    let schema: String
    let screenId: String
    let title: String?
    let theme: ThemeTokens?
    let state: StateDocument?
    let root: UINode
    let meta: MetaDocument?
}

struct ThemeTokens: Decodable, Sendable {
    let spacing: Double?
    let cornerRadius: Double?
    let colors: [String: String]?
    let fonts: [String: String]?
}

struct StateDocument: Decodable, Sendable {
    let vars: [String: DynamicValue]
}

struct MetaDocument: Decodable, Sendable {
    let version: Int?
    let author: String?
    let tags: [String]?
}
