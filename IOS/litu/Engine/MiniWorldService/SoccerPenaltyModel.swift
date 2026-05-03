//
//  SoccerPenaltyModel.swift
//  LITU
//
//  Created by copilot on 14/04/2026.
//

import Foundation

// MARK: - Ball distance category

enum BallDistance: String, Codable {
    case close
    case medium
    case far
}

// MARK: - Shot direction (8 sectors)

enum ShotDirection: String, Codable {
    case topLeft     = "top-left"
    case leftCenter  = "left-center"
    case bottomLeft  = "bottom-left"
    case topCenter   = "top-center"
    case bottomCenter = "bottom-center"
    case topRight    = "top-right"
    case rightCenter = "right-center"
    case bottomRight = "bottom-right"
}

// MARK: - LLM request

struct PenaltyShotRequest: Codable {
    let ballDistance: BallDistance
    let shotDirection: ShotDirection
}

// MARK: - LLM response

struct GoalkeeperResponse: Codable {
    let jumpDirection: ShotDirection
    let intensity: String  // "low" | "medium" | "high"
    let note: String?
}
