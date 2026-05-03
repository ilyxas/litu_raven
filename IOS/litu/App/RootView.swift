import SwiftUI
import UniformTypeIdentifiers


struct RootView: View {
    
    private enum Section: String, CaseIterable, Identifiable {
        case soccerPenalty = "Penalty"
        case localLLM = "Local LLM"
        case networking = "TCP Networking"
        

        var id: Self { self }

        var icon: String {
            switch self {
            case .soccerPenalty: return "soccerball"
            case .localLLM: return "message"
            case .networking: return "network"
            }
        }
    }

    @State private var selection: Section = .networking
    @State private var isImporting = false
    @State private var importError: String? = nil
    @State private var llmEvaluator = LLMEvaluator()
    @State private var chatModel = ChatModel()
    @State private var deviceStat = DeviceStat()
    @State private var penaltyPlanner: SoccerPenaltyPlannerService = {
        let evaluator = LLMEvaluator()
        let chatModel = ChatModel()
        return SoccerPenaltyPlannerService(llm: evaluator, chatModel: chatModel)
    }()
    
    @State private var tcpNetworking:  TCPNetworking = {
        let client = SimpleTCPClient()
        let server =  SimpleTCPServer()
        let networking = TCPNetworking(client: client, server: server)
        client.networking = networking
        server.networking = networking
        return networking
    }()
    
    var body: some View {
        NavigationStack {
            Group {
                switch selection {
                case .localLLM:
                    LocalLLMView(llm: llmEvaluator, chatModel: chatModel, deviceStat: deviceStat)
                case .soccerPenalty:
                    SoccerPenaltyView(planner: penaltyPlanner)
                case .networking:
                    TCPNetworkingView(tcpNetworking: tcpNetworking)
                }
            }
            .toolbar {
                ToolbarItemGroup(placement: .topBarLeading) {
                    Menu {
                            Button {
                                selection = .localLLM
                            } label: {
                                Label("Local LLM", systemImage: "message")
                            }
                            Button {
                                selection = .soccerPenalty
                            } label: {
                                Label("Penalty", systemImage: "soccerball")
                            }
                            Button {
                                selection = .networking
                            } label: {
                                Label("TCP Networking", systemImage: "network")
                            }
                        } label: {
                            Image(systemName: "square.grid.2x2")
                        }
                }
            }
        }
        .fileImporter(
            isPresented: $isImporting,
            allowedContentTypes: [.json],
            allowsMultipleSelection: false
        ) { result in
            handleImport(result)
        }
        .alert("Import Error", isPresented: Binding(
            get: { importError != nil },
            set: { if !$0 { importError = nil } }
        )) {
            Button("OK", role: .cancel) {
                importError = nil
            }
        } message: {
            Text(importError ?? "")
        }
    }

//    private func loadBundledHomeFromRoot() {
//        do {
//            let doc = try JSONLoader.loadScreen(named: "home")
//            try DocumentValidator.validate(doc)
//            screenDocument = doc
//        } catch {
//            importError = error.localizedDescription
//        }
//    }

    private func handleImport(_ result: Result<[URL], Error>) {
        do {
            let urls = try result.get()
            guard let url = urls.first else { return }

            let granted = url.startAccessingSecurityScopedResource()
            defer {
                if granted { url.stopAccessingSecurityScopedResource() }
            }

            let data = try Data(contentsOf: url)
        } catch {
            importError = error.localizedDescription
        }
    }
}

