// Copyright © 2025 Apple Inc.

import Hub
import MLX
import MLXLLM
import MLXLMCommon
import Metal
import SwiftUI
import MLXVLM

@Observable
@MainActor
class LLMEvaluator {
    
    public let availableModels: [LMModel] = [
        LMModel(name: "llama3_8B_4bit", configuration: LLMRegistry.llama3_8B_4bit, type: .llm),
        LMModel(name: "deepSeekR1_7B_4bit", configuration: LLMRegistry.deepSeekR1_7B_4bit, type: .llm),
        LMModel(name: "llama3_1_8B_4bit", configuration: LLMRegistry.llama3_1_8B_4bit, type: .llm),
        LMModel(name: "phi3_5_4bit", configuration: LLMRegistry.phi3_5_4bit, type: .llm),
        LMModel(name: "llama3.2:1b", configuration: LLMRegistry.llama3_2_1B_4bit, type: .llm),
        LMModel(name: "qwen2.5:1.5b", configuration: LLMRegistry.qwen2_5_1_5b, type: .llm),
        LMModel(name: "qwen2_5_7b", configuration: LLMRegistry.qwen2_5_7b, type: .llm),
        LMModel(name: "smolLM:135m", configuration: LLMRegistry.smolLM_135M_4bit, type: .llm),
        LMModel(name: "qwen3:0.6b", configuration: LLMRegistry.qwen3_0_6b_4bit, type: .llm),
        LMModel(name: "qwen3:1.7b", configuration: LLMRegistry.qwen3_1_7b_4bit, type: .llm),
        LMModel(name: "qwen3:4b", configuration: LLMRegistry.qwen3_4b_4bit, type: .llm),
        LMModel(name: "qwen3:8b", configuration: LLMRegistry.qwen3_8b_4bit, type: .llm),
        LMModel(name: "gemma3n:E2B", configuration: LLMRegistry.gemma3n_E2B_it_lm_4bit, type: .llm),
        LMModel(name: "gemma3n:E4B", configuration: LLMRegistry.gemma3n_E4B_it_lm_4bit, type: .llm),
    ]
    
    var running = false

    var includeWeatherTool = false
    var enableThinking = false

    var prompt = ""
    var output = ""
    var modelInfo = ""

    // Download progress tracking
    var downloadProgress: Double?
    var totalSize: String?

    // Performance metrics
    var tokensPerSecond: Double = 0.0
    var timeToFirstToken: Double = 0.0
    var promptLength: Int = 0
    var totalTokens: Int = 0
    var totalTime: Double = 0.0

    // Track if generation was truncated due to hitting max tokens
    var wasTruncated: Bool = false

    // Timer for tracking TTFT in real-time
    private var ttftTimer: Timer?
    private var generationStartTime: TimeInterval = 0

    // Timer for tracking tokens/sec and total time in real-time
    private var generationTimer: Timer?
    private var firstTokenTime: TimeInterval = 0

    
    /// This controls which model loads.
    
    var selectedModel = LMModel(name: "qwen2_5_7b", configuration: LLMRegistry.qwen2_5_7b, type: .llm)
    
    var modelConfiguration: ModelConfiguration {
        selectedModel.configuration
    }

    
    var maxTokens = 1024
    var maxKVSize: Int = 8192
    var kvBits: Int = 4
    var kvGroupSize: Int = 64
    var quantizedKVStart: Int = 512

    var temperature: Float = 0.8
    var topP: Float = 0.95
    var topK: Int = 40
    var minP: Float = 0.05
    var repetitionPenalty: Float = 1.1
    var repetitionContextSize: Int = 64
    var prefillStepSize: Int = 512
    var presencePenalty: Float = 0
    var presenceContextSize: Int = 64
    var frequencyPenalty: Float = 0
    var frequencyContextSize: Int = 64
    
    
    //SAFE
    //var maxTokens = 800
//    var maxKVSize: Int = 2048
//    var kvBits: Int = 4
//    var kvGroupSize: Int = 64
//    var quantizedKVStart: Int = 0
//
//    var temperature: Float = 0.8
//    var topP: Float = 0.9
//    var repetitionPenalty: Float = 1.1
//    var prefillStepSize: Int = 64          
    
    var generateParameters: GenerateParameters {
        GenerateParameters(
            maxTokens: maxTokens,
            maxKVSize: maxKVSize,
            kvBits: kvBits,
            kvGroupSize: kvGroupSize,
            quantizedKVStart: quantizedKVStart,
            temperature: temperature,
            topP: topP,
            topK: topK,
            minP: minP,
            repetitionPenalty: repetitionPenalty,
            repetitionContextSize: repetitionContextSize,
            presencePenalty: presencePenalty,
            presenceContextSize: presenceContextSize,
            frequencyPenalty: frequencyPenalty,
            frequencyContextSize: frequencyContextSize,
            prefillStepSize: prefillStepSize
        )
    }

