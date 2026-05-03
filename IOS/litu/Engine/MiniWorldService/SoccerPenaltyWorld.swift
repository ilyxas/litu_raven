//
//  SoccerPenaltyWorld.swift
//  LITU
//
//  Created by copilot on 14/04/2026.
//

import SwiftUI
import RealityKit
import Combine

// MARK: - Game phase

enum PenaltyPhase {
    case aiming           // user drags to pick direction
    case powerSelection   // user sets power
    case ready            // direction + power chosen, waiting for "Удар"
    case waitingLLM       // sent to LLM, awaiting goalkeeper decision
    case ballInFlight     // physics running
    case scored           // goal — start new round
    case blocked          // goalkeeper saved — restart
}

// MARK: - World

@MainActor
@Observable
final class SoccerPenaltyWorld {

    // MARK: Public observable state

    private(set) var phase: PenaltyPhase = .aiming
    var cameraInput = SIMD2<Float>.zero    // x = rotate, y = zoom
    var shotPower: Float = 0.5            // 0..1
    var showPowerMeter: Bool = false
    var aimDirection: SIMD2<Float> = .zero // normalised screen drag
    var trajectoryPoints: [SIMD3<Float>] = []
    var isLLMBusy = false

    // MARK: Scene roots

    let root = Entity()

    // MARK: Private scene nodes

    private let ground: Entity
    private let ball: ModelEntity
    public let goalPost: Entity
    private let goalkeeper: Entity          // composite humanoid container (y=0 at ground)
    private let camera: PerspectiveCamera

    // Trajectory dashes
    private var dashEntities: [ModelEntity] = []

    // MARK: Camera orbit

    private var cameraAngle: Float = 0          // radians around Y
    private var cameraDistance: Float = 5.0

    // MARK: Ball physics state

    private var ballVelocity: SIMD3<Float> = .zero
    private var ballInFlight = false

    // MARK: Constants

    private let ballStartPosition: SIMD3<Float> = [0, 0.18, 0]
    private let goalCenter: SIMD3<Float>         = [0, 1.0, -6.0]
    private let goalWidth: Float                 = 3.6
    private let goalHeight: Float                = 2.0
    private let gravity: Float                   = 9.8
    private let ballRadius: Float                = 0.18

    // Goalkeeper container is placed at y=0 (ground level); body parts are children offset upward.
    // The torso center is ~0.65 m above the container origin, used for collision.
    private let gkTorsoCenterY: Float            = 0.65

    // MARK: Goalkeeper physics

    private var gkTargetPosition: SIMD3<Float>   = [0, 0, -5.9]
    private var gkVelocity: SIMD3<Float>         = .zero
    private var gkActive: Bool                   = false
    private var gkIntensity: Float               = 0.5

    // MARK: Services

    private let planner: SoccerPenaltyPlannerService

    // MARK: Init

    init(planner: SoccerPenaltyPlannerService) {
        self.planner = planner

        // --- Grass field with alternating stripes ---
        ground = SoccerPenaltyWorld.buildGrassField()

        // --- Ball ---
        let ballMesh = MeshResource.generateSphere(radius: ballRadius)
        var ballMat = SimpleMaterial()
        ballMat.color    = .init(tint: .white, texture: nil)
        ballMat.roughness = 0.4
        ball = ModelEntity(mesh: ballMesh, materials: [ballMat])
        ball.position = ballStartPosition

        // --- Goal post ---
        goalPost = SoccerPenaltyWorld.buildGoalPost(center: goalCenter,
                                                     width: goalWidth,
                                                     height: goalHeight)

        // --- Goalkeeper: humanoid composite ---
        goalkeeper = SoccerPenaltyWorld.buildHumanoidGoalkeeper()
        goalkeeper.position = [0, 0, -5.9]

        // --- Lighting: directional sun + ambient fill ---
        let sun = DirectionalLight()
        sun.light.intensity = 38_000
        sun.shadow = DirectionalLightComponent.Shadow()
        sun.orientation = simd_quatf(angle: -.pi / 3.5, axis: [1, 0, 0])

        let fill = DirectionalLight()
        fill.light.intensity = 6_000
        fill.orientation = simd_quatf(angle: .pi / 6, axis: [0, 1, 0]) *
                           simd_quatf(angle: .pi / 8, axis: [1, 0, 0])

        // --- Camera ---
        camera = PerspectiveCamera()
        camera.camera.fieldOfViewInDegrees = 65

        // --- Environment: sky + crowd ---
        let env = SoccerPenaltyWorld.buildEnvironment()

        // Build hierarchy
        root.addChild(sun)
        root.addChild(fill)
        root.addChild(camera)
        root.addChild(ground)
        root.addChild(ball)
        root.addChild(goalPost)
        root.addChild(goalkeeper)
        root.addChild(env)

        updateCamera()
    }

