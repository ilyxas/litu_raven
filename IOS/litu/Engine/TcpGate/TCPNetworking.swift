//
//  SimpleTCPClient.swift
//  litu
//
//  Created by ilya on 17/04/2026.
//

import Foundation
import Network
import Combine


struct TelemetrySnapshot: Codable, Sendable {
    var sent_ts: UInt32
    var navigation: Navigation
    var system: System
    var sensors: Sensors

    struct Navigation: Codable, Sendable {
        var speed: Float
        var direction: Float
        var position_x: Float
        var position_y: Float
        var ts: UInt32
    }

    struct System: Codable, Sendable {
        var rssi: Int32
        var heap_free: UInt32
        var battery_pct: Float
        var ts: UInt32
    }

    struct Sensors: Codable, Sendable {
        var lidar_distance: Float
        var humidity: Float
        var temperature: Float
        var noise_level: Float
        var ts: UInt32
    }
}

@MainActor
@Observable
public final class TCPNetworking {
    let tcpClient: SimpleTCPClient
    let tcpServer: SimpleTCPServer
    
    init(client: SimpleTCPClient, server: SimpleTCPServer) {
        self.tcpClient = client
        self.tcpServer = server
    }
    
    
    public var totalLog: [Network.Message] = []
    
    public enum SenderActor: String, CaseIterable {
        case server, client
    }
    
    //function threadsafe log appending from client/server
    public func appendLog(_ message: String, from senderActor: SenderActor) {
        Task { @MainActor in
                //save only 50 last lines
            if self.totalLog.count > 50 {
                    self.totalLog.removeFirst()
                }
                //create message with role and content
                let role: Network.Message.Role = senderActor == .server ? .server : .client
                let logMessage = Network.Message(role: role, content: message)
                totalLog.append(logMessage)
            }
    }
}

public enum Network {
    public struct Message {
        /// The role of the message sender.
        public var role: Role

        /// The content of the message.
        public var content: String

        public init(
            role: Role, content: String
        ) {
            self.role = role
            self.content = content
        }

        public static func server(
            _ content: String
        ) -> Self {
            Self(role: .server, content: content)
        }

        public static func client(
            _ content: String
        ) -> Self {
            Self(role: .client, content: content)
        }

        public enum Role: String, Sendable {
            case client
            case server
        }
    }
}
