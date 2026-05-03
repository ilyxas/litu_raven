// Copyright © 2025 Apple Inc.

import MLXLMCommon
import SwiftUI
import MLX
import MLXLLM
import UniformTypeIdentifiers

struct LocalLLMView: View {
    let llm: LLMEvaluator
    let chatModel: ChatModel
    let deviceStat: DeviceStat

    enum DisplayStyle: String, CaseIterable, Identifiable {
        case plain, markdown
        var id: Self { self }
    }

    @State private var selectedDisplayStyle = DisplayStyle.markdown
    @State private var prompt = ""
    @State private var error: String?
    
    

    @FocusState private var promptFocused: Bool

    init(llm: LLMEvaluator, chatModel: ChatModel, deviceStat: DeviceStat) {
        Memory.cacheLimit = 2 * 1024 * 1024
        //MLX.GPU.set(cacheLimit: 1024 * 1024 * 1024)
        self.llm = llm
        self.chatModel = chatModel
        self.deviceStat = deviceStat
    }

    var body: some View {
        NavigationStack {
            ZStack {
                backgroundLayer
                
                ScrollViewReader { proxy in
                    ScrollView(.vertical) {
                        VStack(spacing: 12) {
                            ForEach(Array(chatModel.messages.enumerated()), id: \.offset) { _, message in
                                MessageBubbleRow(
                                    message: message,
                                    displayStyle: selectedDisplayStyle,
                                    wasTruncated: llm.wasTruncated
                                )
                            }

                            Color.clear
                                .frame(height: 1)
                                .id("chat-bottom")
                        }
                        .padding(.horizontal, 14)
                        .padding(.top, 12)
                        .padding(.bottom, 120)
                    }
                    .scrollDismissesKeyboard(.interactively)
                    .onChange(of: chatModel.messages.count) { _, _ in
                        withAnimation(.easeOut(duration: 0.2)) {
                            proxy.scrollTo("chat-bottom", anchor: .bottom)
                        }
                    }
                    .onAppear {
                        proxy.scrollTo("chat-bottom", anchor: .bottom)
                    }
                }
            }
            .overlay(alignment: .bottom) {
                composerBar
                    .padding(.horizontal, 12)
                    .padding(.bottom, 10)
            }
            .navigationTitle("")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .principal) {
                    VStack(spacing: 3) {
                        Text("Local Chat")
                            .font(.headline)

                        Text(titleSubtitle)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .lineLimit(1)
                        Text(memoryBase)
                            .font(.footnote)
                            .foregroundStyle(.green)
                            .lineLimit(1)
                    }
                }

                ToolbarItem(placement: .topBarTrailing) {
                    NavigationLink {
                        LocalLLMSettingsView(
                            llm: llm,
                            deviceStat: deviceStat,
                            chatModel: chatModel,
                            selectedDisplayStyle: $selectedDisplayStyle
                        )
                    } label: {
                        Image(systemName: "gearshape.fill")
                    }
                }
            }
            .task {
                do {
                    let model = try await llm.load()

                    if !chatModel.hasSession {
                        if chatModel.messages.isEmpty {
                            chatModel.createSession(
                                model: model,
                                genParameters: llm.generateParameters
                            )
                        } else {
                            chatModel.restoreSession(
                                model: model,
                                genParameters: llm.generateParameters
                            )
                        }
                    }
                } catch {
                    self.error = error.localizedDescription
                }
            }
            .overlay {
                if llm.isLoading {
                    LoadingOverlayView(
                        modelInfo: llm.modelInfo,
                        downloadProgress: llm.downloadProgress,
                        progressDescription: llm.totalSize
                    )
                }
            }
            .alert("Error", isPresented: isErrorPresented) {
                Button("OK") {
                    error = nil
                }
            } message: {
                Text(error ?? "")
            }
        }
    }

    private var isErrorPresented: Binding<Bool> {
        Binding(
            get: { error != nil },
            set: { newValue in
                if !newValue {
                    error = nil
                }
            }
        )
    }

    private var titleSubtitle: String {
        if llm.isLoading {
            return llm.modelInfo.isEmpty ? "Loading..." : llm.modelInfo
        }

        if !llm.modelInfo.isEmpty {
            return llm.modelInfo
        }

        return "Model not loaded"
    }
    
    private var memoryBase: String {
        if deviceStat.gpuUsage.activeMemory > 0 {
            return "Active:" + FormatUtilities.formatMemory(deviceStat.gpuUsage.activeMemory)
                + " / Cache:" + FormatUtilities.formatMemory(deviceStat.gpuUsage.cacheMemory)
                + " / Peak:" + FormatUtilities.formatMemory(deviceStat.gpuUsage.peakMemory)
        } else {
            return "Memory usage unavailable"
        }
    }

    private var backgroundLayer: some View {
        ZStack {
            LinearGradient(
                colors: [
                    Color.cyan,
                    Color(red: 0.07, green: 0.08, blue: 0.12),
                    Color(red: 0.11, green: 0.11, blue: 0.17)
                ],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )

            RadialGradient(
                colors: [
                    Color.white.opacity(0.30),
                    Color.clear
                ],
                center: .top,
                startRadius: 40,
                endRadius: 420
            )

            LinearGradient(
                colors: [
                    Color.purple.opacity(0.14),
                    Color.clear,
                    Color.white.opacity(0.25)
                ],
                startPoint: .topTrailing,
                endPoint: .bottomLeading
            )
        }
        .ignoresSafeArea()
    }

    private var composerBar: some View {
        HStack(alignment: .bottom, spacing: 10) {


            HStack(alignment: .bottom, spacing: 8) {
                Button {
                    // future attach / tools
                } label: {
                    Image(systemName: "plus")
                        .font(.title3.weight(.semibold))
                        .foregroundStyle(.primary)
                        .frame(width: 32, height: 32)
                        .background(.ultraThinMaterial, in: Circle())
                }
                
                TextField("Message", text: $prompt, axis: .vertical)
                    .lineLimit(1...5)
                    .focused($promptFocused)
                    .onSubmit {
                        respond()
                    }

                if chatModel.isBusy {
                    Button {
                        chatModel.cancel()
                    } label: {
                        Image(systemName: "stop.fill")
                            .font(.headline)
                            .frame(width: 32, height: 32)
                    }
                } else {
                    Button {
                        respond()
                    } label: {
                        Image(systemName: "arrow.up")
                            .font(.headline)
                            .frame(width: 32, height: 32)
                    }
                    .disabled(trimmedPrompt.isEmpty)
                }
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 12)
            .background(.thickMaterial, in: Capsule())
        }
    }

    private var trimmedPrompt: String {
        prompt.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private func respond() {
        let text = trimmedPrompt
        guard !text.isEmpty else { return }

        chatModel.respondBuffered(text)
        prompt = ""
    }
}

