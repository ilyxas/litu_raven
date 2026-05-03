//
//  SoccerPenaltyPlannerService.swift
//  LITU
//
//  Created by copilot on 14/04/2026.
//

import Foundation
import MLXLMCommon

/// Drives the goalkeeper AI decision via LLM.
@MainActor
@Observable
final class SoccerPenaltyPlannerService {

    private let llm: LLMEvaluator
    public var chatModel: ChatModel

    // Allow up to 200 s for the model to respond — LLM inference on-device can
    // be slow on first run (model load + warm-up). The game suspends physics while
    // waiting, so no gameplay disruption occurs.
    var timeoutNanoseconds: UInt64 = 200_000_000_000
    var pollNanoseconds: UInt64 = 120_000_000

    init(llm: LLMEvaluator, chatModel: ChatModel) {
        self.llm = llm
        self.chatModel = chatModel
    }

    /// Sends the natural-language game state to the LLM and returns
    /// the goalkeeper's reaction.
    func interpret(request: PenaltyShotRequest) async throws -> GoalkeeperResponse {
        let modelContainer: ModelContainer
        do {
            modelContainer = try await llm.load()
        } catch {
            throw LevelPlannerServiceError.modelLoadFailed(error.localizedDescription)
        }

        let prompt = makeUserPrompt(request: request)

        chatModel.dropSession()
        chatModel = ChatModel()
        chatModel.systemPrompt = makeGoalkeeperSystemPrompt()
        if !chatModel.hasSession {
            chatModel.createSession(
                model: modelContainer,
                genParameters: llm.generateParameters
            )
        }

        let raw = try await runOneShot(prompt: prompt)

        if let extracted = extractJSONObject(from: raw),
           let data = extracted.data(using: .utf8),
           let response = try? JSONDecoder().decode(GoalkeeperResponse.self, from: data) {
            return response
        }

        // Fallback: random jump
        throw LevelPlannerServiceError.decodingFailed(raw)
    }

    func resetSession() {
        chatModel.dropSession()
    }

    // MARK: - Private helpers

    private func runOneShot(prompt: String) async throws -> String {
        if Task.isCancelled { throw LevelPlannerServiceError.cancelled }

        let initialCount = chatModel.messages.count
        chatModel.respondBuffered(prompt)

        let start = DispatchTime.now().uptimeNanoseconds

        while chatModel.isBusy {
            if Task.isCancelled {
                chatModel.cancel()
                throw LevelPlannerServiceError.cancelled
            }

            let now = DispatchTime.now().uptimeNanoseconds
            if now - start > timeoutNanoseconds {
                chatModel.cancel()
                throw LevelPlannerServiceError.timeout
            }

            try await Task.sleep(nanoseconds: pollNanoseconds)
        }

        let newMessages = Array(chatModel.messages.dropFirst(initialCount))
        guard let assistant = newMessages.last(where: { $0.role == .assistant }) else {
            throw LevelPlannerServiceError.emptyResponse
        }

        let content = assistant.content.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !content.isEmpty, content != "..." else {
            throw LevelPlannerServiceError.emptyResponse
        }

        return content
    }

    private func makeGoalkeeperSystemPrompt() -> String {
        """
        You are a soccer goalkeeper AI deciding how to react to a penalty kick.

        Return ONLY valid JSON.
        Do not add markdown.
        Do not add explanations outside JSON.

        Input fields:
        - ballDistance: "close" | "medium" | "far"
        - shotDirection: one of 8 sectors describing where the ball is heading,
          from the GOALKEEPER's perspective (facing the player):
          "top-left" | "left-center" | "bottom-left" | "top-center" | "bottom-center"
          | "top-right" | "right-center" | "bottom-right"

        IMPORTANT — Direction convention:
        Both shotDirection and jumpDirection use the GOALKEEPER's reference frame.
        "left" means the goalkeeper's left side; "right" means the goalkeeper's right side.
        To BLOCK the shot, your jumpDirection must be the SAME value as shotDirection.
        Example: shot "top-left" → jump "top-left" to block it.

        Output format:
        {
          "jumpDirection": "<one of the 8 sectors above>",
          "intensity": "low | medium | high",
          "note": "short reason"
        }

        Goalkeeper rules:
        - React based on the shot direction and distance.
        - If the ball is far, the goalkeeper has more reaction time — be more accurate.
        - If the ball is close, reaction is harder — allow some misses.
        - Vary the reactions slightly to be realistic (do not always perfectly predict the shot).
        - "intensity" reflects how far / fast the goalkeeper dives.
        - Keep "note" to 10 words or less.
        """
    }

    private func makeUserPrompt(request: PenaltyShotRequest) -> String {
        let data = (try? JSONEncoder().encode(request)) ?? Data()
        let json = String(decoding: data, as: UTF8.self)
        return """
        Shot data:
        \(json)

        Respond with goalkeeper reaction JSON only.
        """
    }

    private func extractJSONObject(from s: String) -> String? {
        var t = s.trimmingCharacters(in: .whitespacesAndNewlines)

        if t.hasPrefix("```") {
            if let firstNewline = t.firstIndex(of: "\n") {
                t = String(t[t.index(after: firstNewline)...])
            }
            if let range = t.range(of: "```", options: .backwards) {
                t.removeSubrange(range.lowerBound..<t.endIndex)
            }
            t = t.trimmingCharacters(in: .whitespacesAndNewlines)
        }

        guard let start = t.firstIndex(of: "{") else { return nil }

        var depth = 0
        var inString = false
        var escape = false
        var index = start

        while index < t.endIndex {
            let ch = t[index]

            if inString {
                if escape {
                    escape = false
                } else if ch == "\\" {
                    escape = true
                } else if ch == "\"" {
                    inString = false
                }
            } else {
                if ch == "\"" {
                    inString = true
                } else if ch == "{" {
                    depth += 1
                } else if ch == "}" {
                    depth -= 1
                    if depth == 0 {
                        let end = t.index(after: index)
                        return String(t[start..<end]).trimmingCharacters(in: .whitespacesAndNewlines)
                    }
                }
            }

            index = t.index(after: index)
        }

        return nil
    }
}
