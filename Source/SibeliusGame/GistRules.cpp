// GistRules.cpp — see the header for what THE GIST is and why it is engine-free.

#include "GistRules.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace Gist
{
namespace GistDetail
{
constexpr float GistPi = 3.14159265f;

const FColour Red{0.85f, 0.15f, 0.12f};
const FColour Orange{0.95f, 0.5f, 0.1f};
const FColour Yellow{0.98f, 0.85f, 0.2f};
const FColour Green{0.25f, 0.7f, 0.3f};
const FColour DarkGreen{0.1f, 0.45f, 0.2f};
const FColour Blue{0.2f, 0.45f, 0.9f};
const FColour LightBlue{0.55f, 0.8f, 0.98f};
const FColour Purple{0.55f, 0.3f, 0.8f};
const FColour Pink{0.98f, 0.55f, 0.7f};
const FColour White{0.95f, 0.95f, 0.92f};
const FColour Black{0.06f, 0.06f, 0.07f};
const FColour Grey{0.55f, 0.55f, 0.58f};
const FColour Brown{0.5f, 0.3f, 0.15f};
const FColour Tan{0.85f, 0.7f, 0.5f};
const FColour Cream{0.98f, 0.92f, 0.75f};
const FColour Silver{0.75f, 0.77f, 0.8f};
const FColour GoldTone{1.0f, 0.75f, 0.25f};
const FColour Palette[] = {Red, Orange, Yellow, Green, Blue, LightBlue, Purple, Pink, White, Tan};
constexpr int PaletteSize = static_cast<int>(sizeof(Palette) / sizeof(Palette[0]));

FVec Add(const FVec& A, const FVec& B) { return {A.X + B.X, A.Y + B.Y, A.Z + B.Z}; }
FVec Mul(const FVec& A, float S) { return {A.X * S, A.Y * S, A.Z * S}; }

FColour Mix(const FColour& A, const FColour& B, float T)
{
	return {A.R + (B.R - A.R) * T, A.G + (B.G - A.G) * T, A.B + (B.B - A.B) * T};
}

FColour Hue(float H)
{
	// A bright toy hue around the wheel, H in [0,1).
	const float R = 0.5f + 0.45f * std::cos(2 * GistPi * H);
	const float G = 0.5f + 0.45f * std::cos(2 * GistPi * (H - 1.0f / 3));
	const float B = 0.5f + 0.45f * std::cos(2 * GistPi * (H - 2.0f / 3));
	return {R, G, B};
}

// Unreal's FRotationMatrix: the part's X, Y and Z axes in world space, so the bounds and
// placement here agree with what the engine draws.
FVec Rotate(const FVec& Rot, const FVec& V)
{
	const float P = Rot.X * GistPi / 180, Yw = Rot.Y * GistPi / 180, R = Rot.Z * GistPi / 180;
	const float SP = std::sin(P), CP = std::cos(P), SY = std::sin(Yw), CY = std::cos(Yw), SR = std::sin(R), CR = std::cos(R);
	const FVec AX{CP * CY, CP * SY, SP};
	const FVec AY{SR * SP * CY - CR * SY, SR * SP * SY + CR * CY, -SR * CP};
	const FVec AZ{-(CR * SP * CY + SR * SY), CY * SR - CR * SP * SY, CR * CP};
	return Add(Add(Mul(AX, V.X), Mul(AY, V.Y)), Mul(AZ, V.Z));
}

// A tiny xorshift generator. Seeded from the words, so it is part of "same words, same gist".
struct FRng
{
	uint32_t S;
	explicit FRng(uint32_t Seed) : S(Seed ? Seed : 0x9E3779B9u) {}
	uint32_t Next() { S ^= S << 13; S ^= S >> 17; S ^= S << 5; return S; }
	float Unit() { return static_cast<float>(Next() >> 8) * (1.0f / 16777216.0f); }
	int Below(int N) { return N > 0 ? static_cast<int>(Next() % static_cast<uint32_t>(N)) : 0; }
};

// A recipe under construction, with the anchors decorations need.
struct FKit
{
	std::vector<FPart> Parts;
	FVec FaceAt;               // between the eyes, on the front surface
	float EyeGap = 10, EyeSize = 8;
	FVec TopAt;                // where a hat goes; raised by everything stacked there
	float HeadSize = 30;       // how wide that spot is
	FColour Signature = Grey;  // what "made of X" and a hybrid borrow from X
	bool bHasWings = false;

	void Put(EShape Shape, const FColour& C, FVec At, FVec Size, FVec Rot = FVec{}, ERole Role = ERole::Body, EFinish Finish = EFinish::Clay)
	{
		FPart P;
		P.Shape = Shape; P.Colour = C; P.Location = At; P.Size = Size; P.Rotation = Rot; P.Role = Role; P.Finish = Finish;
		Parts.push_back(P);
	}
	void Box(const FColour& C, FVec At, FVec Size, FVec Rot = FVec{}) { Put(EShape::Cube, C, At, Size, Rot); }
	void Ball(const FColour& C, FVec At, FVec Size, FVec Rot = FVec{}) { Put(EShape::Sphere, C, At, Size, Rot); }
	void Tube(const FColour& C, FVec At, FVec Size, FVec Rot = FVec{}) { Put(EShape::Cylinder, C, At, Size, Rot); }
	void Spike(const FColour& C, FVec At, FVec Size, FVec Rot = FVec{}) { Put(EShape::Cone, C, At, Size, Rot); }
	void Face(FVec At, float Gap, float Eye) { FaceAt = At; EyeGap = Gap; EyeSize = Eye; }
	void Top(FVec At, float Head) { TopAt = At; HeadSize = Head; }
};

// ---- the 40 archetypes: toy recipes facing +X, standing on Z = 0 ---------------------------

void Cat(FKit& K)
{
	K.Signature = Orange;
	K.Ball(Orange, {0, 0, 30}, {60, 34, 34});
	K.Ball(Orange, {34, 0, 52}, {30, 30, 30});
	K.Spike(Orange, {34, -9, 70}, {10, 10, 14});
	K.Spike(Orange, {34, 9, 70}, {10, 10, 14});
	K.Tube(Orange, {-36, 0, 48}, {6, 6, 40}, {30, 0, 0});
	for (const float X : {-18.0f, 18.0f})
		for (const float Y : {-10.0f, 10.0f}) K.Tube(Orange, {X, Y, 8}, {8, 8, 16});
	K.Face({49, 0, 56}, 12, 9);
	K.Top({34, 0, 67}, 26);
}

void Dog(FKit& K)
{
	K.Signature = Brown;
	K.Ball(Brown, {0, 0, 32}, {62, 32, 32});
	K.Ball(Brown, {36, 0, 54}, {30, 30, 30});
	K.Box(Tan, {54, 0, 50}, {16, 14, 12});
	K.Ball(Black, {63, 0, 53}, {6, 6, 6});
	K.Box(Brown, {34, -17, 56}, {12, 4, 18}, {0, 0, 20});
	K.Box(Brown, {34, 17, 56}, {12, 4, 18}, {0, 0, -20});
	K.Tube(Brown, {-36, 0, 48}, {6, 6, 30}, {35, 0, 0});
	for (const float X : {-18.0f, 18.0f})
		for (const float Y : {-10.0f, 10.0f}) K.Tube(Brown, {X, Y, 9}, {9, 9, 18});
	K.Face({50, 0, 60}, 12, 9);
	K.Top({36, 0, 69}, 26);
}

void Bird(FKit& K)
{
	K.Signature = Blue; K.bHasWings = true;
	K.Ball(Blue, {0, 0, 30}, {36, 30, 30});
	K.Ball(Blue, {16, 0, 50}, {20, 20, 20});
	K.Spike(Yellow, {29, 0, 50}, {8, 8, 12}, {-90, 0, 0});
	K.Ball(LightBlue, {0, -17, 32}, {24, 6, 14}, {0, 0, 20});
	K.Ball(LightBlue, {0, 17, 32}, {24, 6, 14}, {0, 0, -20});
	K.Box(Blue, {-20, 0, 34}, {14, 10, 4}, {20, 0, 0});
	K.Tube(Orange, {0, -6, 8}, {3, 3, 16});
	K.Tube(Orange, {0, 6, 8}, {3, 3, 16});
	K.Face({24, 0, 54}, 9, 7);
	K.Top({16, 0, 60}, 16);
}

void Fish(FKit& K)
{
	K.Signature = Orange;
	K.Ball(Orange, {0, 0, 22}, {60, 20, 34});
	K.Spike(Orange, {-36, 0, 22}, {26, 6, 24}, {90, 0, 0});
	K.Box(Yellow, {0, 0, 40}, {20, 3, 10}, {-20, 0, 0});
	K.Box(Yellow, {6, -11, 18}, {12, 3, 6}, {0, 0, 30});
	K.Box(Yellow, {6, 11, 18}, {12, 3, 6}, {0, 0, -30});
	K.Face({24, 0, 28}, 10, 8);
	K.Top({4, 0, 39}, 20);
}

void Dragon(FKit& K)
{
	K.Signature = Green; K.bHasWings = true;
	K.Ball(Green, {0, 0, 45}, {80, 44, 44});
	K.Tube(Green, {38, 0, 74}, {16, 16, 50}, {-30, 0, 0});
	K.Box(Green, {58, 0, 100}, {34, 24, 20});
	K.Spike(Cream, {52, -8, 116}, {6, 6, 16});
	K.Spike(Cream, {52, 8, 116}, {6, 6, 16});
	K.Box(DarkGreen, {0, -34, 78}, {44, 4, 30}, {0, 0, 35});
	K.Box(DarkGreen, {0, 34, 78}, {44, 4, 30}, {0, 0, -35});
	K.Spike(Green, {-56, 0, 40}, {16, 16, 50}, {70, 0, 0});
	for (const float X : {-24.0f, 24.0f})
		for (const float Y : {-16.0f, 16.0f}) K.Tube(DarkGreen, {X, Y, 12}, {14, 14, 24});
	K.Face({75, 0, 104}, 12, 9);
	K.Top({58, 0, 110}, 24);
}

void Horse(FKit& K)
{
	K.Signature = Brown;
	K.Ball(Brown, {0, 0, 70}, {80, 34, 40});
	K.Tube(Brown, {36, 0, 94}, {16, 16, 40}, {-35, 0, 0});
	K.Box(Brown, {52, 0, 114}, {34, 16, 18});
	K.Spike(Brown, {44, -5, 128}, {6, 6, 12});
	K.Spike(Brown, {44, 5, 128}, {6, 6, 12});
	K.Box(Black, {30, 0, 104}, {28, 4, 12}, {-35, 0, 0});
	K.Tube(Black, {-44, 0, 70}, {6, 6, 40}, {40, 0, 0});
	for (const float X : {-30.0f, 30.0f})
		for (const float Y : {-12.0f, 12.0f}) K.Tube(Brown, {X, Y, 25}, {10, 10, 50});
	K.Face({62, 0, 118}, 10, 8);
	K.Top({50, 0, 123}, 18);
}

void Duck(FKit& K)
{
	K.Signature = Yellow; K.bHasWings = true;
	K.Ball(Yellow, {0, 0, 18}, {40, 30, 26});
	K.Ball(Yellow, {18, 0, 38}, {22, 22, 22});
	K.Box(Orange, {32, 0, 36}, {14, 12, 5});
	K.Spike(Yellow, {-22, 0, 24}, {10, 10, 12}, {60, 0, 0});
	K.Ball(Yellow, {0, -14, 20}, {22, 6, 12});
	K.Ball(Yellow, {0, 14, 20}, {22, 6, 12});
	K.Face({27, 0, 43}, 9, 7);
	K.Top({18, 0, 49}, 18);
}

void Snake(FKit& K)
{
	K.Signature = Green;
	const float Xs[] = {-48, -30, -12, 6, 24};
	const float Ys[] = {-8, 6, -6, 6, -4};
	for (int I = 0; I < 5; ++I)
	{
		const float D = 14.0f + static_cast<float>(I);
		K.Ball(I % 2 ? DarkGreen : Green, {Xs[I], Ys[I], D * 0.5f}, {D, D, D});
	}
	K.Ball(Green, {42, 0, 11}, {20, 18, 18});
	K.Box(Red, {56, 0, 8}, {10, 2, 1});
	K.Face({50, 0, 16}, 8, 7);
	K.Top({42, 0, 20}, 16);
}

void Robot(FKit& K)
{
	K.Signature = Silver;
	K.Box(Grey, {0, -12, 20}, {14, 14, 40});
	K.Box(Grey, {0, 12, 20}, {14, 14, 40});
	K.Box(Silver, {0, 0, 65}, {40, 46, 50});
	K.Box(Silver, {0, 0, 105}, {30, 32, 28});
	K.Box(Grey, {0, -30, 68}, {10, 10, 40});
	K.Box(Grey, {0, 30, 68}, {10, 10, 40});
	K.Tube(Grey, {0, 0, 126}, {3, 3, 14});
	K.Put(EShape::Sphere, Red, {0, 0, 135}, {7, 7, 7}, FVec{}, ERole::Body, EFinish::Glass);
	K.Face({15, 0, 108}, 14, 10);
	K.Top({0, 0, 119}, 26);
}

void Person(FKit& K)
{
	K.Signature = Red;
	K.Tube(Blue, {0, -8, 25}, {12, 12, 50});
	K.Tube(Blue, {0, 8, 25}, {12, 12, 50});
	K.Box(Red, {0, 0, 72}, {24, 36, 44});
	K.Ball(Tan, {0, 0, 107}, {26, 26, 26});
	K.Tube(Tan, {0, -23, 72}, {9, 9, 42});
	K.Tube(Tan, {0, 23, 72}, {9, 9, 42});
	K.Box(Brown, {-2, 0, 119}, {24, 26, 8});
	K.Face({12, 0, 109}, 9, 7);
	K.Top({0, 0, 123}, 24);
}

void Car(FKit& K)
{
	K.Signature = Red;
	K.Box(Red, {0, 0, 32}, {120, 60, 30});
	K.Box(LightBlue, {-10, 0, 59}, {60, 54, 26});
	for (const float X : {-40.0f, 40.0f})
		for (const float Y : {-32.0f, 32.0f}) K.Tube(Black, {X, Y, 16}, {32, 32, 12}, {0, 0, 90});
	K.Face({60, 0, 36}, 36, 14);   // headlights, googly
	K.Top({-10, 0, 72}, 50);
}

void Boat(FKit& K)
{
	K.Signature = Brown;
	K.Box(Brown, {0, 0, 14}, {100, 44, 26});
	K.Spike(Brown, {60, 0, 14}, {26, 44, 26}, {-90, 0, 0});
	K.Tube(Brown, {-5, 0, 60}, {5, 5, 70});
	K.Box(White, {6, 0, 64}, {3, 40, 46});
	K.Face({50, 0, 20}, 20, 10);
	K.Top({-5, 0, 95}, 14);
}

void Rocket(FKit& K)
{
	K.Signature = White; K.bHasWings = true;
	K.Tube(White, {0, 0, 80}, {36, 36, 110});
	K.Spike(Red, {0, 0, 150}, {36, 36, 30});
	for (int I = 0; I < 3; ++I)
	{
		const float A = I * 120.0f;
		K.Box(Red, {std::cos(A * GistPi / 180) * 20, std::sin(A * GistPi / 180) * 20, 36}, {22, 4, 30}, {0, A, 0});
	}
	K.Put(EShape::Cone, Orange, {0, 0, 14}, {24, 24, 24}, {180, 0, 0}, ERole::Body, EFinish::Glass);
	K.Face({18, 0, 98}, 12, 9);
	K.Top({0, 0, 165}, 12);
}

void Train(FKit& K)
{
	K.Signature = Green;
	K.Box(Green, {0, 0, 44}, {90, 50, 50});
	K.Box(DarkGreen, {-30, 0, 86}, {36, 54, 36});
	K.Tube(Black, {28, 0, 82}, {16, 16, 26});
	for (const float X : {-30.0f, 0.0f, 30.0f})
		for (const float Y : {-27.0f, 27.0f}) K.Tube(Black, {X, Y, 14}, {24, 24, 8}, {0, 0, 90});
	K.Face({46, 0, 48}, 22, 12);
	K.Top({-30, 0, 104}, 30);
}

void Plane(FKit& K)
{
	K.Signature = White; K.bHasWings = true;
	K.Tube(White, {0, 0, 40}, {30, 30, 140}, {-90, 0, 0});
	K.Spike(Grey, {78, 0, 40}, {30, 30, 18}, {-90, 0, 0});
	K.Box(Blue, {0, 0, 40}, {36, 150, 5});
	K.Box(Blue, {-60, 0, 62}, {24, 4, 36});
	K.Box(Blue, {-62, 0, 44}, {18, 56, 4});
	K.Tube(Black, {20, 0, 12}, {12, 12, 6}, {0, 0, 90});
	K.Face({68, 0, 48}, 12, 9);
	K.Top({10, 0, 55}, 24);
}

void Toaster(FKit& K)
{
	K.Signature = Silver;
	K.Box(Silver, {0, 0, 30}, {50, 34, 40});
	K.Box(Black, {0, -7, 50}, {36, 6, 2});
	K.Box(Black, {0, 7, 50}, {36, 6, 2});
	K.Box(Tan, {0, -7, 56}, {30, 4, 22});
	K.Box(Tan, {0, 7, 56}, {30, 4, 22});
	K.Box(Black, {0, -18, 36}, {6, 4, 6});
	for (const float X : {-18.0f, 18.0f})
		for (const float Y : {-12.0f, 12.0f}) K.Ball(Black, {X, Y, 5}, {8, 8, 10});
	K.Face({25, 0, 32}, 18, 10);
	K.Top({0, 0, 67}, 30);
}

void Cake(FKit& K)
{
	K.Signature = Pink;
	K.Tube(Pink, {0, 0, 15}, {70, 70, 30});
	K.Tube(Cream, {0, 0, 42}, {50, 50, 24});
	K.Tube(Pink, {0, 0, 62}, {30, 30, 16});
	K.Tube(Yellow, {0, 0, 77}, {4, 4, 14});
	K.Put(EShape::Sphere, Orange, {0, 0, 87}, {5, 5, 7}, FVec{}, ERole::Body, EFinish::Glass);
	K.Face({35, 0, 17}, 22, 10);
	K.Top({0, 0, 90}, 20);
}

void Pizza(FKit& K)
{
	K.Signature = Tan;
	K.Tube(Tan, {0, 0, 2}, {70, 70, 4});
	K.Tube(Red, {0, 0, 5}, {62, 62, 2});
	const float Pepperoni[][2] = {{12, 10}, {-14, 6}, {0, -16}, {-6, 20}, {18, -12}};
	for (const auto& P : Pepperoni) K.Tube(Red, {P[0], P[1], 7}, {10, 10, 2});
	K.Box(Yellow, {-4, -4, 7}, {14, 10, 1});
	K.Face({28, 0, 12}, 18, 12);
	K.Top({0, 0, 8}, 30);
}

void Tree(FKit& K)
{
	K.Signature = Green;
	K.Tube(Brown, {0, 0, 40}, {18, 18, 80});
	K.Ball(Green, {0, 0, 100}, {70, 70, 60});
	K.Ball(Green, {-15, 12, 125}, {50, 50, 44});
	K.Ball(DarkGreen, {15, -10, 128}, {44, 44, 40});
	K.Face({9, 0, 50}, 8, 7);
	K.Top({0, 0, 148}, 30);
}

void House(FKit& K)
{
	K.Signature = Cream;
	K.Box(Cream, {0, 0, 50}, {110, 90, 100});
	K.Spike(Red, {0, 0, 125}, {140, 120, 50});
	K.Box(Brown, {55, 0, 26}, {2, 26, 48});
	K.Box(Brown, {-20, -30, 130}, {14, 14, 30});
	K.Face({56, 0, 70}, 60, 18);   // the windows, googly
	K.Top({0, 0, 150}, 30);
}

void Castle(FKit& K)
{
	K.Signature = Grey;
	K.Box(Grey, {0, 0, 50}, {140, 140, 100});
	for (const float X : {-70.0f, 70.0f})
		for (const float Y : {-70.0f, 70.0f})
		{
			K.Tube(Grey, {X, Y, 70}, {40, 40, 140});
			K.Spike(Red, {X, Y, 155}, {46, 46, 30});
		}
	K.Box(Brown, {71, 0, 25}, {2, 40, 50});
	K.Face({72, 0, 75}, 50, 18);
	K.Top({0, 0, 100}, 60);
}

void Tower(FKit& K)
{
	K.Signature = Grey;
	K.Tube(Grey, {0, 0, 10}, {80, 80, 20});
	K.Tube(Grey, {0, 0, 110}, {60, 60, 200});
	K.Spike(Blue, {0, 0, 230}, {70, 70, 40});
	K.Face({31, 0, 170}, 18, 12);
	K.Top({0, 0, 250}, 20);
}

void Bridge(FKit& K)
{
	K.Signature = Grey;
	K.Box(Grey, {0, -50, 25}, {60, 20, 50});
	K.Box(Grey, {0, 50, 25}, {60, 20, 50});
	K.Box(Brown, {0, 0, 56}, {60, 140, 12});
	K.Box(Brown, {-28, 0, 68}, {4, 140, 12});
	K.Box(Brown, {28, 0, 68}, {4, 140, 12});
	K.Face({31, 0, 58}, 40, 12);
	K.Top({0, 0, 62}, 40);
}

void Chair(FKit& K)
{
	K.Signature = Brown;
	K.Box(Brown, {0, 0, 45}, {50, 50, 8});
	K.Box(Brown, {-22, 0, 75}, {6, 50, 56});
	for (const float X : {-20.0f, 20.0f})
		for (const float Y : {-20.0f, 20.0f}) K.Tube(Brown, {X, Y, 21}, {5, 5, 42});
	K.Face({-18, 0, 86}, 16, 10);
	K.Top({-22, 0, 103}, 20);
}

void Table(FKit& K)
{
	K.Signature = Brown;
	K.Box(Brown, {0, 0, 74}, {100, 70, 6});
	for (const float X : {-42.0f, 42.0f})
		for (const float Y : {-28.0f, 28.0f}) K.Tube(Brown, {X, Y, 36}, {6, 6, 72});
	K.Face({48, 0, 82}, 30, 12);
	K.Top({0, 0, 77}, 40);
}

void Bed(FKit& K)
{
	K.Signature = Blue;
	K.Box(Brown, {0, 0, 25}, {190, 100, 30});
	K.Box(White, {0, 0, 45}, {184, 94, 14});
	K.Box(White, {-75, 0, 56}, {28, 60, 12});
	K.Box(Blue, {20, 0, 53}, {140, 96, 4});
	K.Box(Brown, {-95, 0, 55}, {6, 100, 70});
	K.Face({-91, 0, 80}, 30, 14);
	K.Top({-95, 0, 90}, 30);
}

void Lamp(FKit& K)
{
	K.Signature = Cream;
	K.Tube(Black, {0, 0, 4}, {36, 36, 8});
	K.Tube(Black, {0, 0, 70}, {5, 5, 130});
	K.Put(EShape::Sphere, Yellow, {0, 0, 126}, {14, 14, 14}, FVec{}, ERole::Body, EFinish::Glass);
	K.Spike(Cream, {0, 0, 140}, {50, 50, 36});
	K.Face({14, 0, 140}, 10, 8);
	K.Top({0, 0, 158}, 14);
}

void Hat(FKit& K)
{
	K.Signature = Black;
	K.Tube(Black, {0, 0, 2}, {60, 60, 4});
	K.Tube(Black, {0, 0, 24}, {36, 36, 40});
	K.Tube(Red, {0, 0, 9}, {37, 37, 6});
	K.Face({19, 0, 26}, 12, 9);
	K.Top({0, 0, 44}, 30);
}

void Crown(FKit& K)
{
	K.Signature = GoldTone;
	K.Put(EShape::Cylinder, GoldTone, {0, 0, 10}, {40, 40, 20}, FVec{}, ERole::Body, EFinish::Gold);
	for (int I = 0; I < 5; ++I)
	{
		const float A = I * 72.0f * GistPi / 180;
		K.Put(EShape::Cone, GoldTone, {std::cos(A) * 17, std::sin(A) * 17, 26}, {8, 8, 14}, FVec{}, ERole::Body, EFinish::Gold);
	}
	K.Face({20, 0, 10}, 14, 8);
	K.Top({0, 0, 33}, 30);
}

void Sword(FKit& K)
{
	K.Signature = Silver;
	K.Ball(GoldTone, {0, 0, 3.5f}, {7, 7, 7});
	K.Tube(Brown, {0, 0, 14}, {5, 5, 16});
	K.Put(EShape::Cube, GoldTone, {0, 0, 24}, {6, 40, 6}, FVec{}, ERole::Body, EFinish::Gold);
	K.Box(Silver, {0, 0, 82}, {4, 10, 110});
	K.Spike(Silver, {0, 0, 144}, {4, 10, 14});
	K.Face({3, 0, 104}, 7, 5);
	K.Top({0, 0, 151}, 8);
}

void Guitar(FKit& K)
{
	K.Signature = Orange;
	K.Ball(Orange, {0, 0, 22}, {12, 44, 40});
	K.Ball(Orange, {0, 0, 52}, {12, 34, 30});
	K.Tube(Black, {6, 0, 40}, {10, 10, 2}, {-90, 0, 0});
	K.Box(Brown, {0, 0, 95}, {6, 6, 60});
	K.Box(Black, {0, 0, 130}, {8, 10, 14});
	K.Face({7, 0, 58}, 10, 7);
	K.Top({0, 0, 137}, 10);
}

void Piano(FKit& K)
{
	K.Signature = Black;
	K.Box(Black, {0, 0, 60}, {60, 140, 90});
	K.Box(White, {36, 0, 72}, {14, 120, 4});
	K.Box(Black, {37, 0, 75}, {8, 118, 2});
	K.Face({31, 0, 95}, 40, 14);
	K.Top({0, 0, 105}, 50);
}

void Cup(FKit& K)
{
	K.Signature = White;
	K.Tube(White, {0, 0, 20}, {30, 30, 40});
	K.Box(White, {0, -20, 20}, {4, 10, 22});
	K.Tube(Brown, {0, 0, 38}, {26, 26, 2});
	K.Face({15, 0, 22}, 10, 8);
	K.Top({0, 0, 40}, 26);
}

void Teapot(FKit& K)
{
	K.Signature = White;
	K.Ball(White, {0, 0, 22}, {50, 50, 44});
	K.Ball(Blue, {0, 0, 46}, {22, 22, 10});
	K.Ball(Blue, {0, 0, 52}, {6, 6, 6});
	K.Tube(White, {28, 0, 28}, {7, 7, 30}, {-50, 0, 0});
	K.Box(White, {-28, 0, 26}, {6, 6, 28});
	K.Face({24, 0, 30}, 14, 9);
	K.Top({0, 0, 55}, 16);
}

void Clock(FKit& K)
{
	K.Signature = Brown;
	K.Box(Brown, {0, 0, 90}, {40, 50, 180});
	K.Tube(Cream, {21, 0, 150}, {36, 36, 2}, {-90, 0, 0});
	K.Tube(GoldTone, {21, 0, 90}, {2, 2, 40});
	K.Put(EShape::Sphere, GoldTone, {21, 0, 68}, {12, 3, 12}, FVec{}, ERole::Body, EFinish::Gold);
	K.Face({22, 0, 152}, 14, 9);
	K.Top({0, 0, 180}, 36);
}

void Phone(FKit& K)
{
	K.Signature = Black;
	K.Box(Black, {0, 0, 40}, {6, 40, 80});
	K.Put(EShape::Cube, LightBlue, {3.2f, 0, 42}, {1, 36, 66}, FVec{}, ERole::Body, EFinish::Glass);
	K.Face({4, 0, 58}, 14, 10);
	K.Top({0, 0, 80}, 30);
}

void Computer(FKit& K)
{
	K.Signature = Grey;
	K.Box(Grey, {0, 0, 3}, {30, 40, 6});
	K.Box(Grey, {-2, 0, 30}, {8, 10, 48});
	K.Box(Grey, {0, 0, 72}, {8, 80, 54});
	K.Put(EShape::Cube, Blue, {4.5f, 0, 73}, {1, 72, 46}, FVec{}, ERole::Body, EFinish::Glass);
	K.Box(Grey, {32, 0, 2}, {20, 60, 4});
	K.Face({5, 0, 76}, 26, 14);
	K.Top({0, 0, 99}, 40);
}

void Moon(FKit& K)
{
	K.Signature = Cream;
	K.Ball(Cream, {0, 0, 50}, {100, 100, 100});
	K.Ball(Grey, {40, -24, 70}, {8, 18, 18});
	K.Ball(Grey, {38, 26, 30}, {8, 22, 22});
	K.Ball(Grey, {44, 12, 76}, {6, 12, 12});
	K.Face({48, 0, 52}, 30, 16);
	K.Top({0, 0, 100}, 40);
}

void Star(FKit& K)
{
	K.Signature = Yellow;
	K.Ball(Yellow, {0, 0, 60}, {16, 40, 40});
	for (int I = 0; I < 5; ++I)
	{
		const float Roll = I * 72.0f;
		const float R = Roll * GistPi / 180;
		K.Spike(Yellow, {0, std::sin(R) * 28, 60 + std::cos(R) * 28}, {6, 14, 30}, {0, 0, Roll});
	}
	K.Face({9, 0, 62}, 12, 9);
	K.Top({0, 0, 103}, 12);
}

void Mountain(FKit& K)
{
	K.Signature = Grey;
	K.Spike(Grey, {0, 0, 90}, {200, 200, 180});
	K.Spike(White, {0, 0, 165}, {64, 64, 32});
	K.Face({66, 0, 60}, 30, 18);
	K.Top({0, 0, 181}, 20);
}

struct FArchetype { const char* Id; void (*Make)(FKit&); };
const FArchetype Archetypes[] = {
	{"cat", Cat}, {"dog", Dog}, {"bird", Bird}, {"fish", Fish}, {"dragon", Dragon},
	{"horse", Horse}, {"duck", Duck}, {"snake", Snake}, {"robot", Robot}, {"person", Person},
	{"car", Car}, {"boat", Boat}, {"rocket", Rocket}, {"train", Train}, {"plane", Plane},
	{"toaster", Toaster}, {"cake", Cake}, {"pizza", Pizza}, {"tree", Tree}, {"house", House},
	{"castle", Castle}, {"tower", Tower}, {"bridge", Bridge}, {"chair", Chair}, {"table", Table},
	{"bed", Bed}, {"lamp", Lamp}, {"hat", Hat}, {"crown", Crown}, {"sword", Sword},
	{"guitar", Guitar}, {"piano", Piano}, {"cup", Cup}, {"teapot", Teapot}, {"clock", Clock},
	{"phone", Phone}, {"computer", Computer}, {"moon", Moon}, {"star", Star}, {"mountain", Mountain},
};

bool MakeArchetype(const std::string& Id, FKit& K)
{
	for (const FArchetype& A : Archetypes)
		if (Id == A.Id) { A.Make(K); return true; }
	return false;
}

FColour SignatureOf(const std::string& Id)
{
	FKit K;
	return MakeArchetype(Id, K) ? K.Signature : Grey;
}

// No known noun: a wobbling stack of shapes, the same stack for the same words.
void Contraption(FKit& K, FRng& Rng)
{
	const EShape Shapes[] = {EShape::Cube, EShape::Sphere, EShape::Cylinder, EShape::Cone};
	const int Count = 3 + Rng.Below(5);
	float Z = 0, Width = 60;
	for (int I = 0; I < Count; ++I)
	{
		Width = 30 + Rng.Unit() * 50;
		const float Height = 25 + Rng.Unit() * 45;
		const FColour C = Palette[Rng.Below(PaletteSize)];
		if (I == 0) K.Signature = C;
		K.Put(Shapes[Rng.Below(4)], C, {(Rng.Unit() - 0.5f) * 16, (Rng.Unit() - 0.5f) * 16, Z + Height * 0.5f},
			{Width, Width * (0.8f + 0.4f * Rng.Unit()), Height}, {0, (Rng.Unit() - 0.5f) * 40, 0});
		Z += Height * 0.9f;
	}
	const FPart& Last = K.Parts.back();
	K.Face({Last.Location.X + Last.Size.X * 0.5f, Last.Location.Y, Last.Location.Z}, Width * 0.4f, std::min(Width * 0.3f, 18.0f));
	K.Top({Last.Location.X, Last.Location.Y, Z + Last.Size.Z * 0.1f}, Width);
}

// ---- decorations ---------------------------------------------------------------------------

void Eyes(FKit& K, FRng& Rng)
{
	for (const float Side : {-1.0f, 1.0f})
	{
		const FVec C{K.FaceAt.X + K.EyeSize * 0.2f, K.FaceAt.Y + Side * K.EyeGap * 0.5f, K.FaceAt.Z};
		K.Put(EShape::Sphere, White, C, {K.EyeSize, K.EyeSize, K.EyeSize}, FVec{}, ERole::Eye);
		// Googly: each pupil droops and wanders a little, the same way every time.
		const FVec Off{K.EyeSize * 0.36f, (Rng.Unit() - 0.5f) * K.EyeSize * 0.3f, -K.EyeSize * 0.12f + (Rng.Unit() - 0.5f) * K.EyeSize * 0.2f};
		const float P = K.EyeSize * 0.5f;
		K.Put(EShape::Sphere, Black, Add(C, Off), {P, P, P}, FVec{}, ERole::Pupil);
	}
}

void Feature(FKit& K, EShape Shape, const FColour& C, FVec At, FVec Size, FVec Rot = FVec{}, EFinish Finish = EFinish::Clay)
{
	K.Put(Shape, C, At, Size, Rot, ERole::Feature, Finish);
}

FVec EyeCentre(const FKit& K, float Side) { return {K.FaceAt.X + K.EyeSize * 0.2f, K.FaceAt.Y + Side * K.EyeGap * 0.5f, K.FaceAt.Z}; }

void Brows(FKit& K, float Slant)
{
	for (const float Side : {-1.0f, 1.0f})
		Feature(K, EShape::Cube, Black, Add(EyeCentre(K, Side), {K.EyeSize * 0.35f, 0, K.EyeSize * 0.75f}),
			{2, K.EyeSize * 1.1f, K.EyeSize * 0.22f}, {0, 0, -Side * Slant});
}

void Mouth(FKit& K, float Curve)
{
	for (int I = 0; I < 5; ++I)
	{
		const float T = (I - 2) / 2.0f;   // -1 .. 1
		const float D = K.EyeSize * 0.28f;
		Feature(K, EShape::Sphere, Black, {K.FaceAt.X + K.EyeSize * 0.3f, K.FaceAt.Y + T * K.EyeGap * 0.5f,
			K.FaceAt.Z - K.EyeSize * 1.3f + Curve * T * T * K.EyeSize * 0.5f}, {D, D, D});
	}
}

// Stack something on the hat spot and raise it by the height used.
void Raise(FKit& K, float Height) { K.TopAt.Z += Height; }

void TopHat(FKit& K)
{
	const float H = K.HeadSize, Z = K.TopAt.Z;
	Feature(K, EShape::Cylinder, Black, {K.TopAt.X, K.TopAt.Y, Z + H * 0.03f}, {H * 0.9f, H * 0.9f, H * 0.06f});
	Feature(K, EShape::Cylinder, Black, {K.TopAt.X, K.TopAt.Y, Z + H * 0.33f}, {H * 0.55f, H * 0.55f, H * 0.6f});
	Feature(K, EShape::Cylinder, Red, {K.TopAt.X, K.TopAt.Y, Z + H * 0.11f}, {H * 0.56f, H * 0.56f, H * 0.1f});
	Raise(K, H * 0.63f);
}

void CrownOnTop(FKit& K)
{
	const float H = K.HeadSize, Z = K.TopAt.Z;
	Feature(K, EShape::Cylinder, GoldTone, {K.TopAt.X, K.TopAt.Y, Z + H * 0.12f}, {H * 0.6f, H * 0.6f, H * 0.25f}, FVec{}, EFinish::Gold);
	for (int I = 0; I < 5; ++I)
	{
		const float A = I * 72.0f * GistPi / 180;
		Feature(K, EShape::Cone, GoldTone, {K.TopAt.X + std::cos(A) * H * 0.26f, K.TopAt.Y + std::sin(A) * H * 0.26f, Z + H * 0.33f},
			{H * 0.12f, H * 0.12f, H * 0.2f}, FVec{}, EFinish::Gold);
	}
	Raise(K, H * 0.43f);
}

void Flames(FKit& K)
{
	const float H = K.HeadSize, Z = K.TopAt.Z;
	Feature(K, EShape::Cone, Orange, {K.TopAt.X, K.TopAt.Y, Z + H * 0.4f}, {H * 0.5f, H * 0.5f, H * 0.8f}, FVec{}, EFinish::Glass);
	Feature(K, EShape::Cone, Yellow, {K.TopAt.X, K.TopAt.Y - H * 0.25f, Z + H * 0.25f}, {H * 0.3f, H * 0.3f, H * 0.5f}, FVec{}, EFinish::Glass);
	Feature(K, EShape::Cone, Yellow, {K.TopAt.X, K.TopAt.Y + H * 0.25f, Z + H * 0.25f}, {H * 0.3f, H * 0.3f, H * 0.5f}, FVec{}, EFinish::Glass);
	Raise(K, H * 0.8f);
}

void BowTie(FKit& K)
{
	const FVec C{K.FaceAt.X + K.EyeSize * 0.3f, K.FaceAt.Y, K.FaceAt.Z - K.EyeSize * 1.9f};
	const float E = K.EyeSize;
	Feature(K, EShape::Cone, Red, {C.X, C.Y - E * 0.45f, C.Z}, {E * 0.8f, E * 0.8f, E * 0.9f}, {0, 0, -90});
	Feature(K, EShape::Cone, Red, {C.X, C.Y + E * 0.45f, C.Z}, {E * 0.8f, E * 0.8f, E * 0.9f}, {0, 0, 90});
	Feature(K, EShape::Sphere, Red, C, {E * 0.35f, E * 0.35f, E * 0.35f});
}

FColour Darker(const FColour& C) { return {C.R * 0.45f, C.G * 0.45f, C.B * 0.45f}; }

// Unknown words beside a known noun each add one quirk, chosen by the word itself.
constexpr int QuirkCount = 9;
void Quirk(FKit& K, int Which, FRng& Rng)
{
	FVec Min, Max;
	Bounds(K.Parts, Min, Max);
	const float H = K.HeadSize, E = K.EyeSize;
	const float Length = Max.X - Min.X, Width = Max.Y - Min.Y, Height = Max.Z - Min.Z;
	switch (Which)
	{
	case 0:   // antenna
		Feature(K, EShape::Cylinder, Grey, {K.TopAt.X, K.TopAt.Y, K.TopAt.Z + H * 0.3f}, {H * 0.06f, H * 0.06f, H * 0.6f});
		Feature(K, EShape::Sphere, Red, {K.TopAt.X, K.TopAt.Y, K.TopAt.Z + H * 0.65f}, {H * 0.18f, H * 0.18f, H * 0.18f}, FVec{}, EFinish::Glass);
		Raise(K, H * 0.74f);
		break;
	case 1:   // propeller
		Feature(K, EShape::Cylinder, Grey, {K.TopAt.X, K.TopAt.Y, K.TopAt.Z + H * 0.1f}, {H * 0.12f, H * 0.12f, H * 0.2f});
		Feature(K, EShape::Cube, Palette[Rng.Below(PaletteSize)], {K.TopAt.X, K.TopAt.Y, K.TopAt.Z + H * 0.22f}, {H * 0.9f, H * 0.12f, H * 0.03f});
		Feature(K, EShape::Cube, Palette[Rng.Below(PaletteSize)], {K.TopAt.X, K.TopAt.Y, K.TopAt.Z + H * 0.22f}, {H * 0.9f, H * 0.12f, H * 0.03f}, {0, 90, 0});
		Raise(K, H * 0.24f);
		break;
	case 2:   // party hat
		Feature(K, EShape::Cone, Palette[Rng.Below(PaletteSize)], {K.TopAt.X, K.TopAt.Y, K.TopAt.Z + H * 0.35f}, {H * 0.5f, H * 0.5f, H * 0.7f});
		Feature(K, EShape::Sphere, White, {K.TopAt.X, K.TopAt.Y, K.TopAt.Z + H * 0.72f}, {H * 0.12f, H * 0.12f, H * 0.12f});
		Raise(K, H * 0.78f);
		break;
	case 3:
		BowTie(K);
		break;
	case 4:   // mustache
		for (const float Side : {-1.0f, 1.0f})
			Feature(K, EShape::Sphere, Brown, {K.FaceAt.X + E * 0.5f, K.FaceAt.Y + Side * E * 0.45f, K.FaceAt.Z - E * 1.0f},
				{E * 0.3f, E * 0.9f, E * 0.35f}, {0, 0, Side * 15});
		break;
	case 5:   // monocle
		Feature(K, EShape::Cylinder, LightBlue, Add(EyeCentre(K, 1), {E * 0.65f, 0, 0}), {E * 1.2f, E * 1.2f, 1}, {-90, 0, 0}, EFinish::Glass);
		break;
	case 6:   // spots
		for (int I = 0; I < 4; ++I)
		{
			const float Side = I % 2 ? 1.0f : -1.0f;
			const float D = std::max(4.0f, Height * 0.12f);
			Feature(K, EShape::Sphere, Darker(K.Signature), {Min.X + Length * (0.25f + 0.5f * Rng.Unit()), Side * (Width * 0.5f),
				Min.Z + Height * (0.3f + 0.4f * Rng.Unit())}, {D, D * 0.3f, D});
		}
		break;
	case 7:   // wheels
	{
		const float R = std::max(8.0f, Height * 0.15f);
		for (const float X : {Min.X + Length * 0.2f, Max.X - Length * 0.2f})
			for (const float Side : {-1.0f, 1.0f})
				Feature(K, EShape::Cylinder, Black, {X, Side * (Width * 0.5f + 3), Min.Z + R * 0.5f}, {R * 2, R * 2, 6}, {0, 0, 90});
		break;
	}
	default:  // a tail
		Feature(K, EShape::Cylinder, K.Signature, {Min.X - Length * 0.08f, 0, Min.Z + Height * 0.5f},
			{std::max(4.0f, Length * 0.08f), std::max(4.0f, Length * 0.08f), std::max(10.0f, Length * 0.45f)}, {45, 0, 0});
		break;
	}
}

void Wings(FKit& K)
{
	FVec Min, Max;
	Bounds(K.Parts, Min, Max);
	const float Length = Max.X - Min.X, Width = Max.Y - Min.Y, Height = Max.Z - Min.Z;
	for (const float Side : {-1.0f, 1.0f})
		Feature(K, EShape::Sphere, White, {(Min.X + Max.X) * 0.5f, Side * (Width * 0.5f + Length * 0.15f), Min.Z + Height * 0.65f},
			{std::max(12.0f, Length * 0.5f), 4, std::max(8.0f, Height * 0.25f)}, {0, 0, -Side * 30});
}

void Translate(std::vector<FPart>& Parts, const FVec& By) { for (FPart& P : Parts) P.Location = Add(P.Location, By); }
void ScaleParts(std::vector<FPart>& Parts, float S) { for (FPart& P : Parts) { P.Location = Mul(P.Location, S); P.Size = Mul(P.Size, S); } }

// Standing on the floor, centred.
void Settle(std::vector<FPart>& Parts)
{
	FVec Min, Max;
	Bounds(Parts, Min, Max);
	Translate(Parts, {-(Min.X + Max.X) * 0.5f, -(Min.Y + Max.Y) * 0.5f, -Min.Z});
}

// ---- words ---------------------------------------------------------------------------------

const std::set<std::string>& Stopwords()
{
	static const std::set<std::string> Words = {
		"a", "an", "the", "some", "my", "your", "his", "her", "their", "our", "its", "this", "that", "these",
		"those", "please", "me", "i", "id", "im", "want", "wanted", "would", "like", "make", "give", "build",
		"create", "generate", "need", "get", "got", "can", "could", "you", "u", "we", "us", "to", "for", "in",
		"on", "at", "is", "are", "be", "it", "just", "really", "very", "so", "super", "quite", "now", "new",
		"thing", "things", "something", "stuff", "kind", "sort", "type", "looking", "one", "of", "from", "made",
		"built", "out", "carved", "crafted", "sculpted"};
	return Words;
}

bool IsJoin(const std::string& W)
{
	return W == "with" || W == "and" || W == "wearing" || W == "holding" || W == "riding" || W == "carrying" || W == "plus";
}

bool IsMaterialLead(const std::string& W)
{
	return W == "made" || W == "built" || W == "out" || W == "carved" || W == "crafted" || W == "sculpted";
}

int CountOf(const std::string& W)
{
	static const std::map<std::string, int> Counts = {
		{"two", 2}, {"three", 3}, {"four", 4}, {"five", 5}, {"six", 6}, {"seven", 7}, {"eight", 8}, {"nine", 9},
		{"ten", 10}, {"eleven", 11}, {"twelve", 12}, {"dozen", 12}, {"pair", 2}, {"couple", 2}, {"few", 3},
		{"several", 4}, {"many", 6}, {"lots", 8}, {"bunch", 5}, {"herd", 8}, {"flock", 8}, {"crowd", 10},
		{"army", 12}, {"horde", 12}, {"swarm", 12}};
	const auto Found = Counts.find(W);
	if (Found != Counts.end()) return Found->second;
	if (!W.empty() && W.size() <= 3 && std::all_of(W.begin(), W.end(), [](char C) { return C >= '0' && C <= '9'; }))
	{
		int N = 0;
		for (const char C : W) N = N * 10 + (C - '0');
		return std::max(1, std::min(N, MaxCopies));   // "1 cat" and "0 cats" both build one
	}
	return 0;
}

// A known noun, or a plural of one ("cats", "boxes", "ponies").
const std::string* LookupNoun(const FVocab& Vocab, const std::string& W, bool& bPlural)
{
	bPlural = false;
	auto Found = Vocab.Nouns.find(W);
	if (Found != Vocab.Nouns.end()) return &Found->second;
	if (W.size() < 4 || W.back() != 's') return nullptr;
	const std::string Stems[] = {
		W.size() > 3 && W.compare(W.size() - 3, 3, "ies") == 0 ? W.substr(0, W.size() - 3) + "y" : std::string(),
		W.compare(W.size() - 2, 2, "es") == 0 ? W.substr(0, W.size() - 2) : std::string(),
		W.substr(0, W.size() - 1)};
	for (const std::string& Stem : Stems)
	{
		if (Stem.empty()) continue;
		Found = Vocab.Nouns.find(Stem);
		if (Found != Vocab.Nouns.end()) { bPlural = true; return &Found->second; }
	}
	return nullptr;
}

struct FAttach { std::string Archetype; std::vector<std::string> Mods; };

struct FParse
{
	std::string Main, Flavour, Material;
	bool bPlural = false;
	int Count = 0;
	std::vector<std::string> Mods;        // for the main thing, in order, no repeats
	std::vector<FAttach> Attach;          // "with a golden hat"
	std::vector<std::string> Unknown;     // words that build a quirk
	std::vector<std::string> Meaningful;  // for the plaque's initials
};

void PushUnique(std::vector<std::string>& List, const std::string& Id)
{
	if (std::find(List.begin(), List.end(), Id) == List.end()) List.push_back(Id);
}

FParse Parse(const FVocab& Vocab, const std::vector<std::string>& Tokens)
{
	enum class EMode { Free, Material, Attach };
	FParse Out;
	EMode Mode = EMode::Free;
	std::vector<std::string> Pending;   // modifiers waiting for the noun they describe
	for (size_t I = 0; I < Tokens.size(); ++I)
	{
		const std::string& W = Tokens[I];
		if (IsJoin(W)) { Mode = EMode::Attach; continue; }
		if (W == "from" || (W == "of" && I > 0 && IsMaterialLead(Tokens[I - 1]))) { Mode = EMode::Material; continue; }
		if (const int N = CountOf(W)) { Out.Count = N; continue; }
		if (Stopwords().count(W)) continue;
		bool bPlural = false;
		if (const std::string* Noun = LookupNoun(Vocab, W, bPlural))
		{
			Out.Meaningful.push_back(W);
			if (Mode == EMode::Material) { Out.Material = *Noun; for (const auto& M : Pending) PushUnique(Out.Mods, M); }
			else if (Mode == EMode::Attach) { if (Out.Attach.size() < 2) Out.Attach.push_back({*Noun, Pending}); }
			else
			{
				if (!Out.Main.empty()) Out.Flavour = Out.Main;
				Out.Main = *Noun;
				Out.bPlural = bPlural;
				for (const auto& M : Pending) PushUnique(Out.Mods, M);
			}
			Pending.clear();
			Mode = EMode::Free;
			continue;
		}
		const auto Mod = Vocab.Modifiers.find(W);
		if (Mod != Vocab.Modifiers.end())
		{
			Out.Meaningful.push_back(W);
			if (Mode == EMode::Free) PushUnique(Out.Mods, Mod->second);
			else Pending.push_back(Mod->second);
			continue;
		}
		Out.Meaningful.push_back(W);
		if (Out.Unknown.size() < 3) Out.Unknown.push_back(W);
	}
	for (const auto& M : Pending) PushUnique(Out.Mods, M);   // "a cat made of golden": the cat is golden
	if (Out.Main.empty() && !Out.Attach.empty())
	{
		// "with a hat" alone: the hat is the thing.
		Out.Main = Out.Attach.front().Archetype;
		for (const auto& M : Out.Attach.front().Mods) PushUnique(Out.Mods, M);
		Out.Attach.erase(Out.Attach.begin());
	}
	return Out;
}

bool Has(const std::vector<std::string>& Mods, const char* Id) { return std::find(Mods.begin(), Mods.end(), Id) != Mods.end(); }

// Colour, finish and invisibility for one list of Body parts.
void ApplyLooks(std::vector<FPart>& Parts, const std::vector<std::string>& Mods)
{
	struct FTint { const char* Id; FColour Colour; float Amount; EFinish Finish; };
	const FTint Tints[] = {
		{"red", Red, 0.75f, EFinish::Clay}, {"orange", Orange, 0.75f, EFinish::Clay}, {"yellow", Yellow, 0.75f, EFinish::Clay},
		{"green", Green, 0.75f, EFinish::Clay}, {"blue", Blue, 0.75f, EFinish::Clay}, {"purple", Purple, 0.75f, EFinish::Clay},
		{"pink", Pink, 0.75f, EFinish::Clay}, {"white", White, 0.8f, EFinish::Clay}, {"black", {0.12f, 0.12f, 0.13f}, 0.8f, EFinish::Clay},
		{"wooden", Brown, 0.8f, EFinish::Clay}, {"stone", Grey, 0.85f, EFinish::Clay}, {"icy", LightBlue, 0.8f, EFinish::Glass},
		{"golden", GoldTone, 1.0f, EFinish::Gold}, {"glass", LightBlue, 0.3f, EFinish::Glass}};
	for (const std::string& Mod : Mods)
	{
		for (const FTint& T : Tints)
		{
			if (Mod != T.Id) continue;
			for (FPart& P : Parts)
			{
				if (P.Role != ERole::Body) continue;
				P.Colour = Mix(P.Colour, T.Colour, T.Amount);
				// A colour turns gold to painted clay (a red crown is red), but glowing glass stays
				// glass, just tinted; a material word sets its own finish.
				if (T.Finish != EFinish::Clay) P.Finish = T.Finish;
				else if (P.Finish == EFinish::Gold) P.Finish = EFinish::Clay;
			}
		}
		if (Mod == "rainbow")
		{
			int I = 0;
			for (FPart& P : Parts)
				if (P.Role == ERole::Body) P.Colour = Hue(std::fmod(0.13f + 0.618f * static_cast<float>(I++), 1.0f));
		}
	}
	if (Has(Mods, "invisible"))
		Parts.erase(std::remove_if(Parts.begin(), Parts.end(), [](const FPart& P) { return P.Role == ERole::Body; }), Parts.end());
}

float SizeOf(const std::vector<std::string>& Mods)
{
	float S = 1;
	if (Has(Mods, "giant")) S *= 2.5f;
	if (Has(Mods, "big")) S *= 1.6f;
	if (Has(Mods, "small")) S *= 0.6f;
	if (Has(Mods, "tiny")) S *= 0.35f;
	return std::max(0.25f, std::min(2.5f, S));
}

// A small whole archetype set on the hat spot: "a cat with a hat", "a chair lamp".
void AttachMini(FKit& K, const std::string& Id, const std::vector<std::string>& Mods, FRng& Rng)
{
	FKit Mini;
	if (!MakeArchetype(Id, Mini)) return;
	Eyes(Mini, Rng);
	ApplyLooks(Mini.Parts, Mods);
	for (FPart& P : Mini.Parts) if (P.Role == ERole::Body) P.Role = ERole::Feature;
	FVec MinM, MaxM, MinK, MaxK;
	Bounds(Mini.Parts, MinM, MaxM);
	Bounds(K.Parts, MinK, MaxK);
	const float MiniSpan = std::max({MaxM.X - MinM.X, MaxM.Y - MinM.Y, MaxM.Z - MinM.Z, 1.0f});
	const float HostSpan = std::max({MaxK.X - MinK.X, MaxK.Y - MinK.Y, MaxK.Z - MinK.Z, 1.0f});
	const float S = std::max(0.15f, std::min(1.0f, 0.45f * HostSpan / MiniSpan)) * (Has(Mods, "tiny") ? 0.5f : 1.0f);
	ScaleParts(Mini.Parts, S);
	Bounds(Mini.Parts, MinM, MaxM);
	Translate(Mini.Parts, {K.TopAt.X - (MinM.X + MaxM.X) * 0.5f, K.TopAt.Y - (MinM.Y + MaxM.Y) * 0.5f, K.TopAt.Z - MinM.Z});
	K.Parts.insert(K.Parts.end(), Mini.Parts.begin(), Mini.Parts.end());
	Raise(K, MaxM.Z - MinM.Z);
	K.HeadSize = std::max(8.0f, K.HeadSize * 0.6f);
}

uint32_t HashWord(const std::string& W) { return Hash(std::vector<std::string>{W}); }

std::string CapEcho(const std::string& Text)
{
	// At most EchoChars characters, never cutting a UTF-8 sequence, with an ellipsis if cut.
	int Chars = 0;
	for (size_t I = 0; I < Text.size(); ++I)
	{
		if ((static_cast<unsigned char>(Text[I]) & 0xC0) == 0x80) continue;
		if (++Chars == EchoChars)
		{
			size_t End = I + 1;
			while (End < Text.size() && (static_cast<unsigned char>(Text[End]) & 0xC0) == 0x80) ++End;
			return End < Text.size() ? Text.substr(0, I) + "\xE2\x80\xA6" : Text;
		}
	}
	return Text;
}
} // namespace GistDetail

using namespace GistDetail;

// ---- public API ----------------------------------------------------------------------------

std::vector<std::pair<std::string, std::string>> ParseWordCsv(const std::string& Text)
{
	std::vector<std::pair<std::string, std::string>> Rows;
	size_t Start = 0;
	bool bHeader = true;
	while (Start <= Text.size())
	{
		size_t End = Text.find('\n', Start);
		if (End == std::string::npos) End = Text.size();
		std::string Line = Text.substr(Start, End - Start);
		Start = End + 1;
		Line.erase(std::remove(Line.begin(), Line.end(), '\r'), Line.end());
		if (bHeader) { bHeader = false; continue; }
		if (Line.empty() || Line[0] == '#') continue;
		const size_t Comma = Line.find(',');
		if (Comma == std::string::npos) continue;
		auto Clean = [](std::string S)
		{
			S.erase(std::remove(S.begin(), S.end(), '"'), S.end());
			const size_t B = S.find_first_not_of(" \t"), E = S.find_last_not_of(" \t");
			S = B == std::string::npos ? std::string() : S.substr(B, E - B + 1);
			for (char& C : S) if (C >= 'A' && C <= 'Z') C = static_cast<char>(C - 'A' + 'a');
			return S;
		};
		std::string Word = Clean(Line.substr(0, Comma)), Id = Clean(Line.substr(Comma + 1));
		if (!Word.empty() && !Id.empty()) Rows.emplace_back(Word, Id);
	}
	return Rows;
}

std::vector<std::string> ParseBlocklistCsv(const std::string& Text)
{
	std::vector<std::string> Words;
	for (const auto& Row : ParseWordCsv(Text)) Words.push_back(Row.second);
	return Words;
}

FVocab MakeVocab(const std::string& NounsCsv, const std::string& ModifiersCsv, const std::string& BlocklistCsv)
{
	FVocab V;
	for (const auto& Row : ParseWordCsv(NounsCsv)) V.Nouns[Row.first] = Row.second;
	for (const auto& Row : ParseWordCsv(ModifiersCsv)) V.Modifiers[Row.first] = Row.second;
	V.Blocklist = ParseBlocklistCsv(BlocklistCsv);
	return V;
}

std::vector<std::string> Tokenize(const std::string& Prompt)
{
	std::vector<std::string> Tokens;
	std::string Word;
	for (const char Raw : Prompt)
	{
		char C = Raw;
		if (C >= 'A' && C <= 'Z') C = static_cast<char>(C - 'A' + 'a');
		const bool bWordChar = (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || (static_cast<unsigned char>(C) >= 0x80);
		if (bWordChar) Word += C;
		else if (C == '\'') continue;   // "dragon's" -> "dragons", as the matcher strips it
		else if (!Word.empty()) { Tokens.push_back(Word); Word.clear(); }
	}
	if (!Word.empty()) Tokens.push_back(Word);
	return Tokens;
}

uint32_t Hash(const std::vector<std::string>& Tokens)
{
	uint32_t H = 2166136261u;   // FNV-1a over the words joined by single spaces
	for (size_t I = 0; I < Tokens.size(); ++I)
	{
		if (I) { H ^= static_cast<uint32_t>(' '); H *= 16777619u; }
		for (const char C : Tokens[I]) { H ^= static_cast<unsigned char>(C); H *= 16777619u; }
	}
	return H;
}

void Bounds(const std::vector<FPart>& Parts, FVec& OutMin, FVec& OutMax)
{
	OutMin = {1e9f, 1e9f, 1e9f};
	OutMax = {-1e9f, -1e9f, -1e9f};
	for (const FPart& P : Parts)
		for (int Corner = 0; Corner < 8; ++Corner)
		{
			const FVec Local{(Corner & 1 ? 0.5f : -0.5f) * P.Size.X, (Corner & 2 ? 0.5f : -0.5f) * P.Size.Y, (Corner & 4 ? 0.5f : -0.5f) * P.Size.Z};
			const FVec W = Add(P.Location, Rotate(P.Rotation, Local));
			OutMin = {std::min(OutMin.X, W.X), std::min(OutMin.Y, W.Y), std::min(OutMin.Z, W.Z)};
			OutMax = {std::max(OutMax.X, W.X), std::max(OutMax.Y, W.Y), std::max(OutMax.Z, W.Z)};
		}
	if (Parts.empty()) OutMin = OutMax = FVec{};
}

const std::vector<std::string>& ArchetypeIds()
{
	static const std::vector<std::string> Ids = []
	{
		std::vector<std::string> Out;
		for (const FArchetype& A : Archetypes) Out.push_back(A.Id);
		return Out;
	}();
	return Ids;
}

const std::vector<std::string>& ModifierIds()
{
	static const std::vector<std::string> Ids = {
		"giant", "big", "small", "tiny", "golden", "glass", "wooden", "stone", "icy", "red", "orange", "yellow",
		"green", "blue", "purple", "pink", "white", "black", "rainbow", "invisible", "flying", "spinning",
		"dancing", "bouncy", "wobbly", "sleepy", "angry", "happy", "sad", "scary", "fancy", "royal", "fiery"};
	return Ids;
}

FRecipe Build(const FVocab& Vocab, const std::string& Prompt)
{
	FRecipe Out;
	const std::vector<std::string> Tokens = Tokenize(Prompt);
	if (Tokens.empty()) { Out.bEmpty = true; return Out; }
	// The Mrs. Hall blocklist runs in the matcher first; this is the second lock, plurals included.
	for (const std::string& W : Tokens)
	{
		const bool bBlocked = std::find(Vocab.Blocklist.begin(), Vocab.Blocklist.end(), W) != Vocab.Blocklist.end()
			|| (W.size() > 3 && W.back() == 's' && std::find(Vocab.Blocklist.begin(), Vocab.Blocklist.end(), W.substr(0, W.size() - 1)) != Vocab.Blocklist.end());
		if (bBlocked) { Out.bRefused = true; return Out; }
	}
	Out.Seed = Hash(Tokens);
	FRng Rng(Out.Seed);
	const FParse P = Parse(Vocab, Tokens);

	FKit K;
	if (P.Main.empty() || !MakeArchetype(P.Main, K))
	{
		Out.bContraption = true;
		Contraption(K, Rng);
	}
	else Out.Archetype = P.Main;

	// Looks: "made of" first, then a hybrid's flavour, then the adjectives.
	if (!P.Material.empty())
	{
		const FColour M = SignatureOf(P.Material);
		for (FPart& Part : K.Parts) if (Part.Role == ERole::Body) Part.Colour = Mix(Part.Colour, M, 0.85f);
	}
	if (!P.Flavour.empty())
	{
		const FColour F = SignatureOf(P.Flavour);
		for (FPart& Part : K.Parts) if (Part.Role == ERole::Body) Part.Colour = Mix(Part.Colour, F, 0.5f);
	}
	if (Has(P.Mods, "angry")) for (FPart& Part : K.Parts) if (Part.Role == ERole::Body) Part.Colour = Mix(Part.Colour, Red, 0.45f);
	if (Has(P.Mods, "sad")) for (FPart& Part : K.Parts) if (Part.Role == ERole::Body) Part.Colour = Mix(Part.Colour, Blue, 0.3f);
	if (Has(P.Mods, "scary")) for (FPart& Part : K.Parts) if (Part.Role == ERole::Body) Part.Colour = Mix(Darker(Part.Colour), Purple, 0.2f);
	ApplyLooks(K.Parts, P.Mods);

	Eyes(K, Rng);
	if (Has(P.Mods, "angry")) Brows(K, 25);
	if (Has(P.Mods, "sad")) { Brows(K, -20); Mouth(K, -1); }
	if (Has(P.Mods, "happy")) Mouth(K, 1);
	if (Has(P.Mods, "scary"))
	{
		const FVec Saved = K.FaceAt;
		const float SavedEye = K.EyeSize;
		K.FaceAt.Z += K.EyeSize * 1.2f;
		K.EyeSize *= 0.7f;
		Eyes(K, Rng);
		K.FaceAt = Saved;
		K.EyeSize = SavedEye;
	}
	if (Has(P.Mods, "sleepy"))
		for (const float Side : {-1.0f, 1.0f})
			Feature(K, EShape::Sphere, Mix(K.Signature, Black, 0.2f), Add(EyeCentre(K, Side), {K.EyeSize * 0.05f, 0, K.EyeSize * 0.25f}),
				{K.EyeSize * 1.05f, K.EyeSize * 1.05f, K.EyeSize * 0.55f});

	// Things on top and beside.
	if (!P.Flavour.empty()) AttachMini(K, P.Flavour, {}, Rng);
	for (const FAttach& A : P.Attach) AttachMini(K, A.Archetype, A.Mods, Rng);
	if (!Out.bContraption)
		for (const std::string& W : P.Unknown) Quirk(K, static_cast<int>(HashWord(W) % QuirkCount), Rng);
	else if (Rng.Unit() < 0.35f) TopHat(K);
	if (Has(P.Mods, "fancy")) { TopHat(K); BowTie(K); }
	if (Has(P.Mods, "royal")) CrownOnTop(K);
	if (Has(P.Mods, "fiery")) Flames(K);
	if (Has(P.Mods, "flying") && !K.bHasWings) Wings(K);

	if (Has(P.Mods, "flying")) Out.Behaviours |= Behaviour::Hover;
	if (Has(P.Mods, "spinning")) Out.Behaviours |= Behaviour::Spin;
	if (Has(P.Mods, "dancing")) Out.Behaviours |= Behaviour::Dance;
	if (Has(P.Mods, "bouncy")) Out.Behaviours |= Behaviour::Bounce;
	if (Has(P.Mods, "wobbly")) Out.Behaviours |= Behaviour::Wobble;
	if (Has(P.Mods, "sleepy")) Out.Behaviours |= Behaviour::Sway;

	// Size, copies, and the room it has to fit.
	std::vector<FPart> One = K.Parts;
	if (static_cast<int>(One.size()) > MaxParts) One.resize(MaxParts);
	Settle(One);
	Out.Scale = SizeOf(P.Mods);
	Out.bGiant = Out.Scale > 1.61f;
	ScaleParts(One, Out.Scale);
	int Copies = P.Count > 0 ? P.Count : (P.bPlural ? 3 : 1);
	Copies = std::max(1, std::min({Copies, MaxCopies, MaxParts / std::max(1, static_cast<int>(One.size()))}));
	Out.Copies = Copies;
	FVec Min, Max;
	Bounds(One, Min, Max);
	const int Columns = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(Copies))));
	const float StepX = (Max.X - Min.X) * 1.1f + 10, StepY = (Max.Y - Min.Y) * 1.1f + 10;
	for (int C = 0; C < Copies; ++C)
	{
		std::vector<FPart> Copy = One;
		Translate(Copy, {-StepX * static_cast<float>(C / Columns), StepY * static_cast<float>(C % Columns), 0});
		Out.Parts.insert(Out.Parts.end(), Copy.begin(), Copy.end());
	}
	Settle(Out.Parts);
	Bounds(Out.Parts, Min, Max);
	const float Limit = Out.bGiant ? GiantRoom : Room;
	const float Span = std::max({Max.X - Min.X, Max.Y - Min.Y, Max.Z - Min.Z});
	if (Span > Limit)
	{
		ScaleParts(Out.Parts, Limit / Span);
		Out.Scale *= Limit / Span;
		Settle(Out.Parts);
	}

	// The plaque.
	std::string Initials;
	for (const std::string& W : P.Meaningful)
		if (Initials.size() < 3 && W[0] >= 'a' && W[0] <= 'z') Initials += static_cast<char>(W[0] - 'a' + 'A');
	if (Initials.empty()) Initials = "X";
	Out.Title = "THE GIST \xE2\x80\x94 Model " + Initials + "-" + std::to_string(1000 + Out.Seed % 9000);
	std::string Joined;
	for (const std::string& W : Tokens) Joined += (Joined.empty() ? "" : " ") + W;
	Out.Echo = CapEcho(Joined);
	static const char* Taglines[] = {
		"Assembly not required.", "Batteries not included.", "Do not feed after midnight.",
		"Any resemblance is coincidental.", "As described, more or less.", "Warranty void if looked at.",
		"Exactly as imagined. Probably.", "Returns not accepted.", "Handle with mild concern.", "Close enough."};
	Out.Tagline = Out.bContraption ? "Interpretation " + std::to_string(2 + Out.Seed % 97) + " of \xE2\x88\x9E"
		: Taglines[(Out.Seed / 7) % (sizeof(Taglines) / sizeof(Taglines[0]))];
	return Out;
}
}
