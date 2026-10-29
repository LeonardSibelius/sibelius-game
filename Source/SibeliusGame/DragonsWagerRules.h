#pragma once

// Engine-independent rules. The world supplies visibility and interactions; only
// banking commits a score. Leaving or getting caught never awards a partial run.
#include <algorithm>
#include <cstdint>
#include <vector>

namespace DragonsWager
{
enum class Phase { Idle, Hunting, Returned, Caught };
struct Rules
{
    Phase State = Phase::Idle;
    int Round = 1;
    int Jewels = 0;
    int Purse = 0;
    std::vector<int> JewelValues;
    bool Crown = false;
    float Exposure = 0;
    float Grace = 0;
    float Recovery = 0;
    static constexpr int MaxRounds = 10;

    void Start() { *this = Rules{}; State = Phase::Hunting; Grace = 3; }
    void Leave() { *this = Rules{}; }
    bool TakeJewel()
    {
        if (State != Phase::Hunting) return false;
        ++Jewels; Purse += 25 * Round; JewelValues.push_back(25 * Round); return true;
    }
    bool Decoy()
    {
        if (State != Phase::Hunting || Jewels == 0) return false;
        --Jewels; Purse -= JewelValues.back(); JewelValues.pop_back(); return true;
    }
    bool TakeCrown()
    {
        if (State != Phase::Hunting || Crown) return false;
        Crown = true; Purse += 100 * Round; return true;
    }
    bool Return()
    {
        if (State != Phase::Hunting || !Crown) return false;
        State = Phase::Returned; Exposure = 0; return true;
    }
    bool Raise()
    {
        if (State != Phase::Returned || Round >= MaxRounds) return false;
        ++Round; Crown = false; State = Phase::Hunting; Grace = 3; return true;
    }
    int Bank()
    {
        if (State != Phase::Returned) return 0;
        const int Score = Purse; Leave(); return Score;
    }
    float CatchSeconds() const { return std::max(0.85f, 1.65f - (Round - 1) * 0.08f); }
    bool Tick(float Dt, bool Seen)
    {
        Dt = std::clamp(Dt, 0.0f, 0.1f); // a hitch cannot cause an instant catch
        if (State == Phase::Caught) { Recovery = std::max(0.0f, Recovery - Dt); return false; }
        if (State != Phase::Hunting) return false;
        if (Grace > 0) { Grace = std::max(0.0f, Grace - Dt); Exposure = 0; return false; }
        Exposure = std::clamp(Exposure + Dt * (Seen ? 1.0f : -1.8f), 0.0f, CatchSeconds());
        if (Exposure < CatchSeconds()) return false;
        State = Phase::Caught; Purse = 0; Jewels = 0; JewelValues.clear(); Crown = false; Recovery = 1.5f;
        return true;
    }
};
}
