// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "TextureBakerStyle.h"

class FTextureBakerCommands : public TCommands<FTextureBakerCommands>
{
public:

	FTextureBakerCommands()
		: TCommands<FTextureBakerCommands>(TEXT("TextureBaker"), NSLOCTEXT("Contexts", "TextureBaker", "TextureBaker Plugin"), NAME_None, FTextureBakerStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};