// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "XmlFile.h"

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
	void ProcessLoD1(const TArray<FXmlNode*>& CityObjectMembers, FVector Offsetvector);
	void ProcessLoD2(const TArray<FXmlNode*>& CityObjectMembers, FVector Offsetvector);
	void ProcessLoD3(const TArray<FXmlNode*>& CityObjectMembers, FVector Offsetvector);
	FVector ConvertUtmToUnreal(float UTM_X, float UTM_Y, float UTM_Z,  FVector OriginOffset);
	void CreateMeshFromPolygon(TArray<TArray<TArray<FVector>>>& Buildings, TArray<TArray<TArray<int32>>>& Triangles, TArray<FString> BuildingIds);
	void CreateOneMeshFromPolygon(TArray<TArray<TArray<FVector>>>& Buildings, TArray<TArray<TArray<int32>>>& Triangles);
	TArray<FVector> ParsePolygon(const FXmlNode* PolygonNode, FVector OffsetVector);
	TArray<int32> GenerateTriangles(const TArray<FVector>& Vertices);
	void GenerateNormals(const TArray<FVector>& v);
	void GenerateUVs(const TArray<FVector>& v);
	void GenerateTangents(const TArray<FVector>& v, const TArray<int32> t);
	//bool IsConvex(const TArray<FVector>& p);




	TSharedPtr<FUICommandList> PluginCommands;
};