    // MARK: - Per-frame step

    func step(dt: Float) {
        // Camera orbit input
        if cameraInput != .zero {
            cameraAngle    += cameraInput.x * dt * 1.4
            cameraDistance  = max(2.0, min(10.0, cameraDistance - cameraInput.y * dt * 4.0))
            updateCamera()
        }

        guard ballInFlight else { return }

        // Ball flight
        ballVelocity.y -= gravity * dt
        ball.position  += ballVelocity * dt

        // Goalkeeper chase
        if gkActive {
            let diff  = gkTargetPosition - goalkeeper.position
            let dist  = simd_length(diff)
            let speed = gkIntensity * 6.0
            if dist > 0.01 {
                goalkeeper.position += (diff / dist) * min(speed * dt, dist)
            } else {
                goalkeeper.position = gkTargetPosition
            }
        }

        // --- Collision checks ---

        // 1. Ball hits goalkeeper? (check against torso center)
        let gkTorsoPos = goalkeeper.position + SIMD3<Float>(0, gkTorsoCenterY, 0)
        let toGK = simd_distance(ball.position, gkTorsoPos)
        if toGK < (ballRadius + 0.55) && ball.position.z < -4.5 {
            handleBlocked()
            return
        }

        // 2. Ball crosses goal plane?
        if ball.position.z <= goalCenter.z {
            checkGoalOrMiss()
            return
        }

        // 3. Ball hits ground?
        if ball.position.y < ballRadius {
            handleBlocked()
        }
    }

    // MARK: - Phase management (called from View)

    func setPhaseReady() {
        phase = .ready
    }

    func updateAimDrag(_ drag: SIMD2<Float>) {
        aimDirection = drag
        rebuildTrajectory()
    }

    func confirmAim() {
        phase = .ready
    }

    // MARK: - Power

    func setPower(_ power: Float) {
        shotPower = max(0, min(1, power))
    }

    func confirmPower() {
        showPowerMeter = false
        phase = .ready
    }

    // MARK: - Shoot

    func shoot() {
        guard phase == .ready || phase == .aiming else { return }
        phase = .waitingLLM

        let request = buildLLMRequest()
        isLLMBusy = true

        Task { @MainActor in
            do {
                let response = try await planner.interpret(request: request)
                applyGoalkeeperResponse(response)
            } catch {
                // Fallback: goalkeeper dives to a random corner
                applyFallbackGoalkeeper()
            }
            isLLMBusy = false
            launchBall()
        }
    }

    // MARK: - Round management

    private func handleBlocked() {
        ballInFlight = false
        phase = .blocked
        gkActive = false

        Task { @MainActor in
            try? await Task.sleep(nanoseconds: 1_500_000_000)
            restartRound()
        }
    }

    private func handleScored() {
        ballInFlight = false
        phase = .scored
        gkActive = false

        Task { @MainActor in
            try? await Task.sleep(nanoseconds: 1_500_000_000)
            startNewRound()
        }
    }

    private func restartRound() {
        ball.position    = ballStartPosition
        ballVelocity     = .zero
        aimDirection     = .zero
        shotPower        = 0.5
        phase            = .aiming
        goalkeeper.position = [0, 0, -5.9]
        clearDashes()
        updateCamera()
    }

    private func startNewRound() {
        let offsetX = Float.random(in: -1.0...1.0)
        ball.position    = [offsetX, ballRadius, 0]
        ballVelocity     = .zero
        aimDirection     = .zero
        shotPower        = 0.5
        phase            = .aiming
        goalkeeper.position = [0, 0, -5.9]
        clearDashes()
        updateCamera()
    }

    // MARK: - Private helpers