private struct MessageBubbleRow: View {
    let message: Chat.Message
    let displayStyle: LocalLLMView.DisplayStyle
    let wasTruncated: Bool

    private var isUser: Bool {
        message.role == .user
    }

    private var isSystem: Bool {
        message.role == .system
    }

    var body: some View {
        HStack(alignment: .bottom) {
            if isUser {
                Spacer(minLength: 36)
            }

            VStack(alignment: .leading, spacing: 6) {
                // Заголовок роли
                if !isUser {
                    Text(roleTitle)
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }

                // Основной контент сообщения
                OutputViewSecond(
                    output: message.content,
                    displayStyle: displayStyle,
                    wasTruncated: wasTruncated && !isUser
                )
                .fixedSize(horizontal: false, vertical: true)
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 12)
            .background(bubbleBackground, in: RoundedRectangle(cornerRadius: 22, style: .continuous))
            .overlay {
                RoundedRectangle(cornerRadius: 22, style: .continuous)
                    .stroke(borderColor, lineWidth: 1)
            }

            if !isUser {
                Spacer(minLength: 36)
            }
        }
        .frame(maxWidth: .infinity)
    }

    private var roleTitle: String {
        switch message.role {
        case .user:
            return "You"
        case .assistant:
            return "Tessa"
        case .system:
            return "System"
        default:
            return "Message"
        }
    }

    private var bubbleBackground: some ShapeStyle {
        if isUser {
            return AnyShapeStyle(Color.blue.opacity(0.88))
        } else if isSystem {
            return AnyShapeStyle(Color.white.opacity(0.88))
        } else {
            return AnyShapeStyle(.ultraThinMaterial)
        }
    }

    private var borderColor: Color {
        if isUser {
            return Color.white.opacity(0.12)
        } else if isSystem {
            return Color.orange.opacity(0.28)
        } else {
            return Color.white.opacity(0.08)
        }
    }
}

struct LocalLLMSettingsView: View {
    @State public var isPromtImporting = false
    @State private var importPromtError: String? = nil
    let llm: LLMEvaluator
    let deviceStat: DeviceStat
    let chatModel: ChatModel

    @Binding var selectedDisplayStyle: LocalLLMView.DisplayStyle

    @Environment(\.dismiss) private var dismiss

