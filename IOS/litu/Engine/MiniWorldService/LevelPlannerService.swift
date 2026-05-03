//
//  LevelPlannerService.swift
//  LITU
//
//  Created by ilya on 06/04/2026.
//

import Foundation
import MLXLMCommon

enum LevelPlannerServiceError: Error, LocalizedError {
    case modelLoadFailed(String)
    case emptyResponse
    case decodingFailed(String)
    case timeout
    case cancelled

    var errorDescription: String? {
        switch self {
        case .modelLoadFailed(let message):
            return "Model load failed: \(message)"
        case .emptyResponse:
            return "Model returned empty response"
        case .decodingFailed(let raw):
            return "Failed to decode LevelsResponse from model response:\n\(raw)"
        case .timeout:
            return "Timed out waiting for model response"
        case .cancelled:
            return "Generation cancelled"
        }
    }
}

@MainActor
@Observable
final class LevelPlannerService {

    private let llm: LLMEvaluator
    public var chatModel: ChatModel

    var timeoutNanoseconds: UInt64 = 200_000_000_000
    var pollNanoseconds: UInt64 = 120_000_000

    init(llm: LLMEvaluator, chatModel: ChatModel) {
        self.llm = llm
        self.chatModel = chatModel
    }

    func interpret(context: LevelGenerationRequest) async throws -> String {
        let modelContainer: ModelContainer
        do {
            modelContainer = try await llm.load()
        } catch {
            throw LevelPlannerServiceError.modelLoadFailed(error.localizedDescription)
        }

        let prompt = try makeUserPrompt(request: context)

        
        chatModel.dropSession()
        chatModel = ChatModel()
        chatModel.systemPrompt = makeLevelSystemPrompt()
        if !chatModel.hasSession {
            chatModel.createSession(
                model: modelContainer,
                genParameters: llm.generateParameters
            )
        }

        let raw = try await runOneShot(prompt: prompt)

        if let extracted = extractJSONObject(from: raw) {
            return extracted
        }

        throw LevelPlannerServiceError.decodingFailed(raw)
    }

    func resetSession() {
        chatModel.dropSession()
    }

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

    private func makeLevelSystemPrompt() -> String {
        """
        You generate level layouts for a simple 3-lane obstacle game.

        Return ONLY valid JSON.
        Do not add markdown.
        Do not add explanations outside JSON.
        Do not invent new segment types.

        Allowed segment types:
        intro_jump
        low_block
        tall_block
        wide_block
        left_bypass
        right_bypass
        pillar_pair
        jump_sequence
        vanishing_guard
        bounce_punish
        safe_zone
        goal_guard

        Task rules:
        - You only choose abstract segment types
        - You do not choose coordinates
        - You do not describe physics
        - You do not explain implementation
        - You only build playable segment sequences

        Playability rules:
        - Every level must be possible to complete
        - Never create an impossible sequence
        - If tall_block appears, the level should still remain playable
        - Avoid more than 2 challenging segments in a row
        - Prefer natural pacing
        - Use safe_zone when needed to create breathing room
        - Avoid repeating the same segment more than twice in a row

        Difficulty rules:
        - easy: simple, forgiving, mostly intro_jump, low_block, safe_zone, and at least one light variation
        - medium: should include decision-making such as jump or bypass, with moderate variety
        - hard: should combine mechanics such as jump, bypass, timing, vanishing_guard, bounce_punish, or jump_sequence

        Variety rules:
        - Levels should not all have the same structure
        - Do not reuse the exact same segment sequence across levels
        - Vary the starting pattern sometimes
        - Easy levels should still feel engaging, not trivial

        Output formats:

        If the user asks for ONE level, return:
        {
          "segments": ["segment1", "segment2", "segment3", "segment4", "segment5", "segment6"],
          "difficulty": "easy|medium|hard",
          "note": "short reason"
        }

        If the user asks for MULTIPLE levels, return:
        {
          "levels": [
            {
              "segments": ["segment1", "segment2", "segment3", "segment4", "segment5", "segment6"],
              "difficulty": "easy|medium|hard",
              "note": "short reason"
            }
          ]
        }

        Keep "note" short and simple.
        """
    }

    private func makeUserPrompt(request: LevelGenerationRequest) throws -> String {
        let data = try JSONEncoder().encode(request)
        let json = String(decoding: data, as: UTF8.self)
        return """
        Request:
        \(json)

        Generate levels as JSON only.
        """
    }

    private func tryDecode(from text: String) -> LevelsResponse? {
        let cleaned = stripCodeFences(text)
        guard let data = cleaned.data(using: .utf8) else { return nil }
        return try? JSONDecoder().decode(LevelsResponse.self, from: data)
    }

    private func stripCodeFences(_ s: String) -> String {
        var t = s.trimmingCharacters(in: .whitespacesAndNewlines)

        if t.hasPrefix("```") {
            if let firstNewline = t.firstIndex(of: "\n") {
                t = String(t[t.index(after: firstNewline)...])
            }
            if let range = t.range(of: "```", options: .backwards) {
                t.removeSubrange(range.lowerBound..<t.endIndex)
            }
        }

        return t.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private func extractJSONObject(from s: String) -> String? {
        let t = stripCodeFences(s)
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
