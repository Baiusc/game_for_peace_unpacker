#pragma once

// --- Entity Overlay System ---
namespace Visuals {
bool Initialize();
void CalculateESP(); // Background loop
void DrawOverlay();
void DrawFOVCircle();
void DrawCrosshair();
void DrawWatermark();
void DrawSafeModeIndicator();
void DrawInjectionNotification();
void TriggerNotification();
void DrawGrenadeTrajectory();
void DrawFloatingMascot();
void Cleanup();
} // namespace Visuals