    private func buildLLMRequest() -> PenaltyShotRequest {
        PenaltyShotRequest(
            ballDistance: classifyDistance(),
            shotDirection: classifyDirection()
        )
    }

    private func classifyDistance() -> BallDistance {
        let d = simd_distance(ball.position, goalCenter)
        switch d {
        case ..<3.5:  return .close
        case ..<6.0:  return .medium
        default:      return .far
        }
    }

    private func classifyDirection() -> ShotDirection {
        // aimDirection: x > 0 = player's right (+X world), y > 0 = up
        let x = aimDirection.x
        let y = aimDirection.y

        let isLeft   = x < -0.25
        let isRight  = x >  0.25
        let isTop    = y >  0.25
        let isBottom = y < -0.25

        switch (isLeft, isRight, isTop, isBottom) {
        case (true,  false, true,  false): return .topLeft
        case (true,  false, false, false): return .leftCenter
        case (true,  false, false, true ): return .bottomLeft
        case (false, false, true,  false): return .topCenter
        case (false, false, false, true ): return .bottomCenter
        case (false, true,  true,  false): return .topRight
        case (false, true,  false, false): return .rightCenter
        case (false, true,  false, true ): return .bottomRight
        default:                           return .topCenter
        }
    }

    private func applyGoalkeeperResponse(_ response: GoalkeeperResponse) {
        let target = goalPositionFor(direction: response.jumpDirection)
        gkTargetPosition = target

        switch response.intensity {
        case "low":   gkIntensity = 0.3
        case "high":  gkIntensity = 1.0
        default:      gkIntensity = 0.6
        }

        gkActive = true
    }

    private func applyFallbackGoalkeeper() {
        let directions: [ShotDirection] = [.bottomLeft, .bottomRight, .topLeft, .topRight]
        let dir = directions.randomElement() ?? .bottomLeft
        gkTargetPosition = goalPositionFor(direction: dir)
        gkIntensity = 0.5
        gkActive = true
    }

    /// Maps goalkeeper jump direction to world-space position.
    ///
    /// The LLM responds using the GOALKEEPER's perspective (facing the player):
    ///   • "left"  = goalkeeper's left  = player's right = +X world space
    ///   • "right" = goalkeeper's right = player's left  = –X world space
    ///
    /// The goalkeeper container origin is at ground level (y = 0).
    private func goalPositionFor(direction: ShotDirection) -> SIMD3<Float> {
        let halfW = goalWidth / 2
        let z: Float = -5.9

        switch direction {
        case .topLeft, .leftCenter, .bottomLeft:
            return [-halfW * 0.75, 0, z]   // goalkeeper's left  = player's right = +X
        case .topCenter, .bottomCenter:
            return [0, 0, z]
        case .topRight, .rightCenter, .bottomRight:
            return [+halfW * 0.75, 0, z]   // goalkeeper's right = player's left  = –X
        }
    }

    private func launchBall() {
        let power = shotPower * 18.0 + 6.0    // 6…24 m/s
        let dx    = aimDirection.x * 3.0
        // Vertical: base lift of 2 m/s; full upward aim adds ~4.5 m/s,
        // calibrated so that top-of-goal is reachable at medium power.
        let dy    = max(0, aimDirection.y) * 4.5 + 2.0
        let dz    = -power

        ballVelocity  = SIMD3<Float>(dx, dy, dz)
        ballInFlight  = true
        phase         = .ballInFlight
    }

    private func checkGoalOrMiss() {
        let bx = ball.position.x
        let by = ball.position.y

        let inWidth  = abs(bx - goalCenter.x) < goalWidth / 2
        let inHeight = by > 0 && by < goalCenter.y + goalHeight / 2

        // Check if goalkeeper is blocking (against torso center)
        let gkTorsoPos = goalkeeper.position + SIMD3<Float>(0, gkTorsoCenterY, 0)
        let gkDist = simd_distance(ball.position, gkTorsoPos)
        let blocked = gkDist < (ballRadius + 0.55)

        if inWidth && inHeight && !blocked {
            handleScored()
        } else {
            handleBlocked()
        }
    }

    // MARK: - Trajectory dashes (physics-based arc)

