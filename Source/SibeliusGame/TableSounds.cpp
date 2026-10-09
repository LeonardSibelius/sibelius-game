// TableSounds.cpp — see the header.

#include "TableSounds.h"

#include "ProceduralPcm.h"

#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"
#include "Sound/SoundWaveProcedural.h"

// Named, not anonymous: the unity-build rule (ProceduralPcm.h tells the story).
namespace TableSoundsDetail
{
	// One sound: Duration seconds of Voice(t, Noise), where Noise is white noise already
	// passed through a one-pole low-pass at Cutoff Hz - raw white noise is a hiss, and
	// every one of these is a knock, not a hiss.
	template <typename TVoice>
	TArray<uint8> Build(float Duration, int32 Seed, float Cutoff, TVoice Voice)
	{
		TArray<uint8> Pcm;
		FRandomStream Rng(Seed);
		const int32 N = static_cast<int32>(SND_SR * Duration);
		Pcm.Reserve(N * 2);
		const float Alpha = 1.0f - FMath::Exp(-2.0f * PI * Cutoff / SND_SR);
		float Filtered = 0.0f;
		for (int32 I = 0; I < N; ++I)
		{
			const float T = static_cast<float>(I) / SND_SR;
			Filtered += Alpha * (Rng.FRandRange(-1.0f, 1.0f) - Filtered);
			// A 3 ms fade at the very end, so no sound stops on a click of its own.
			const float Tail = FMath::Clamp((Duration - T) / 0.003f, 0.0f, 1.0f);
			AppendSample(Pcm, Voice(T, Filtered) * Tail);
		}
		return Pcm;
	}

	inline float Sine(float Hz, float T) { return FMath::Sin(2.0f * PI * Hz * T); }
	inline float Decay(float Rate, float T) { return FMath::Exp(-Rate * T); }

	// A struck bell bar. Bars ring inharmonically - 1, 2.76, 5.40 times the fundamental -
	// which is what makes a chime sound like metal and not like an organ.
	TArray<uint8> Chime(float Hz, int32 Seed)
	{
		return Build(1.6f, Seed, 6000.0f, [Hz](float T, float N)
		{
			return 0.32f * Sine(Hz, T) * Decay(2.6f, T)
				+ 0.12f * Sine(Hz * 2.76f, T) * Decay(5.0f, T)
				+ 0.05f * Sine(Hz * 5.40f, T) * Decay(11.0f, T)
				+ 0.20f * N * Decay(400.0f, T);   // the striker
		});
	}

	// A knock: a low body tone, a noise burst for the impact, and a little ring.
	TArray<uint8> Knock(float Duration, int32 Seed, float BodyHz, float BodyDecay, float Snap, float RingHz, float Ring)
	{
		return Build(Duration, Seed, 3500.0f, [=](float T, float N)
		{
			return 0.55f * Sine(BodyHz, T) * Decay(BodyDecay, T)
				+ Snap * N * Decay(260.0f, T)
				+ Ring * Sine(RingHz, T) * Decay(45.0f, T);
		});
	}

