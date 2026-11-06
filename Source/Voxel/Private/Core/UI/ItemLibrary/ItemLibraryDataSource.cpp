// Copyright (c) 2026 zolnoor. All rights reserved.

#include "Core/UI/ItemLibrary/ItemLibraryDataSource.h"

bool UItemLibraryDataSource::MatchesSearch_Implementation(const FItemLibraryEntry& Entry, const FString& Keyword) const
{
	if (Keyword.IsEmpty())
	{
		return true;
	}
	if (Entry.DisplayName.ToString().Contains(Keyword, ESearchCase::IgnoreCase))
	{
		return true;
	}
	if (Entry.EntryTag.ToString().Contains(Keyword, ESearchCase::IgnoreCase))
	{
		return true;
	}
	return false;
}
