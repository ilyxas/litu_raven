//
//  LevelModel.swift
//  LITU
//
//  Created by ilya on 06/04/2026.
//

enum LevelSegment: String, Codable {
    case intro_jump
    case low_block
    case tall_block
    case wide_block
    case left_bypass
    case right_bypass
    case pillar_pair
    case jump_sequence
    case vanishing_guard
    case bounce_punish
    case safe_zone
    case goal_guard
}

struct LevelResponse: Codable {
    let segments: [LevelSegment]
    let difficulty: String
    let note: String?
}

struct LevelsResponse: Codable {
    let levels: [LevelResponse]
}

struct LevelGenerationRequest: Codable {
    let count: Int
    let segmentsPerLevel: Int
    let difficultyPlan: [String]
}