    /// A task responsible for handling the generation process.
    var generationTask: Task<Void, Error>?

    /// Tool executor for function calling
    //private let toolExecutor: ToolExecutor?

    enum LoadState {
        case idle
        case loading
        case loaded(ModelContainer)
    }

    var loadState = LoadState.idle

    var isLoading: Bool {
        if case .loading = loadState {
            return true
        }
        return false
    }

    /// Short model name extracted from the full model ID.
    private var modelName: String {
        modelConfiguration.name.components(separatedBy: "/").last ?? modelConfiguration.name
    }

    /// Load and return the model. Can be called multiple times; subsequent calls return the cached model.
    func load() async throws -> ModelContainer {
        while true {
            switch loadState {
            case .idle:
                return try await performLoad()

            case .loading:
                // Already loading, wait and retry
                try await Task.sleep(for: .milliseconds(100))

            case .loaded(let modelContainer):
                return modelContainer
            }
        }
    }

    private func performLoad() async throws -> ModelContainer {
        loadState = .loading
        modelInfo = "Downloading \(modelName)..."
        downloadProgress = 0.0
        Memory.cacheLimit = 10 * 1024 * 1024
        let hub = HubApi(
            downloadBase: FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask).first
        )

        do {
            let modelDirectory = try await downloadModel(
                hub: hub,
                configuration: modelConfiguration
            ) { [weak self] progress in
                Task { @MainActor in
                    self?.updateDownloadProgress(progress)
                }
            }

            // Verify the download succeeded by checking for model files
            let fileManager = FileManager.default
            let directoryExists = fileManager.fileExists(atPath: modelDirectory.path)
            let contents = (try? fileManager.contentsOfDirectory(atPath: modelDirectory.path)) ?? []
            let hasSafetensors = contents.contains { $0.hasSuffix(".safetensors") }

            if !directoryExists || !hasSafetensors {
                throw NSError(
                    domain: "LLMEvaluator",
                    code: -1,
                    userInfo: [
                        NSLocalizedDescriptionKey:
                            "Model download failed. Please check your network connection and try again."
                    ]
                )
            }

            modelInfo = "Loading \(modelName)..."
            downloadProgress = nil
            totalSize = nil
            let modelContainer = try await LLMModelFactory.shared.loadContainer(
                hub: hub,
                configuration: modelConfiguration
            ) { _ in }

            let numParams = await modelContainer.perform { $0.model.numParameters() }

            self.prompt = ""
            self.modelInfo = formatModelInfo(name: modelConfiguration.name, parameters: numParams)
            loadState = .loaded(modelContainer)
            return modelContainer

        } catch {
            resetLoadingState()
            throw error
        }
    }


    private func updateDownloadProgress(_ progress: Progress) {
        modelInfo = "Downloading \(modelName) (\(Int(progress.fractionCompleted * 100))%)"
        downloadProgress = progress.fractionCompleted

        // Get file count info
        if progress.totalUnitCount > 0 && progress.totalUnitCount < 100 {
            totalSize = "File \(progress.completedUnitCount + 1) of \(progress.totalUnitCount)"
        } else if progress.totalUnitCount > 0 {
            totalSize =
                "\(formatBytes(progress.completedUnitCount)) of \(formatBytes(progress.totalUnitCount))"
        } else {
            totalSize = nil
        }
    }

    public func resetLoadingState() {
        loadState = .idle
        downloadProgress = nil
        totalSize = nil
    }

    private func formatModelInfo(name: String, parameters: Int) -> String {
        // Extract model name from full ID (e.g., "mlx-community/Qwen3-8B-4bit" -> "Qwen3-8B-4bit")
        let modelName = name.components(separatedBy: "/").last ?? name

        // Format parameter count (convert millions to billions if appropriate)
        let paramMillions = parameters / (1024 * 1024)
        let paramString: String
        if paramMillions >= 1000 {
            let paramBillions = Double(paramMillions) / 1000.0
            paramString = String(format: "%.1fB", paramBillions)
        } else {
            paramString = "\(paramMillions)M"
        }

        return "\(modelName) • \(paramString) parameters"
    }

    private func formatBytes(_ bytes: Int64) -> String {
        let formatter = ByteCountFormatter()
        formatter.allowedUnits = [.useMB, .useGB]
        formatter.countStyle = .file
        return formatter.string(fromByteCount: bytes)
    }

}
