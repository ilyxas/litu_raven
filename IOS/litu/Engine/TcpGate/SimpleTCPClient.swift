//
//  SimpleTCPClient.swift
//  litu
//
//  Created by ilya on 17/04/2026.
//

import Foundation
import Network
import Combine

@MainActor
final class SimpleTCPClient: ObservableObject  {
    weak var networking: TCPNetworking?

    private var connection: NWConnection?

    func connect(host: String = "172.20.10.13", port: UInt16 = 8080) {
        let nwHost = NWEndpoint.Host(host)
        let nwPort = NWEndpoint.Port(rawValue: port)!

        let connection = NWConnection(host: nwHost, port: nwPort, using: .tcp)
        self.connection = connection

        connection.stateUpdateHandler = { [weak self] state in
            Task { @MainActor in
                self?.networking?.appendLog("state: \(state)", from: .client)
                switch state {
                case .ready:
                    //self?.send("hello from iphone client\n")
                    self?.receive()
                default:
                    break
                }
            }
        }

        connection.start(queue: .global(qos: .userInitiated))
    }

    func send(_ text: String) {
        guard let connection else { return }

        let data = Data(text.utf8)
        connection.send(content: data, completion: .contentProcessed { [weak self] error in
            Task { @MainActor in
                if let error {
                    let line = "send error: \(error)"
                    self?.networking?.appendLog(line, from: .client)
                } else {
                    let line = "sent: \(text.trimmingCharacters(in: .newlines))"
                    self?.networking?.appendLog(line, from: .client)
                }
            }
        })
    }
    
    func sendMessage(msgId: UInt16, payload: Data) {
            guard let connection else { return }

            var data = Data()

            // Header
            var msgIdLE = msgId.littleEndian
            var sizeLE = UInt32(payload.count).littleEndian

            withUnsafeBytes(of: &msgIdLE) { data.append(contentsOf: $0) }
            withUnsafeBytes(of: &sizeLE) { data.append(contentsOf: $0) }

            // Payload
            data.append(payload)

            connection.send(content: data, completion: .contentProcessed {  [weak self]  error in
                Task { @MainActor in
                    if let error {
                        //self?.log.append("send error: \(error)")
                    } else {
                        //payload size sent
                        //self?.log.append("payload size: \(payload.count) bytes")
                    }
                }
            })
    }
    

    func receive() {
        guard let connection else { return }

        connection.receive(minimumIncompleteLength: 1, maximumLength: 4096) { [weak self] data, _, isComplete, error in
            if let data, !data.isEmpty {
                let text = String(decoding: data, as: UTF8.self)
                Task { @MainActor in
                    let line = "received: \(text)"
                    self?.networking?.appendLog(line, from: .client)
                }
            }

            if let error {
                Task { @MainActor in
                    let line = "receive error: \(error)"
                    self?.networking?.appendLog(line, from: .client)
                }
                return
            }

            if isComplete {
                Task { @MainActor in
                    let line = "connection complete"
                    self?.networking?.appendLog(line, from: .client)
                }
                return
            }

            Task { @MainActor in
                self?.receive()
            }
        }
    }

    func disconnect() {
        connection?.cancel()
        connection = nil
        Task { @MainActor in
            let line = "disconnected"
            self.networking?.appendLog(line, from: .client)
        }
    }
}

