//
//  TCPClientTestView.swift
//  litu
//
//  Created by ilya on 17/04/2026.
//
import MarkdownUI
import SwiftUI


struct JoystickPayload {
    var x: Float
    var y: Float
    var timestamp: UInt32
}

struct StartManualPayload {
    var timestamp: UInt32
}

struct TCPNetworkingView: View {
    let tcpNetworking: TCPNetworking
    
    enum DisplayStyle: String, CaseIterable, Identifiable {
        case plain, markdown
        var id: Self { self }
    }
    
    init(tcpNetworking: TCPNetworking) {
        self.tcpNetworking = tcpNetworking
    }
    
    @State private var selectedDisplayStyle = DisplayStyle.markdown
    @State private var stick = CGSize.zero
    
    var body: some View  {
        NavigationStack {
            ZStack {
                backgroundLayer
                
                ScrollViewReader { proxy in
                    ScrollView(.vertical) {
                        VStack(spacing: 12) {
                            ForEach(Array(tcpNetworking.totalLog.enumerated()), id: \.offset) { _, message in
                                MessageBubbleRow(
                                    message: message,
                                    displayStyle: selectedDisplayStyle,
                                    wasTruncated: false
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
                    .onChange(of: tcpNetworking.totalLog.count) { _, _ in
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
                cameraJoystick
                    .padding(.horizontal, 12)
                    .padding(.bottom, 10)
            }
        }
        .navigationTitle("")
        .toolbar {
            ToolbarItemGroup(placement: .topBarLeading) {
                Button(action: {
                    tcpNetworking.tcpClient.connect()
                }) {
                    Label("Connect", systemImage: "arrow.up.circle")
                }

                Button(action: {
                    tcpNetworking.tcpClient.disconnect()
                }) {
                    Label("Disconnect", systemImage: "arrow.down.circle")
                }
                
                Button(action: {
                    tcpNetworking.tcpClient.sendMessage(msgId: 0x1002, payload: serializeManual(StartManualPayload(timestamp: 123)))
                }) {
                    Label("Start Manual Nav", systemImage: "play.circle.fill")
                }
                
                Button(action: {
                    tcpNetworking.tcpClient.sendMessage(msgId: 0x1003, payload: serializeManual(StartManualPayload(timestamp: 123)))
                }) {
                    Label("Halt Manual", systemImage: "pause.circle.fill")
                }
                
                //delimiter
                Divider()

                Button(action: {
                    tcpNetworking.tcpServer.start()
                }) {
                    Label("Start Server", systemImage: "arrow.up.circle.fill")
                }

                Button(action: {
                    tcpNetworking.tcpServer.stop()
                }) {
                    Label("Stop Server", systemImage: "arrow.down.circle.fill")
                }
            }
        }
    }
    
    private struct MessageBubbleRow: View {
        let message: Network.Message
        let displayStyle : TCPNetworkingView.DisplayStyle
        let wasTruncated: Bool

        private var isServer: Bool {
            message.role == .server
        }

        private var isClient: Bool {
            message.role == .client
        }

        var body: some View {
            HStack(alignment: .bottom) {
                if isClient {
                    Spacer(minLength: 36)
                }

                VStack(alignment: .leading, spacing: 6) {
                    Text(roleTitle)
                        .font(.caption2)
                        .foregroundStyle(.secondary)

                    if displayStyle == .plain {
                        Text(message.content)
                            .textSelection(.enabled)
                    } else {
                        OutputViewSecond(
                            output: message.content,
                            displayStyle: displayStyle,
                            wasTruncated: wasTruncated && !isClient
                        )
                    }
                }
                .padding(.horizontal, 14)
                .padding(.vertical, 12)
                .background(bubbleBackground, in: RoundedRectangle(cornerRadius: 22, style: .continuous))
                .overlay {
                    RoundedRectangle(cornerRadius: 22, style: .continuous)
                        .stroke(borderColor, lineWidth: 1)
                }

                if !isClient {
                    Spacer(minLength: 36)
                }
            }
            .frame(maxWidth: .infinity)
        }

        private var roleTitle: String {
            switch message.role {
            case .client:
                return "Client"
            case .server:
                return "Server"
            default:
                return "Message"
            }
        }

        private var bubbleBackground: some ShapeStyle {
            if isClient {
                return AnyShapeStyle(Color.blue.opacity(0.88))
            } else if isServer {
                return AnyShapeStyle(Color.green.opacity(0.88))
            } else {
                return AnyShapeStyle(.ultraThinMaterial)
            }
        }

        private var borderColor: Color {
            if isClient {
                return Color.white.opacity(0.12)
            } else if isServer {
                return Color.orange.opacity(0.28)
            } else {
                return Color.white.opacity(0.08)
            }
        }
    }
    
    struct OutputView: View {
        let output: String
        let displayStyle: TCPNetworkingView.DisplayStyle
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
        let displayStyle: TCPNetworkingView.DisplayStyle
        let wasTruncated: Bool

        var body: some View {
            VStack(alignment: .leading, spacing: 12) {
                if displayStyle == .plain {
                    Text(output)
                        .textSelection(.enabled)
                } else {
                    Markdown(output)
                        .textSelection(.enabled)
                }

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
    
    private var cameraJoystick: some View {
        let size: CGFloat = 110
        let knob: CGFloat = 40
        let limit = (size - knob) / 2

        return ZStack {
            Circle()
                .fill(.thinMaterial)
                .frame(width: size, height: size)

            Circle()
                .fill(.ultraThickMaterial)
                .frame(width: knob, height: knob)
                .offset(stick)
        }
        .gesture(
            DragGesture(minimumDistance: 0)
                .onChanged { value in
                    var x = value.translation.width
                    var y = value.translation.height
                    let len = hypot(x, y)
                    if len > limit, len > 0 {
                        x = x / len * limit
                        y = y / len * limit
                    }
                    
                    let nx = Float(x / limit)
                    let ny = Float(-y / limit)
                    stick = CGSize(width: x, height: y)

                    let payload = JoystickPayload(x: nx, y: ny, timestamp: 123)
                    
                    tcpNetworking.tcpClient.sendMessage(msgId: 0x1001, payload: serializeJoystick(payload))
                }
                .onEnded { _ in
                    stick = .zero
                }
        )
    }
    
    func serializeJoystick(_ js: JoystickPayload) -> Data {
        var data = Data()

        var x = js.x.bitPattern.littleEndian
        var y = js.y.bitPattern.littleEndian
        var t = js.timestamp.littleEndian

        withUnsafeBytes(of: &x) { data.append(contentsOf: $0) }
        withUnsafeBytes(of: &y) { data.append(contentsOf: $0) }
        withUnsafeBytes(of: &t) { data.append(contentsOf: $0) }

        return data
    }
    
    func serializeManual(_ js: StartManualPayload) -> Data {
        var data = Data()
        
        var t = js.timestamp.littleEndian
        withUnsafeBytes(of: &t) { data.append(contentsOf: $0) }

        return data
    }
}

