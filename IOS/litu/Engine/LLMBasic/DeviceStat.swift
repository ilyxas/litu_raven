// Copyright © 2025 Apple Inc.

import Foundation
import MLX

@Observable
@MainActor
final class DeviceStat {

    @MainActor
    var gpuUsage = Memory.snapshot()

    private let initialGPUSnapshot = Memory.snapshot()
    private var timer: Timer?

    init() {
        timer = Timer.scheduledTimer(withTimeInterval: 2.0, repeats: true) { [weak self] _ in
            self?.updateGPUUsages()
        }
    }

//    deinit {
//        timer?.invalidate()
//    }

    private func updateGPUUsages() {
        let gpuSnapshotDelta = initialGPUSnapshot.delta(Memory.snapshot())
        DispatchQueue.main.async { [weak self] in
            self?.gpuUsage = gpuSnapshotDelta
        }
    }

}
