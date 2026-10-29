#include "DragonsWagerRules.h"
#include <cassert>
#include <iostream>
using namespace DragonsWager;
int main()
{
    Rules R;
    assert(!R.TakeJewel() && !R.TakeCrown() && !R.Return() && R.Bank()==0);
    R.Start();
    assert(!R.Decoy() && !R.Return() && !R.Raise());
    assert(R.TakeJewel() && R.Purse==25 && R.Jewels==1);
    assert(R.Decoy() && R.Purse==0 && R.Jewels==0 && !R.Decoy());
    assert(R.TakeCrown() && !R.TakeCrown() && R.Purse==100);
    assert(R.Return() && !R.TakeJewel() && !R.Decoy());
    assert(R.Raise() && R.Round==2 && R.Purse==100 && !R.Crown);
    assert(R.TakeJewel() && R.TakeCrown() && R.Return());
    assert(R.Bank()==350 && R.Bank()==0 && R.State==Phase::Idle);
    R.Start(); R.TakeCrown();
    for(int I=0; I<20; ++I) assert(!R.Tick(0.1f,true)); // grace
    assert(R.Exposure==0);
    for(int I=0; I<20; ++I) R.Tick(0.1f,false);
    for(int I=0; I<5; ++I) R.Tick(0.1f,true);
    assert(R.Exposure>0);
    for(int I=0; I<5; ++I) R.Tick(0.1f,false);
    assert(R.Exposure==0); // real cover clears suspicion
    bool Caught=false;
    for(int I=0; I<30; ++I) Caught=R.Tick(0.1f,true)||Caught;
    assert(Caught && R.State==Phase::Caught && R.Purse==0 && !R.Crown && R.Bank()==0);
    R.Start(); R.Grace=0;
    assert(!R.Tick(100,true)); // a frame hitch is not a loss
    R.Leave(); assert(R.Bank()==0 && R.Purse==0);
    R.Start();
    for(int I=1; I<=Rules::MaxRounds; ++I)
    {
        assert(R.TakeCrown() && R.Return());
        if(I<Rules::MaxRounds) assert(R.Raise());
    }
    assert(!R.Raise() && R.Bank()==5500);
    R.Start(); R.TakeJewel(); R.TakeCrown(); R.Return(); R.Raise();
    assert(R.Decoy() && R.Purse==100); // old jewel keeps its original value
    R.TakeJewel(); R.TakeCrown(); R.Return(); R.Raise();
    assert(R.Decoy() && R.Purse==300);
    std::cout << "Dragon's Wager: state, risk, decoy, cover recovery, catch, replay, cap and banking tests passed.\n";
}
