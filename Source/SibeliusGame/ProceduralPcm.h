// ProceduralPcm.h
//
// The slot machine and the poker machine both synthesise their sounds as raw PCM at
// runtime, and both files carried their own BYTE-IDENTICAL copy of these two helpers
// inside an ANONYMOUS namespace.
//
// That is perfectly legal while they are separate translation units — and a hard
// redefinition error the moment UE's unity build packs both .cpp files into the same
// Module.SibeliusGame.N.cpp blob, because the two anonymous namespaces then ARE the
// same namespace.
//
// Which is exactly what happened on 2026-08-03: adding DancerAgentComponent.cpp and
// DancerAgentSubsystem.cpp shifted the blob boundaries and dropped SlotScreenWidget.cpp
// next to PokerScreenWidget.cpp. Nothing was wrong with either file — the break was
// always latent, waiting for ANY new .cpp to land in this module and renumber the
// blobs. One shared definition removes the trap permanently.
//
// constexpr gives the constant internal linkage and inline does the same for the
// function, so including this anywhere is safe.

#pragma once

#include "CoreMinimal.h"

/** Procedural PCM sample rate — mono, signed 16-bit, little-endian. */
constexpr int32 SND_SR = 22050;

/** Append one float sample, clamped to [-1,1], to a mono s16 PCM buffer. */
inline void AppendSample(TArray<uint8>& Pcm, float S)
{
	const int16 V = static_cast<int16>(FMath::Clamp(S, -1.0f, 1.0f) * 32767.0f);
	Pcm.Add(static_cast<uint8>(V & 0xFF));
	Pcm.Add(static_cast<uint8>((V >> 8) & 0xFF));
}
