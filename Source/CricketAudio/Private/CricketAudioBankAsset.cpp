#include "CricketAudioBankAsset.h"

const FCricketSoundSet* UCricketAudioBankAsset::Find(ECricketAudioEvent Event) const
{
	for (const FCricketAudioBankEntry& Entry : Entries)
	{
		if (Entry.Event == Event) { return &Entry.Sound; }
	}
	return nullptr;
}