    private func rebuildTrajectory() {
        clearDashes()
        guard simd_length(aimDirection) > 0.08 else { return }

        let power  = shotPower * 18.0 + 6.0
        let dx     = aimDirection.x * 3.0
        let dy     = max(0, aimDirection.y) * 4.5 + 2.0
        let dz     = -power
        var vel    = SIMD3<Float>(dx, dy, dz)
        var pos    = ball.position
        let simDt: Float = 0.09

        for i in 0..<14 {
            vel.y -= gravity * simDt
            pos   += vel * simDt
            guard pos.y >= ballRadius && pos.z >= goalCenter.z - 0.5 else { break }

            let alpha = Float(0.9) * (1.0 - Float(i) / 16.0)
            var dotMat = SimpleMaterial()
            // Blue and yellow color
            dotMat.color = .init(tint: UIColor(red: 1.0, green: 0.0, blue: 0.0, alpha: CGFloat(alpha)), texture: nil)
            let dotMesh = MeshResource.generateSphere(radius: 0.045)
            let dot = ModelEntity(mesh: dotMesh, materials: [dotMat])
            dot.position = pos

            root.addChild(dot)
            dashEntities.append(dot)
        }
    }

    private func clearDashes() {
        dashEntities.forEach { $0.removeFromParent() }
        dashEntities.removeAll()
    }

    // MARK: - Camera

    private func updateCamera() {
        let targetPos = ball.position
        let x = sin(cameraAngle) * cameraDistance
        let z = cos(cameraAngle) * cameraDistance
        let camPos = targetPos + SIMD3<Float>(x, 2.2, z)

        camera.position = camPos
        camera.look(at: targetPos, from: camPos, relativeTo: nil)
    }

    // MARK: - Scene builders

    /// Creates a grass field with alternating dark / light green stripes
    /// plus a white penalty box outline and centre spot.
    private static func buildGrassField() -> Entity {
        let container = Entity()

        let darkGreen  = UIColor(red: 0.10, green: 0.38, blue: 0.10, alpha: 1)
        let lightGreen = UIColor(red: 0.16, green: 0.52, blue: 0.16, alpha: 1)
        let stripeDepth: Float = 3.0
        let fieldWidth: Float  = 22.0
        let numStripes = 10

        for i in 0..<numStripes {
            let color = i % 2 == 0 ? darkGreen : lightGreen
            var mat = SimpleMaterial()
            mat.color = .init(tint: color, texture: nil)
            mat.roughness = 1.0
            mat.metallic  = 0.0
            let mesh   = MeshResource.generatePlane(width: fieldWidth, depth: stripeDepth)
            let stripe = ModelEntity(mesh: mesh, materials: [mat])
            // stripes: 0 at z=+0, 1 at z=-3, …, 9 at z=-27
            stripe.position = [0, 0.0, Float(i) * (-stripeDepth) + 1.5]
            container.addChild(stripe)
        }

        // Field markings in white
        addFieldMarkings(to: container, fieldWidth: fieldWidth)

        return container
    }

    /// Adds white penalty box and centre-spot markings as thin box entities.
    private static func addFieldMarkings(to container: Entity, fieldWidth: Float) {
        let lineMat: SimpleMaterial = {
            var m = SimpleMaterial()
            m.color = .init(tint: UIColor(white: 1, alpha: 0.80), texture: nil)
            m.roughness = 1.0
            return m
        }()
        let lineH: Float  = 0.02   // line sits just above the grass
        let lineT: Float  = 0.06   // line thickness (world units)

        // Penalty box: 7.32 m wide, extends ~5.5 m forward from goal line
        let boxW: Float = 8.0
        let boxDepth: Float = 5.5
        let goalLineZ: Float = -6.0
        let frontZ = goalLineZ + boxDepth  // ≈ -0.5

        // Goal line (x-axis at z = -6)
        addLine(to: container, mat: lineMat,
                size: [boxW + lineT, lineH, lineT],
                at: [0, lineH / 2, goalLineZ])

        // Front line of penalty box
        addLine(to: container, mat: lineMat,
                size: [boxW + lineT, lineH, lineT],
                at: [0, lineH / 2, frontZ])

        // Left side of penalty box
        addLine(to: container, mat: lineMat,
                size: [lineT, lineH, boxDepth],
                at: [-boxW / 2, lineH / 2, goalLineZ + boxDepth / 2])

        // Right side of penalty box
        addLine(to: container, mat: lineMat,
                size: [lineT, lineH, boxDepth],
                at: [boxW / 2, lineH / 2, goalLineZ + boxDepth / 2])

        // Penalty spot
        let spotMesh = MeshResource.generatePlane(width: 0.2, depth: 0.2)
        let spot = ModelEntity(mesh: spotMesh, materials: [lineMat])
        spot.position = [0, lineH / 2, -5.5]
        container.addChild(spot)
    }

