//
//  SoccerPenaltyView.swift
//  LITU
//
//  Created by copilot on 14/04/2026.
//

import SwiftUI
import RealityKit

struct SoccerPenaltyView: View {

    let planner: SoccerPenaltyPlannerService
    @State private var world: SoccerPenaltyWorld
    @State private var stick = CGSize.zero

    init(planner: SoccerPenaltyPlannerService) {
        self.planner = planner
        self._world = State(initialValue: SoccerPenaltyWorld(planner: planner))
    }

    var body: some View {
        ZStack {
            // MARK: 3-D scene
            RealityView { content in
                content.camera = .virtual
                content.add(world.root)

                _ = content.subscribe(to: SceneEvents.Update.self) { event in
                    Task { @MainActor in
                        world.step(dt: Float(event.deltaTime))
                    }
                }
            }
            .gesture(aimGesture)
            .gesture(doubleTapGesture)

            // MARK: Overlay UI
            VStack {
                // Status banner
                statusBanner

                Spacer()

                // Power meter (shown after double-tap)
                if world.showPowerMeter {
                    powerMeter
                        .padding(.bottom, 12)
                }

                HStack(alignment: .bottom) {
                    // Camera joystick (bottom-left)
                    cameraJoystick

                    Spacer()

                    // Shoot button (bottom-right)
                    shootButton
                }
                .padding(20)
            }

            // LLM loading indicator
            if world.isLLMBusy {
                llmIndicator
            }
        }
        .ignoresSafeArea()
    }

    // MARK: - Aim gesture (drag from anywhere on the scene)

    private var aimGesture: some Gesture {
        DragGesture(minimumDistance: 4)
            .onChanged { value in
               
                let bounds = world.goalPost.visualBounds(relativeTo: world.root)
                // Derive a 2D size from 3D bounding box extents (x ~ width, y ~ height)
                let width = max(CGFloat(bounds.extents.x), 1)
                let height = max(CGFloat(bounds.extents.y), 1)
                let nx = Float(value.translation.width  / width)  * 2
                let ny = Float(-value.translation.height / height) * 2
                let raw = SIMD2<Float>(nx, ny)
                let len = simd_length(raw)
                let clamped = len > 1 ? raw / len : raw
                world.updateAimDrag(clamped)
            }
            .onEnded { _ in
                guard world.phase == .aiming else { return }
                world.confirmAim()
            }
    }

    private var doubleTapGesture: some Gesture {
        TapGesture(count: 2).onEnded {
            if world.phase == .aiming || world.phase == .ready {
                world.showPowerMeter.toggle()
                if !world.showPowerMeter && world.phase == .aiming {
                        world.setPhaseReady()
                    }
            }
        }
    }

    // MARK: - Status banner

    @ViewBuilder
    private var statusBanner: some View {
        switch world.phase {
        case .aiming:
            banner("Pull to aim", color: .yellow)
        case .powerSelection:
            banner("Select shot power", color: .orange)
        case .ready:
            banner("Push to shoot", color: .green)
        case .waitingLLM:
            banner("Goalkeeper is thinking...", color: .blue)
        case .ballInFlight:
            EmptyView()
        case .scored:
            banner("⚽ Goal!", color: .green)
        case .blocked:
            banner("Saved by goalkeeper, try again!", color: .red)
        }
    }

    private func banner(_ text: String, color: Color) -> some View {
        Text(text)
            .font(.system(size: 17, weight: .semibold))
            .foregroundStyle(.white)
            .padding(.horizontal, 16)
            .padding(.vertical, 8)
            .background(color.opacity(0.75), in: Capsule())
            .padding(.top, 60)
    }

    // MARK: - Power meter

    private var powerMeter: some View {
        VStack(spacing: 6) {
            Text("Kick force")
                .font(.caption)
                .foregroundStyle(.white)

            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    RoundedRectangle(cornerRadius: 6)
                        .fill(.white.opacity(0.25))

                    RoundedRectangle(cornerRadius: 6)
                        .fill(
                            LinearGradient(
                                colors: [.green, .yellow, .red],
                                startPoint: .leading,
                                endPoint: .trailing
                            )
                        )
                        .frame(width: geo.size.width * CGFloat(world.shotPower))
                }
                .frame(height: 22)
                .gesture(
                    DragGesture(minimumDistance: 0)
                        .onChanged { value in
                            let pct = Float(value.location.x / geo.size.width)
                            world.setPower(pct)
                        }
                )
            }
            .frame(height: 22)

            Text("\(Int(world.shotPower * 100))%")
                .font(.caption2)
                .foregroundStyle(.white)
        }
        .padding(.horizontal, 32)
        .padding(.vertical, 10)
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
        .padding(.horizontal, 40)
    }

    // MARK: - Camera joystick

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
                    stick = CGSize(width: x, height: y)
                    world.cameraInput = SIMD2<Float>(
                        Float(x / limit),
                        Float(-y / limit)
                    )
                }
                .onEnded { _ in
                    stick = .zero
                    world.cameraInput = .zero
                }
        )
    }

    // MARK: - Shoot button

    private var shootButton: some View {
        let enabled = world.phase == .ready || world.phase == .aiming
        return Button {
            world.shoot()
        } label: {
            Text("Kick")
                .font(.system(size: 20, weight: .bold))
                .padding(.horizontal, 28)
                .padding(.vertical, 16)
        }
        .buttonStyle(.borderedProminent)
        .tint(.red)
        .disabled(!enabled)
    }

    // MARK: - LLM indicator

    private var llmIndicator: some View {
        VStack(spacing: 10) {
            ProgressView()
                .progressViewStyle(.circular)
                .tint(.white)
            Text("AI is thinking...")
                .font(.caption)
                .foregroundStyle(.white)
        }
        .padding(20)
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 14))
    }
}

