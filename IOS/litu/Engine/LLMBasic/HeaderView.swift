// Copyright © 2025 Apple Inc.

import SwiftUI

struct HeaderView: View {
    var llm: LLMEvaluator
    @Binding var selectedDisplayStyle: LocalLLMView.DisplayStyle

    @Environment(\.horizontalSizeClass) var horizontalSizeClass

    var status: some View {
        // Model info with status
        HStack {
            VStack(alignment: .leading, spacing: 4) {
                Text("Model")
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Text(llm.modelInfo)
                    .font(.headline)
                    .lineLimit(1)
            }

            Spacer()
            if llm.running {
                HStack(spacing: 8) {
                    ProgressView()
                        .controlSize(.small)
                    Text("Generating...")
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                }
            }
        }
    }

    var tokens: some View {
        // Max tokens slider
        VStack(alignment: .leading, spacing: 4) {
            Text("Max Tokens: \(llm.maxTokens)")
                .font(.caption)
                .foregroundStyle(.secondary)

            Slider(
                value: Binding(
                    get: { log2(Double(llm.maxTokens)) },
                    set: { llm.maxTokens = Int(pow(2, $0)) }
                ),
                in: 10 ... 15,  // 2^10 (1024) to 2^15 (32768)
                step: 1
            )
            .frame(width: 120)
            .help("Maximum number of tokens to generate (1024-32768)")
        }
    }

    var display: some View {
        Picker("Display", selection: $selectedDisplayStyle) {
            ForEach(LocalLLMView.DisplayStyle.allCases, id: \.self) { option in
                Text(option.rawValue.capitalized)
                    .tag(option)
            }
        }
        .pickerStyle(.segmented)
        .labelsHidden()
        .frame(maxWidth: 180)
    }

    var body: some View {
        if horizontalSizeClass == .compact {
            VStack {
                status
                DisclosureGroup("Controls") {
                    VStack {
                        HStack {
                            tokens
                            display
                        }
                    }
                }
            }
        } else {
            VStack(alignment: .leading, spacing: 12) {
                status

                // Controls row
                HStack(spacing: 16) {
                    HStack(spacing: 24) {
                        tokens
                    }

                    Spacer()

                    display
                }
            }
            .padding(.bottom, 12)
        }
    }
}