    private static func addLine(to container: Entity,
                                mat: SimpleMaterial,
                                size: SIMD3<Float>,
                                at position: SIMD3<Float>) {
        let mesh   = MeshResource.generateBox(size: size)
        let entity = ModelEntity(mesh: mesh, materials: [mat])
        entity.position = position
        container.addChild(entity)
    }

    /// Builds a realistic soccer goal using cylinder posts and a visible net.
    private static func buildGoalPost(center: SIMD3<Float>,
                                      width: Float,
                                      height: Float) -> Entity {
        let container  = Entity()
        let postRadius: Float = 0.065
        let goalDepth: Float  = 1.5   // how deep the goal frame extends back

        // White metallic post material
        let postMat = SimpleMaterial(color: .white, roughness: 0.25, isMetallic: true)

        // Helper: vertical cylinder
        func vertPost(x: Float, z: Float) -> ModelEntity {
            let mesh = MeshResource.generateCylinder(height: height, radius: postRadius)
            let e = ModelEntity(mesh: mesh, materials: [postMat])
            e.position = [x, center.y, z]
            return e
        }

        // Helper: horizontal cylinder along X
        func horizBar(y: Float, z: Float, barWidth: Float) -> ModelEntity {
            let mesh = MeshResource.generateCylinder(height: barWidth, radius: postRadius)
            let e = ModelEntity(mesh: mesh, materials: [postMat])
            e.position = [center.x, y, z]
            e.orientation = simd_quatf(angle: .pi / 2, axis: [0, 0, 1])
            return e
        }

        // Helper: horizontal cylinder along Z (depth bar)
        func depthBar(x: Float, y: Float) -> ModelEntity {
            let mesh = MeshResource.generateCylinder(height: goalDepth, radius: postRadius)
            let e = ModelEntity(mesh: mesh, materials: [postMat])
            e.position = [x, y, center.z - goalDepth / 2]
            e.orientation = simd_quatf(angle: .pi / 2, axis: [1, 0, 0])
            return e
        }

        let lx = center.x - width / 2
        let rx = center.x + width / 2
        let topY = center.y + height / 2

        // Front left & right posts
        container.addChild(vertPost(x: lx, z: center.z))
        container.addChild(vertPost(x: rx, z: center.z))

        // Back left & right posts
        container.addChild(vertPost(x: lx, z: center.z - goalDepth))
        container.addChild(vertPost(x: rx, z: center.z - goalDepth))

        // Front crossbar
        container.addChild(horizBar(y: topY, z: center.z,            barWidth: width + postRadius * 2))
        // Back crossbar
        container.addChild(horizBar(y: topY, z: center.z - goalDepth, barWidth: width + postRadius * 2))

        // Top side bars (left and right, going into depth)
        container.addChild(depthBar(x: lx, y: topY))
        container.addChild(depthBar(x: rx, y: topY))

        // Bottom back bar
        container.addChild(horizBar(y: 0, z: center.z - goalDepth,   barWidth: width + postRadius * 2))

        // Net planes (semi-transparent white)
        var netMat = SimpleMaterial()
        netMat.color = .init(tint: UIColor(red: 0.15, green: 0.45, blue: 0.95, alpha: 0.18), texture: nil)
        // Back net
        let backNetMesh = MeshResource.generatePlane(width: width, depth: height)
        let backNet = ModelEntity(mesh: backNetMesh, materials: [netMat])
        backNet.orientation = simd_quatf(angle: .pi / 2, axis: [1, 0, 0])
        backNet.position = [center.x, center.y, center.z - goalDepth]
        container.addChild(backNet)

        // Top net (ceiling)
        let topNetMesh = MeshResource.generatePlane(width: width, depth: goalDepth)
        let topNet = ModelEntity(mesh: topNetMesh, materials: [netMat])
        topNet.position = [center.x, topY, center.z - goalDepth / 2]
        container.addChild(topNet)

        // Left side net
        let sideNetMesh = MeshResource.generatePlane(width: goalDepth, depth: height)
        let leftSideNet = ModelEntity(mesh: sideNetMesh, materials: [netMat])
        leftSideNet.orientation = simd_quatf(angle: .pi / 2, axis: [0, 1, 0]) *
                                   simd_quatf(angle: .pi / 2, axis: [1, 0, 0])
        leftSideNet.position = [lx, center.y, center.z - goalDepth / 2]
        container.addChild(leftSideNet)

        // Right side net
        let rightSideNet = ModelEntity(mesh: sideNetMesh, materials: [netMat])
        rightSideNet.orientation = simd_quatf(angle: -.pi / 2, axis: [0, 1, 0]) *
                                    simd_quatf(angle: .pi / 2, axis: [1, 0, 0])
        rightSideNet.position = [rx, center.y, center.z - goalDepth / 2]
        container.addChild(rightSideNet)

        return container
    }

