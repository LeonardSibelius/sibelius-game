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
		default:
			break;
		}
		return Pcm;
	}
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