	const TArray<uint8>& Get(ETableSound Sound)
	{
		static TArray<uint8> Bank[static_cast<int32>(ETableSound::Count)];
		const int32 Index = static_cast<int32>(Sound);
		check(Index >= 0 && Index < static_cast<int32>(ETableSound::Count));
		TArray<uint8>& Pcm = Bank[Index];
		if (Pcm.Num() > 0)
		{
			return Pcm;
		}

		switch (Sound)
		{
		// The three chimes of an EM machine: 10s high, 100s middle, 1000s low.
		case ETableSound::Chime10:     Pcm = Chime(1318.5f, 10); break;   // E6
		case ETableSound::Chime100:    Pcm = Chime(880.0f, 100); break;   // A5
		case ETableSound::Chime1000:   Pcm = Chime(587.3f, 1000); break;  // D5

		case ETableSound::Bumper:      Pcm = Knock(0.18f, 21, 140.0f, 38.0f, 0.55f, 2200.0f, 0.06f); break;
		case ETableSound::Slingshot:   Pcm = Knock(0.12f, 22, 300.0f, 55.0f, 0.70f, 1700.0f, 0.05f); break;
		case ETableSound::FlipperUp:
			// The coil's 120 Hz buzz under the clack, gone in a few hundredths.
			Pcm = Build(0.14f, 23, 3000.0f, [](float T, float N)
			{
				return 0.45f * Sine(210.0f, T) * Decay(60.0f, T)
					+ 0.60f * N * Decay(240.0f, T)
					+ 0.12f * FMath::Sign(Sine(120.0f, T)) * Decay(35.0f, T);
			});
			break;
		case ETableSound::FlipperDown: Pcm = Knock(0.09f, 24, 160.0f, 80.0f, 0.35f, 900.0f, 0.0f); break;
		case ETableSound::Plunger:
			// The thwack of the rod, then the spring ringing down a little in pitch.
			Pcm = Build(0.40f, 25, 2500.0f, [](float T, float N)
			{
				return 0.50f * N * Decay(90.0f, T)
					+ 0.40f * Sine(95.0f - 30.0f * T, T) * Decay(18.0f, T)
					+ 0.08f * Sine(720.0f + 25.0f * FMath::Sin(2.0f * PI * 9.0f * T), T) * Decay(9.0f, T);
			});
			break;
		case ETableSound::KickoutIn:   Pcm = Knock(0.22f, 26, 180.0f, 28.0f, 0.25f, 1200.0f, 0.04f); break;
		case ETableSound::Drain:
			// Down the outhole: two clunks as it drops, then gone.
			Pcm = Build(0.45f, 27, 1500.0f, [](float T, float N)
			{
				auto Clunk = [N](float U) { return U < 0.0f ? 0.0f : 0.5f * Sine(110.0f, U) * Decay(22.0f, U) + 0.3f * N * Decay(120.0f, U); };
				return Clunk(T) + 0.7f * Clunk(T - 0.13f);
			});
			break;
		case ETableSound::ReelClick:   Pcm = Knock(0.05f, 28, 900.0f, 120.0f, 0.45f, 2600.0f, 0.10f); break;

		case ETableSound::CueStrike:   Pcm = Knock(0.07f, 31, 260.0f, 90.0f, 0.50f, 1250.0f, 0.18f); break;
		case ETableSound::BallClick:
			// Phenolic resin: bright and very short, most of it above a kilohertz.
			Pcm = Build(0.07f, 32, 7000.0f, [](float T, float N)
			{
				return 0.40f * Sine(2850.0f, T) * Decay(170.0f, T)
					+ 0.22f * Sine(1450.0f, T) * Decay(110.0f, T)
					+ 0.35f * N * Decay(420.0f, T);
			});
			break;
		case ETableSound::CushionThud: Pcm = Knock(0.16f, 33, 95.0f, 32.0f, 0.30f, 400.0f, 0.0f); break;
		case ETableSound::PocketDrop:
			// A thud into the leather, and the rattle of it settling among the others.
			Pcm = Build(0.40f, 34, 3000.0f, [](float T, float N)
			{
				float S = 0.5f * Sine(125.0f, T) * Decay(20.0f, T) + 0.3f * N * Decay(80.0f, T);
				const float Rattles[3] = { 0.07f, 0.13f, 0.18f };
				for (int32 K = 0; K < 3; ++K)
				{
					const float U = T - Rattles[K];
					if (U >= 0.0f)
					{
						S += (0.22f - 0.05f * K) * Sine(1900.0f - 250.0f * K, U) * Decay(160.0f, U);
					}
				}
				return S;
			});
			break;
		case ETableSound::BowlRelease: Pcm = Knock(0.25f, 41, 70.0f, 18.0f, 0.35f, 300.0f, 0.0f); break;
		case ETableSound::BowlRoll:
			// A low rumble with the lane's boards ticking under it, swelling, then fading
			// out as the ball reaches the pins about two and a half seconds later.
			Pcm = Build(2.6f, 42, 260.0f, [](float T, float N)
			{
				const float Envelope = FMath::Min(T / 0.25f, 1.0f) * FMath::Clamp((2.6f - T) / 0.9f, 0.0f, 1.0f);
				const float Boards = 0.5f + 0.5f * FMath::Sin(2.0f * PI * 38.0f * T);
				return Envelope * (0.9f * N * (0.75f + 0.25f * Boards) + 0.12f * Sine(52.0f, T));
			});
			break;
		case ETableSound::PinClatter:
			// Maple pins: a hollow wooden knock, lower and longer than a pool ball's click.
			Pcm = Build(0.22f, 43, 5000.0f, [](float T, float N)
			{
				return 0.40f * Sine(820.0f, T) * Decay(40.0f, T)
					+ 0.25f * Sine(1330.0f, T) * Decay(55.0f, T)
					+ 0.18f * Sine(2210.0f, T) * Decay(80.0f, T)
					+ 0.45f * N * Decay(90.0f, T);
			});
			break;
		case ETableSound::ChessPlace:
			// Felt under the piece takes the snap off; what is left is the board's own tone.
			Pcm = Knock(0.10f, 51, 210.0f, 60.0f, 0.18f, 1050.0f, 0.05f);
			break;
		case ETableSound::ChessCapture:
			// Set down harder, and a moment later the taken piece lifted against its neighbour.
			Pcm = Build(0.20f, 52, 4200.0f, [](float T, float N)
			{
				const float Second = FMath::Max(0.0f, T - 0.055f);
				return 0.55f * Sine(230.0f, T) * Decay(55.0f, T) + 0.30f * N * Decay(180.0f, T)
					+ (T > 0.055f ? 0.30f * Sine(1480.0f, Second) * Decay(70.0f, Second) + 0.22f * N * Decay(220.0f, Second) : 0.0f);
			});
			break;
		case ETableSound::KartBeep:
			Pcm = Build(0.22f, 61, 6000.0f, [](float T, float N) { return 0.38f * Sine(660.0f, T) * Decay(9.0f, T) + 0.02f * N; });
			break;
		case ETableSound::KartGo:
			Pcm = Build(0.65f, 62, 6000.0f, [](float T, float N) { return 0.40f * Sine(1320.0f, T) * Decay(4.0f, T) + 0.15f * Sine(660.0f, T) * Decay(5.0f, T) + 0.02f * N; });
			break;
		case ETableSound::KartBump:   Pcm = Knock(0.20f, 63, 85.0f, 26.0f, 0.70f, 600.0f, 0.04f); break;
		case ETableSound::KartPickup:
			Pcm = Build(0.35f, 64, 7000.0f, [](float T, float N)
			{
				const float Second = FMath::Max(0.0f, T - 0.09f);
				return 0.30f * Sine(988.0f, T) * Decay(18.0f, T) + (T > 0.09f ? 0.32f * Sine(1480.0f, Second) * Decay(10.0f, Second) : 0.0f) + 0.03f * N * Decay(60.0f, T);
			});
			break;
		case ETableSound::KartBoost:
			// Air rushing past, its pitch climbing: noise through a rising band plus a low surge.
			Pcm = Build(0.80f, 65, 4500.0f, [](float T, float N)
			{
				const float Swell = FMath::Min(T / 0.08f, 1.0f) * FMath::Clamp((0.8f - T) / 0.5f, 0.0f, 1.0f);
				return Swell * (0.55f * N + 0.18f * Sine(110.0f + 260.0f * T, T));
			});
			break;
		case ETableSound::KartSlap:   Pcm = Knock(0.16f, 66, 320.0f, 40.0f, 0.95f, 2400.0f, 0.06f); break;
		case ETableSound::KartChest:
			Pcm = Build(0.90f, 67, 8000.0f, [](float T, float N)
			{
				const float Fall = 1800.0f - 1300.0f * FMath::Min(T / 0.6f, 1.0f);
				const float Thud = T > 0.6f ? 0.55f * Sine(70.0f, T - 0.6f) * Decay(14.0f, T - 0.6f) + 0.3f * N * Decay(40.0f, T - 0.6f) : 0.0f;
				return 0.22f * Sine(Fall, T) * Decay(2.5f, T) + 0.12f * Sine(Fall * 1.5f, T) * Decay(3.5f, T) + Thud;
			});
			break;
		case ETableSound::KartLap:
			Pcm = Build(0.75f, 68, 7000.0f, [](float T, float)
			{
				float S = 0.0f;
				const float Notes[3] = { 784.0f, 988.0f, 1175.0f };
				for (int32 K = 0; K < 3; ++K)
				{
					const float U = T - 0.11f * K;
					S += U >= 0.0f ? 0.26f * Sine(Notes[K], U) * Decay(6.0f, U) : 0.0f;
				}
				return S;
			});
			break;
		case ETableSound::KartFinish:
			Pcm = Build(1.80f, 69, 7000.0f, [](float T, float)
			{
				float S = 0.0f;
				const float Notes[5] = { 523.3f, 659.3f, 784.0f, 1046.5f, 1318.5f };
				for (int32 K = 0; K < 5; ++K)
				{
					const float U = T - 0.13f * K;
					const float Hold = K == 4 ? 1.6f : 4.5f;
					S += U >= 0.0f ? 0.20f * (Sine(Notes[K], U) + 0.3f * Sine(Notes[K] * 2.0f, U)) * Decay(Hold, U) : 0.0f;
				}
				return S;
			});
			break;
		default:
			break;
		}
		return Pcm;
	}