    /// Builds a humanoid goalkeeper character from primitive shapes.
    /// The container origin is at ground level (y = 0) so that the entity can
    /// be moved purely in the XZ plane. Body parts are offset in Y.
    private static func buildHumanoidGoalkeeper() -> Entity {
        let container = Entity()

        // Jersey: bright orange (typical goalkeeper colour)
        let jerseyMat = SimpleMaterial(
            color: UIColor(red: 0.90, green: 0.55, blue: 0.05, alpha: 1),
            roughness: 0.5, isMetallic: false
        )
        // Shorts: dark navy
        let shortsMat = SimpleMaterial(
            color: UIColor(red: 0.05, green: 0.08, blue: 0.50, alpha: 1),
            roughness: 0.5, isMetallic: false
        )
        // Skin
        let skinMat = SimpleMaterial(
            color: UIColor(red: 0.94, green: 0.80, blue: 0.65, alpha: 1),
            roughness: 0.60, isMetallic: false
        )
        // Gloves: white
        let gloveMat = SimpleMaterial(
            color: UIColor(white: 0.92, alpha: 1),
            roughness: 0.55, isMetallic: false
        )
        // Boots: black
        let bootMat = SimpleMaterial(
            color: UIColor(white: 0.08, alpha: 1),
            roughness: 0.6, isMetallic: false
        )

        let legH: Float     = 0.52
        let legR: Float     = 0.075
        let legMesh         = MeshResource.generateCylinder(height: legH, radius: legR)
        let leftLeg         = ModelEntity(mesh: legMesh, materials: [shortsMat])
        leftLeg.position    = [-0.12, legH / 2, 0]
        let rightLeg        = ModelEntity(mesh: legMesh, materials: [shortsMat])
        rightLeg.position   = [ 0.12, legH / 2, 0]
        container.addChild(leftLeg)
        container.addChild(rightLeg)

        // Boots (small boxes at foot level)
        let bootMesh       = MeshResource.generateBox(size: [0.14, 0.10, 0.22])
        let leftBoot       = ModelEntity(mesh: bootMesh, materials: [bootMat])
        leftBoot.position  = [-0.12, 0.05, 0.04]
        let rightBoot      = ModelEntity(mesh: bootMesh, materials: [bootMat])
        rightBoot.position = [ 0.12, 0.05, 0.04]
        container.addChild(leftBoot)
        container.addChild(rightBoot)

        // Torso
        let torsoMesh     = MeshResource.generateBox(size: [0.44, 0.46, 0.22])
        let torso         = ModelEntity(mesh: torsoMesh, materials: [jerseyMat])
        torso.position    = [0, 0.77, 0]
        container.addChild(torso)

        // Neck
        let neckMesh      = MeshResource.generateCylinder(height: 0.12, radius: 0.07)
        let neck          = ModelEntity(mesh: neckMesh, materials: [skinMat])
        neck.position     = [0, 1.06, 0]
        container.addChild(neck)

        // Head
        let headMesh      = MeshResource.generateSphere(radius: 0.16)
        let head          = ModelEntity(mesh: headMesh, materials: [skinMat])
        head.position     = [0, 1.24, 0]
        container.addChild(head)

        // Arms (angled slightly outward — ready pose)
        let armH: Float   = 0.40
        let armR: Float   = 0.065
        let armMesh       = MeshResource.generateCylinder(height: armH, radius: armR)

        let leftArm       = ModelEntity(mesh: armMesh, materials: [jerseyMat])
        leftArm.position  = [-0.30, 0.88, 0]
        leftArm.orientation = simd_quatf(angle: .pi / 5.5, axis: [0, 0, 1])
        let rightArm      = ModelEntity(mesh: armMesh, materials: [jerseyMat])
        rightArm.position = [ 0.30, 0.88, 0]
        rightArm.orientation = simd_quatf(angle: -.pi / 5.5, axis: [0, 0, 1])
        container.addChild(leftArm)
        container.addChild(rightArm)

        // Gloves (at the tips of the arms)
        let gloveMesh       = MeshResource.generateSphere(radius: 0.09)
        let leftGlove       = ModelEntity(mesh: gloveMesh, materials: [gloveMat])
        leftGlove.position  = [-0.41, 0.62, 0]
        let rightGlove      = ModelEntity(mesh: gloveMesh, materials: [gloveMat])
        rightGlove.position = [ 0.41, 0.62, 0]
        container.addChild(leftGlove)
        container.addChild(rightGlove)

        return container
    }

