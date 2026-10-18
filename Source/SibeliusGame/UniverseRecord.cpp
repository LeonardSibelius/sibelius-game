// UniverseRecord.cpp - see the header.

#include "UniverseRecord.h"

bool FUniverseRecord::IsValid() const
{
	// A save is read back from disk, so everything is checked - an out-of-range ending or a
	// chronicle of the wrong length would otherwise reach the forge's playback as an index.
	const auto Unit = [](double V) { return FMath::IsFinite(V) && V >= 0.0 && V <= 1.0; };
	return Id.IsValid()
		&& ModelVersion == 1
		&& !Name.IsEmpty()
		&& Ending >= 0 && Ending <= static_cast<int32>(EUniverseEnding::LookedBack)
		&& LastStage >= 0 && LastStage < static_cast<int32>(EUniverseStage::Count)
		&& Galaxies >= 0 && Galaxies <= 16
		&& Unit(Gravity) && Unit(Light) && Unit(Time) && Unit(StarHeat)
		&& FMath::IsFinite(AgeBillions) && AgeBillions >= 0.0
		&& Chronicle.Num() == LastStage + 2          // a line per stage reached, and the ending's
		&& !EndingTitle.IsEmpty()
		&& !EndingLine.IsEmpty();
}

FUniverseRecord FUniverseRecord::Capture(const FUniverseHistory& History, const FGuid& Identity)
{
	FUniverseRecord Record;
	Record.Id = Identity;
	Record.Seed = History.Seed;
	Record.Gravity = History.Dials.Gravity;
	Record.Light = History.Dials.Light;
	Record.Time = History.Dials.Time;
	Record.Name = UTF8_TO_TCHAR(History.Name.c_str());
	Record.Ending = static_cast<int32>(History.Ending);
	Record.LastStage = static_cast<int32>(History.LastStage);
	Record.Galaxies = History.Galaxies;
	Record.StarHeat = History.StarHeat;
	Record.AgeBillions = History.AgeBillions;
	Record.bWorldThawed = History.bWorldThawed;
	for (const std::string& Line : History.Chronicle)
	{
		Record.Chronicle.Add(UTF8_TO_TCHAR(Line.c_str()));
	}
	Record.EndingTitle = UTF8_TO_TCHAR(History.EndingTitle.c_str());
	Record.EndingLine = UTF8_TO_TCHAR(History.EndingLine.c_str());
	return Record;
}

FUniverseHistory FUniverseRecord::Restore() const
{
	FUniverseHistory History;
	History.Seed = Seed;
	History.Dials = { Gravity, Light, Time };
	History.Name = TCHAR_TO_UTF8(*Name);
	History.Ending = static_cast<EUniverseEnding>(Ending);
	History.LastStage = static_cast<EUniverseStage>(LastStage);
	History.Galaxies = Galaxies;
	History.StarHeat = StarHeat;
	History.AgeBillions = AgeBillions;
	History.bWorldThawed = bWorldThawed;
	for (const FString& Line : Chronicle)
	{
		History.Chronicle.emplace_back(TCHAR_TO_UTF8(*Line));
	}
	History.EndingTitle = TCHAR_TO_UTF8(*EndingTitle);
	History.EndingLine = TCHAR_TO_UTF8(*EndingLine);
	return History;
}