	// Half a second of engine: a 60 Hz two-stroke - a buzzy fundamental, its harmonics, and
	// the firing pulses as a 30 Hz throb - every component a whole number of cycles in half a
	// second, so the loop's seam is silent. The noise is left out for the same reason.
	const TArray<uint8>& EngineLoop()
	{
		static TArray<uint8> Pcm;
		if (Pcm.Num() > 0)
		{
			return Pcm;
		}
		const int32 N = SND_SR / 2;
		Pcm.Reserve(N * 2);
		for (int32 I = 0; I < N; ++I)
		{
			const float T = static_cast<float>(I) / SND_SR;
			const float Throb = 0.75f + 0.25f * Sine(30.0f, T);
			const float Buzz = 0.30f * Sine(60.0f, T) + 0.18f * Sine(120.0f, T) + 0.12f * Sine(180.0f, T)
				+ 0.07f * Sine(240.0f, T) + 0.05f * Sine(360.0f, T);
			AppendSample(Pcm, 0.55f * Buzz * Throb);
		}
		return Pcm;
	}
}

USoundWaveProcedural* TableSounds::MakeKartEngine()
{
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
	Wave->SetSampleRate(SND_SR);
	Wave->NumChannels = 1;
	// Unreal's INDEFINITELY_LOOPING_DURATION, the length it gives a sound that never ends.
	// Not more: the mixer multiplies it by the DEVICE's rate (48 kHz) into an int32 frame
	// count, and 100000 s overflowed that and asserted (NumTotalFrames > 0) the moment the
	// engine started - the first time Walt got into a kart, 2026-10-09.
	Wave->Duration = 10000.0f;
	Wave->SoundGroup = SOUNDGROUP_Effects;
	Wave->bLooping = false;
	const TArray<uint8>& Loop = TableSoundsDetail::EngineLoop();
	Wave->QueueAudio(Loop.GetData(), Loop.Num());
	// Called on the audio thread when it runs low: the static loop is read-only by then.
	Wave->OnSoundWaveProceduralUnderflow = FOnSoundWaveProceduralUnderflow::CreateLambda([](USoundWaveProcedural* InWave, int32)
	{
		const TArray<uint8>& Again = TableSoundsDetail::EngineLoop();
		InWave->QueueAudio(Again.GetData(), Again.Num());
	});
	return Wave;
}

void TableSounds::Play(const UObject* WorldContext, ETableSound Sound, float Volume, float Pitch)
{
	if (!WorldContext || !WorldContext->GetWorld() || Volume <= 0.001f)
	{
		return;
	}
	const TArray<uint8>& Pcm = TableSoundsDetail::Get(Sound);
	if (Pcm.Num() == 0)
	{
		return;
	}
	// The poker machine's recipe: a fresh procedural wave per play, queued whole.
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(GetTransientPackage());
	Wave->SetSampleRate(SND_SR);
	Wave->NumChannels = 1;
	Wave->Duration = Pcm.Num() / (2.0f * SND_SR);
	Wave->SoundGroup = SOUNDGROUP_Effects;
	Wave->bLooping = false;
	Wave->QueueAudio(Pcm.GetData(), Pcm.Num());
	UGameplayStatics::PlaySound2D(WorldContext, Wave, FMath::Clamp(Volume, 0.0f, 1.5f), Pitch);
}
