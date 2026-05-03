import Foundation

enum JSONLoader {
    static func loadScreen(named name: String) throws -> ScreenDocument {
        guard let url = Bundle.main.url(forResource: name, withExtension: "json") else {
            throw NSError(domain: "JSONLoader", code: 1)
        }
        let data = try Data(contentsOf: url)
        return try JSONDecoder().decode(ScreenDocument.self, from: data)
    }
}