    /// Adds a sky backdrop and simple crowd suggestion behind the goal.
    private static func buildEnvironment() -> Entity {
        let container = Entity()

        // Sky: large light-blue vertical plane far behind the goal
        var skyMat = SimpleMaterial()
        skyMat.color = .init(tint: UIColor(red: 0.53, green: 0.81, blue: 0.98, alpha: 1), texture: nil)
        skyMat.roughness = 1.0
        let skyMesh = MeshResource.generatePlane(width: 60, depth: 20)
        let sky = ModelEntity(mesh: skyMesh, materials: [skyMat])
        sky.orientation = simd_quatf(angle: .pi / 2, axis: [1, 0, 0])
        sky.position = [0, 10, -20]
        container.addChild(sky)

        // Crowd stand: two rows of coloured boxes behind the goal
        let rowColors: [UIColor] = [
            UIColor(red: 0.80, green: 0.15, blue: 0.15, alpha: 1),
            UIColor(red: 0.20, green: 0.50, blue: 0.80, alpha: 1),
            UIColor(red: 0.90, green: 0.85, blue: 0.10, alpha: 1),
            UIColor(red: 0.85, green: 0.85, blue: 0.85, alpha: 1),
            UIColor(red: 0.15, green: 0.65, blue: 0.20, alpha: 1),
        ]
        let seatSize: SIMD3<Float> = [1.8, 0.6, 0.5]

        for row in 0..<3 {
            for col in 0..<7 {
                let color = rowColors[(row + col) % rowColors.count]
                var mat = SimpleMaterial()
                mat.color = .init(tint: color, texture: nil)
                let mesh  = MeshResource.generateBox(size: seatSize)
                let seat  = ModelEntity(mesh: mesh, materials: [mat])
                seat.position = [
                    Float(col - 3) * 2.0,
                    0.8 + Float(row) * 0.8,
                    -9.0 - Float(row) * 0.6
                ]
                container.addChild(seat)
            }
        }

        // Side stands (left and right)
        let standColors: [UIColor] = [
            UIColor(red: 0.80, green: 0.15, blue: 0.15, alpha: 1),
            UIColor(red: 0.20, green: 0.50, blue: 0.80, alpha: 1),
        ]
        for side in [-1, 1] as [Float] {
            for row in 0..<2 {
                for col in 0..<5 {
                    let color = standColors[(row + col) % standColors.count]
                    var mat = SimpleMaterial()
                    mat.color = .init(tint: color, texture: nil)
                    let mesh  = MeshResource.generateBox(size: seatSize)
                    let seat  = ModelEntity(mesh: mesh, materials: [mat])
                    seat.position = [
                        side * (10.0 + Float(row) * 0.6),
                        0.6 + Float(row) * 0.7,
                        Float(col - 2) * 2.2 - 3.0
                    ]
                    container.addChild(seat)
                }
            }
        }

        return container
    }
}

