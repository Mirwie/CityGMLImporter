// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FCityGMLImporterModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void PluginButtonClicked();
	void AddMenuExtension(FMenuBuilder& Builder);
	void ProcessCityGML(const FString& FString);
	FVector ConvertUtmToUnreal(float UTM_X, float UTM_Y, FVector OriginOffset);
	void CreateMeshFromPolygon(TArray<TArray<TArray<FVector>>>& Buildings, TArray<TArray<TArray<int32>>>& Triangles, TArray<FString> BuildingIds);
	void testGebauedeGenerationUE();



	TSharedPtr<FUICommandList> PluginCommands;

};
