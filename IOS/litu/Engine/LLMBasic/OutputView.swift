// Copyright © 2025 Apple Inc.

import MarkdownUI
import SwiftUI

struct OutputView: View {
    let output: String
    let displayStyle: LocalLLMView.DisplayStyle
    let wasTruncated: Bool

    var body: some View {
        ScrollView(.vertical) {
            ScrollViewReader { sp in
                VStack(alignment: .leading, spacing: 12) {
                    Group {
                        if displayStyle == .plain {
                            Text(output)
                                .textSelection(.enabled)
                        } else {
                            Markdown(output)
                                .textSelection(.enabled)
                        }
                    }

                    // Warning banner when output is truncated
                    if wasTruncated && !output.isEmpty {
                        HStack(spacing: 8) {
                            Image(systemName: "exclamationmark.triangle.fill")
                                .foregroundStyle(.orange)
                            Text("Output truncated: Maximum token limit reached")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                        .padding(8)
                        .background(.orange.opacity(0.1), in: RoundedRectangle(cornerRadius: 6))
                    }
                }
                .onChange(of: output) { _, _ in
                    sp.scrollTo("bottom")
                }

                Spacer()
                    .frame(width: 1, height: 1)
                    .id("bottom")
            }
        }
    }
}

// Copyright © 2025 Apple Inc.

struct OutputViewSecond: View {
    let output: String
    let displayStyle: LocalLLMView.DisplayStyle
    let wasTruncated: Bool

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Group {
                if displayStyle == .plain {
                    Text(output)
                        .textSelection(.enabled)
                } else {
                    Markdown(output)
                        .textSelection(.enabled)
                        .markdownTextStyle(\.text) {
                            FontSize(16)
                        }
                        .markdownTextStyle(\.strong) {
                            FontWeight(.semibold)
                        }
                }
            }
            .fixedSize(horizontal: false, vertical: true)
            .frame(maxWidth: .infinity, alignment: .leading)

            // Предупреждение о достижении лимита токенов
            if wasTruncated && !output.isEmpty {
                HStack(spacing: 8) {
                    Image(systemName: "exclamationmark.triangle.fill")
                        .foregroundStyle(.orange)
                    Text("Output truncated: Maximum token limit reached")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                .padding(.horizontal, 10)
                .padding(.vertical, 8)
                .background(.orange.opacity(0.12), in: RoundedRectangle(cornerRadius: 8))
            }
        }
    }
}


