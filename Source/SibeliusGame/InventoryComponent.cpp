#include "InventoryComponent.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::Add(EResourceType Resource, int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}
	int32& Count = Counts.FindOrAdd(Resource);
	Count += Amount;
	OnInventoryChanged.Broadcast(Resource, Count);
}

bool UInventoryComponent::Spend(EResourceType Resource, int32 Amount)
{
	if (Amount <= 0)
	{
		return false;
	}
	int32* Count = Counts.Find(Resource);
	if (!Count || *Count < Amount)
	{
		return false; // C3: reject, never go negative
	}
	*Count -= Amount;
	OnInventoryChanged.Broadcast(Resource, *Count);
	return true;
}

int32 UInventoryComponent::GetCount(EResourceType Resource) const
{
	const int32* Count = Counts.Find(Resource);
	return Count ? *Count : 0;
}

void UInventoryComponent::RestoreCount(EResourceType Resource, int32 Count)
{
	const int32 Clamped = FMath::Max(0, Count);
	Counts.FindOrAdd(Resource) = Clamped;
	OnInventoryChanged.Broadcast(Resource, Clamped);
}

bool UInventoryComponent::RunInventorySelfTest(FString& OutError)
{
	// Bar item 2: Add(Book,12) -> Spend(Book,8) -> Count==4; over-spend rejected.
	Counts.Reset();

	Add(EResourceType::Book, 12);
	if (GetCount(EResourceType::Book) != 12)
	{
		OutError = TEXT("Add(Book,12) did not yield 12");
		return false;
	}
	if (!Spend(EResourceType::Book, 8))
	{
		OutError = TEXT("Spend(Book,8) rejected with 12 in inventory");
		return false;
	}
	if (GetCount(EResourceType::Book) != 4)
	{
		OutError = TEXT("Count after spend != 4");
		return false;
	}
	if (Spend(EResourceType::Book, 5))
	{
		OutError = TEXT("Over-spend (5 of 4) was wrongly accepted");
		return false;
	}
	if (GetCount(EResourceType::Book) != 4)
	{
		OutError = TEXT("Rejected spend mutated the count");
		return false;
	}
	if (Spend(EResourceType::Book, -1) || GetCount(EResourceType::Book) != 4)
	{
		OutError = TEXT("Negative spend was accepted or mutated count");
		return false;
	}

	Counts.Reset();
	return true;
}
