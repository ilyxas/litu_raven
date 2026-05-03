import Foundation
import Network
import Combine


@MainActor
final class SimpleTCPServer: ObservableObject, Sendable {
    weak var networking: TCPNetworking?

    private var listener: NWListener?
    private var connections: [NWConnection] = []
    private var receiveBuffer = Data()

    private let networkHeaderSize = 6
    private let telemetryMsgId: UInt16 = 0x3001

    func start(port: UInt16 = 8080) {
        do {
            let params = NWParameters.tcp
            let listener = try NWListener(using: params, on: NWEndpoint.Port(rawValue: port)!)

            listener.stateUpdateHandler = { [weak self] state in
                Task { @MainActor in
                    self?.networking?.appendLog("listener state: \(state)", from: .server)
                }
            }

            listener.newConnectionHandler = { [weak self] connection in
                Task { @MainActor in
                    self?.handle(connection: connection)
                }
            }

            self.listener = listener
            listener.start(queue: .global(qos: .userInitiated))
        } catch {
            Task { @MainActor in
                self.networking?.appendLog("listener failed: \(error)", from: .server)
            }
        }
    }

    func stop() {
        listener?.cancel()
        listener = nil
        connections.forEach { $0.cancel() }
        connections.removeAll()
        receiveBuffer.removeAll()
    }

    private func handle(connection: NWConnection) {
        connections.append(connection)

        connection.stateUpdateHandler = { [weak self] state in
            Task { @MainActor in
                self?.networking?.appendLog("connection state: \(state)", from: .server)
            }
        }

        connection.start(queue: .global(qos: .userInitiated))
        receive(on: connection)
    }

    private func receive(on connection: NWConnection) {
        connection.receive(minimumIncompleteLength: 1, maximumLength: 4096) { [weak self] data, _, isComplete, error in
            guard let self else { return }

            if let data, !data.isEmpty {
                Task { @MainActor in
                    self.processIncomingData(data, on: connection)
                }
            }

            if let error {
                Task { @MainActor in
                    self.networking?.appendLog("receive error: \(error)", from: .server)
                }
                return
            }

            if isComplete {
                Task { @MainActor in
                    self.networking?.appendLog("connection complete", from: .server)
                }
                return
            }

            Task { @MainActor in
                self.receive(on: connection)
            }
        }
    }

    private func readUInt16LE(_ data: Data, at offset: Int) -> UInt16 {
        let b0 = UInt16(data[offset])
        let b1 = UInt16(data[offset + 1]) << 8
        return b0 | b1
    }

    private func readUInt32LE(_ data: Data, at offset: Int) -> UInt32 {
        let b0 = UInt32(data[offset])
        let b1 = UInt32(data[offset + 1]) << 8
        let b2 = UInt32(data[offset + 2]) << 16
        let b3 = UInt32(data[offset + 3]) << 24
        return b0 | b1 | b2 | b3
    }

    private func processIncomingData(_ data: Data, on connection: NWConnection) {
        receiveBuffer.append(data)

        while receiveBuffer.count >= networkHeaderSize {
            let msgId = readUInt16LE(receiveBuffer, at: 0)
            let payloadSize = Int(readUInt32LE(receiveBuffer, at: 2))
            let fullMessageSize = networkHeaderSize + payloadSize

            if receiveBuffer.count < fullMessageSize {
                return
            }

            let payload = receiveBuffer.subdata(in: networkHeaderSize..<fullMessageSize)
            handleMessage(msgId: msgId, payload: payload, on: connection)

            receiveBuffer.removeSubrange(0..<fullMessageSize)
        }
    }

    private func handleMessage(msgId: UInt16, payload: Data, on connection: NWConnection) {
        let line1 = "frame msgId=0x\(String(msgId, radix: 16)) payload=\(payload.count) bytes"
        networking?.appendLog(line1, from: .server)

        if let text = String(data: payload, encoding: .utf8) {
            if msgId == telemetryMsgId {
               //prepare to decoding
                do {
                    let data = Data(text.utf8)
                    let decoder = JSONDecoder()
                    let snapshot = try decoder.decode(TelemetrySnapshot.self, from: data)

                    // Example: format a few values for logging
                    let summary = "ts=\(snapshot.sent_ts) speed=\(snapshot.navigation.speed) battery=\(snapshot.system.battery_pct)% temp=\(snapshot.sensors.temperature)"
                    // Forward to your aggregator if desired:
                    networking?.appendLog("telemetry: \(snapshot)", from: .server)

                    print(snapshot)  // or use it wherever needed
                } catch {
                    print("Decode error: \(error)")
                }
                
            } else {
                let line2 = "utf8 payload: \(text)"
                networking?.appendLog(line2, from: .server)
            }
        } else {
            let line2 = "binary payload: \(payload.count) bytes"
            networking?.appendLog(line2, from: .server)
        }

        let reply = Data("hello from iphone\n".utf8)
        connection.send(content: reply, completion: .contentProcessed { [weak self] sendError in
            if let sendError {
                Task { @MainActor in
                    let line = "send error: \(sendError)"
                    self?.networking?.appendLog(line, from: .server)
                }
            }
        })
    }
}