    var body: some View {
        List {
            sessionSection
            modelSection
            generationSection
            displaySection
            metricsSection
            debugSection
        }
        .scrollContentBackground(.hidden)
        .background(
            LinearGradient(
                colors: [
                    Color.black,
                    Color(red: 0.07, green: 0.08, blue: 0.12)
                ],
                startPoint: .top,
                endPoint: .bottom
            )
            .ignoresSafeArea()
        )
        .navigationTitle("Settings")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                Button {
                    dismiss()
                } label: {
                    Image(systemName: "xmark")
                }
            }
        }
    }

    var sessionSection: some View {
        Section("Session") {
            TextField(
                "System Prompt",
                text: Binding(
                    get: { chatModel.systemPrompt },
                    set: { chatModel.systemPrompt = $0 }
                ),
                axis: .vertical
            )
            .lineLimit(3...10)
            
            Button {
                isPromtImporting = true
            } label: {
                Image(systemName: "arrow.up.doc")
            }
            .accessibilityLabel("Upload System Promt")
            
            Button("New Chat") {
                dismiss()
                Task {
                    do {
                        let model = try await llm.load()
                        chatModel.resetSession(
                            model: model,
                            genParameters: llm.generateParameters
                        )
                    } catch {
                    }
                }
            }

            Button("Restore Session From Messages") {
                dismiss()
                Task {
                    do {
                        let model = try await llm.load()
                        chatModel.restoreSession(
                            model: model,
                            genParameters: llm.generateParameters
                        )
                    } catch {
                    }
                }
            }
        }
        .fileImporter(
            isPresented: $isPromtImporting,
            allowedContentTypes: [.plainText,.text],
            allowsMultipleSelection: false
        ) { result in
            handlePromtImport(result)
        }
        .alert("Import Error", isPresented: Binding(
            get: { importPromtError != nil },
            set: { if !$0 { importPromtError = nil } }
        )) {
            Button("OK", role: .cancel) {
                importPromtError = nil
            }
        } message: {
            Text(importPromtError ?? "")
        }
    }

    private func handlePromtImport() {
        
    }
    
    private func handlePromtImport(_ result: Result<[URL], Error>) {
        do {
            let urls = try result.get()
            guard let url = urls.first else { return }

            let granted = url.startAccessingSecurityScopedResource()
            defer {
                if granted { url.stopAccessingSecurityScopedResource() }
            }
            let data = try Data(contentsOf: url)
            guard let content = String(data: data, encoding: .utf8) else {
                importPromtError = "Unable to decode file content"
                return
            }
            chatModel.systemPrompt = content
            
        } catch {
            importPromtError = error.localizedDescription
        }
    }
    
    private var modelSection: some View {
        Section("Model") {
            Text(llm.modelInfo.isEmpty ? "No model info" : llm.modelInfo)
                .foregroundStyle(.secondary)

            Picker("Model", selection: selectedModelBinding) {
                ForEach(llm.availableModels) { model in
                    Text(model.displayName).tag(model)
                }
            }

            Button("Load Model") {
                dismiss()
                Task {
                    do {
                        chatModel.resetState()
                        llm.resetLoadingState()

                        let model = try await llm.load()

                        chatModel.createSession(
                            model: model,
                            genParameters: llm.generateParameters
                        )
                    } catch {
                    }
                }
            }
        }
    }

    private var generationSection: some View {
            Section("Generation Parameters") {
                Stepper(
                    "Max Tokens: \(llm.generateParameters.maxTokens ?? 0)",
                    value: Binding(
                        get: { llm.generateParameters.maxTokens ?? 2048 },
                        set: { llm.maxTokens = $0 }
                    ),
                    in: 256...32768,
                    step: 256
                )
                
                parameterRow(
                    title: "Max KV Size",
                    value: Binding(
                        get: { llm.generateParameters.maxKVSize ?? 0 },
                        set: { llm.maxKVSize = $0 }
                    ),
                    range: 1024...65536,
                    step: 1024
                )
                
                parameterRow(
                    title: "KV Bits",
                    value: Binding(
                        get: { llm.generateParameters.kvBits ?? 4 },
                        set: { llm.kvBits = $0 }
                    ),
                    range: 1...8,
                    step: 1
                )
                
                parameterRow(
                    title: "KV Group Size",
                    value: Binding(
                        get: { llm.generateParameters.kvGroupSize },
                        set: { llm.kvGroupSize = $0 }
                    ),
                    range: 16...256,
                    step: 16
                )
                
                parameterRow(
                    title: "Quantized KV Start",
                    value: Binding(
                        get: { llm.generateParameters.quantizedKVStart },
                        set: { llm.quantizedKVStart = $0 }
                    ),
                    range: 0...4096,
                    step: 64
                )
                
                doubleParameterRow(
                    title: "Temperature",
                    value: Binding(
                        get: { llm.generateParameters.temperature },
                        set: { llm.temperature = $0 }
                    ),
                    range: 0.0...2.0,
                    step: 0.05
                )
                
                doubleParameterRow(
                    title: "Top P",
                    value: Binding(
                        get: { llm.generateParameters.topP },
                        set: { llm.topP = $0 }
                    ),
                    range: 0.0...1.0,
                    step: 0.01
                )
                
                parameterRow(
                    title: "Top K",
                    value: Binding(
                        get: { llm.generateParameters.topK },
                        set: { llm.topK = $0 }
                    ),
                    range: 0...200,
                    step: 1
                )
                
                doubleParameterRow(
                    title: "Min P",
                    value: Binding(
                        get: { llm.generateParameters.minP },
                        set: { llm.minP = $0 }
                    ),
                    range: 0.0...1.0,
                    step: 0.01
                )
                
                doubleParameterRow(
                    title: "Repetition Penalty",
                    value: Binding(
                        get: { llm.generateParameters.repetitionPenalty ?? 1.12 },
                        set: { llm.repetitionPenalty = $0 }
                    ),
                    range: 1.0...2.0,
                    step: 0.01
                )
                
                parameterRow(
                    title: "Repetition Context Size",
                    value: Binding(
                        get: { llm.generateParameters.repetitionContextSize },
                        set: { llm.repetitionContextSize = $0 }
                    ),
                    range: 0...512,
                    step: 8
                )
                
                doubleParameterRow(
                    title: "Presence Penalty",
                    value: Binding(
                        get: { llm.generateParameters.presencePenalty ?? 0.2 },
                        set: { llm.presencePenalty = $0 }
                    ),
                    range: 0.0...2.0,
                    step: 0.01
                )
                
                parameterRow(
                    title: "Presence Context Size",
                    value: Binding(
                        get: { llm.generateParameters.presenceContextSize },
                        set: { llm.presenceContextSize = $0 }
                    ),
                    range: 0...512,
                    step: 8
                )
                
                doubleParameterRow(
                    title: "Frequency Penalty",
                    value: Binding(
                        get: { llm.generateParameters.frequencyPenalty ?? 0.1 },
                        set: { llm.frequencyPenalty = $0 }
                    ),
                    range: 0.0...2.0,
                    step: 0.01
                )
                
                parameterRow(
                    title: "Frequency Context Size",
                    value: Binding(
                        get: { llm.generateParameters.frequencyContextSize },
                        set: { llm.frequencyContextSize = $0 }
                    ),
                    range: 0...512,
                    step: 8
                )
                
                parameterRow(
                    title: "Prefill Step Size",
                    value: Binding(
                        get: { llm.generateParameters.prefillStepSize },
                        set: { llm.prefillStepSize = $0 }
                    ),
                    range: 64...2048,
                    step: 64
                )
            }
    }

    private var displaySection: some View {
        Section("Display") {
            Picker("Style", selection: $selectedDisplayStyle) {
                ForEach(LocalLLMView.DisplayStyle.allCases) { style in
                    Text(style.rawValue.capitalized).tag(style)
                }
            }
            .pickerStyle(.segmented)
        }
    }

    private var debugSection: some View {
        Section("Debug") {
            LabeledContent("Has Session", value: chatModel.hasSession ? "Yes" : "No")
            LabeledContent("Is Busy", value: chatModel.isBusy ? "Yes" : "No")
            LabeledContent("Messages", value: "\(chatModel.messages.count)")
            LabeledContent("Loading", value: llm.isLoading ? "Yes" : "No")
        }
    }

    private var selectedModelBinding: Binding<LMModel> {
        Binding(
            get: { llm.selectedModel },
            set: { llm.selectedModel = $0 }
        )
    }

    private func parameterRow(
        title: String,
        value: Binding<Int>,
        range: ClosedRange<Int>,
        step: Int
    ) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("\(title): \(value.wrappedValue)")
                .font(.subheadline)

            Slider(
                value: Binding(
                    get: { Double(value.wrappedValue) },
                    set: { value.wrappedValue = Int($0) }
                ),
                in: Double(range.lowerBound)...Double(range.upperBound),
                step: Double(step)
            )
        }
        .padding(.vertical, 4)
    }
    
    private var metricsSection: some View {
        Section("Metrics") {
            MetricsView(
                tokensPerSecond: chatModel.tokensPerSecond,
                timeToFirstToken: chatModel.timeToFirstToken * 1000.0,
                promptLength: chatModel.promptLength,
                totalTokens: chatModel.totalTokens,
                totalTime: chatModel.totalTime,
                memoryUsed: deviceStat.gpuUsage.activeMemory,
                cacheMemory: deviceStat.gpuUsage.cacheMemory,
                peakMemory: deviceStat.gpuUsage.peakMemory
            )
            .listRowInsets(EdgeInsets(top: 8, leading: 0, bottom: 8, trailing: 0))
            .listRowBackground(Color.clear)
        }
    }

    private func doubleParameterRow(
        title: String,
        value: Binding<Float>,
        range: ClosedRange<Float>,
        step: Float
    ) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("\(title): \(String(format: "%.2f", value.wrappedValue))")
                .font(.subheadline)

            Slider(
                value: value,
                in: range,
                step: step
            )
        }
        .padding(.vertical, 4)
    }
}
