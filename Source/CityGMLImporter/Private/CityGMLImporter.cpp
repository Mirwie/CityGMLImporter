// Copyright Epic Games, Inc. All Rights Reserved.

#include "CityGMLImporter.h"
#include "Misc/MessageDialog.h"
#include "LevelEditor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Framework/Application/SlateApplication.h"
#include "DesktopPlatformModule.h"
#include "XmlParser/Public/XmlFile.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"

static const FName CityGMLImporterTabName("CityGMLImporter");

#define LOCTEXT_NAMESPACE "FCityGMLImporterModule"

TArray<TArray<TArray<FVector>>> AllBuildings;
TArray<TArray<TArray<int32>>> AllTriangles;
TArray<FVector> Normalen;
float Skalierung = 100.0f; // Normalgroeße bei UE


void FCityGMLImporterModule::StartupModule()
{
    // Dieser Code wird nach dem Laden des Moduls ausgeführt
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

    TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
    MenuExtender->AddMenuExtension("FileProject", EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateRaw(this, &FCityGMLImporterModule::AddMenuExtension));

    LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
}

void FCityGMLImporterModule::ShutdownModule()
{
    // Diese Funktion kann beim Herunterfahren aufgerufen werden, um das Modul aufzuräumen
}

void FCityGMLImporterModule::AddMenuExtension(FMenuBuilder& Builder)
{
    Builder.AddMenuEntry(
        LOCTEXT("CityGMLParser", "Select CityGML-File"),
        LOCTEXT("CityGMLParser_Tooltip", "Select a CityGML file to import"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(this, &FCityGMLImporterModule::PluginButtonClicked))
    );
}


void FCityGMLImporterModule::PluginButtonClicked()
{

    TArray<FString> OutFiles;
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    bool bOpened = false;

    if (DesktopPlatform)
    {
        bOpened = DesktopPlatform->OpenFileDialog(  // Öffne den Datei-Dialog
            nullptr,
            TEXT("Select CityGML files to import"),
            FPaths::ProjectDir(),
            TEXT(""),
            TEXT("CityGML Files (*.xml)|*.xml"),
            EFileDialogFlags::Multiple,
            OutFiles
        );
    }

    if (bOpened && OutFiles.Num() > 0)
    {
        AllBuildings.Empty();
        AllTriangles.Empty();
        for (const FString& SelectedFile : OutFiles) {
            ProcessCityGML(SelectedFile);
        }
        CreateOneMeshFromPolygon(AllBuildings, AllTriangles);
        FText DialogText = FText::Format(
            LOCTEXT("FilesLoaded", "{0} Dateien erfolgreich eingelesen."),
            FText::AsNumber(OutFiles.Num())
            );
        FMessageDialog::Open(EAppMsgType::Ok, DialogText);
    }
    else
    {
        FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FileNotSelected", "Keine Datei ausgewählt"));
    }
}

void FCityGMLImporterModule::ProcessCityGML(const FString& FilePath)
{
    // XML-Datei laden
    FXmlFile XmlFile(FilePath);
    if (!XmlFile.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load XML file: %s"), *FilePath);
        return;
    }

    // Überprüfen, ob es sich um eine CityGML-Datei handelt und Rootnode bekommen
    FXmlNode* RootNode = XmlFile.GetRootNode();
    if (!RootNode || RootNode->GetTag() != TEXT("core:CityModel"))
    {
        UE_LOG(LogTemp, Error, TEXT("The file does not appear to be a valid CityGML file: %s"), *FilePath);
        return;
    }

    const TArray<FXmlNode*>& CityObjectMembers = RootNode->GetChildrenNodes();
    FVector OffsetVector = FVector(0.0f, 0.0f, 0.0f);
    const FXmlNode* BoundaryNode = CityObjectMembers[1]->FindChildNode(TEXT("gml:Envelope"));

    if (BoundaryNode) {
        // Aus dem Boundary Vektor die x und y Koordinaten extrahieren für einen Offset
        FString lowerCorner = BoundaryNode->GetChildrenNodes()[0]->GetContent();
        TArray<FString> OffsetArray;
        lowerCorner.ParseIntoArray(OffsetArray, TEXT(" "), true);

        OffsetVector.X = FCString::Atof(*OffsetArray[0]);
        OffsetVector.Y = FCString::Atof(*OffsetArray[1]);

        OffsetVector.X = 548000.0f;
        OffsetVector.Y = 5935000.0f;

        OffsetVector.Z = 0.0f;
    }

    const FXmlNode* NameNode = CityObjectMembers[0];
    if (NameNode) {
        FString LoD = NameNode->GetContent().Left(4);
        if (LoD == "LoD1") {
            ProcessLoD1(CityObjectMembers, OffsetVector);
        }
        else if (LoD == "LoD2") {
            ProcessLoD2(CityObjectMembers, OffsetVector);
        }
        else {
            UE_LOG(LogTemp, Log, TEXT("LoD nicht unterstuetzt"));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Finished processing CityGML file: %s"), *FilePath);

}

void FCityGMLImporterModule::ProcessLoD1(const TArray<FXmlNode*>& CityObjectMembers, FVector OffsetVector)
{
    // Verarbeite die CityObjectMembers für LoD1
    UE_LOG(LogTemp, Log, TEXT("Processing LoD1"));

    // Iteriere über alle Knoten
    TArray<TArray<TArray<FVector>>> allBuildingsFromFile; // Alle Gebäude
    TArray<TArray<TArray<int32>>> allBuildingsFromFileTriangles;
    TArray<FString> BuildingIds;
    for (FXmlNode* CityObjectMember : CityObjectMembers)
    {
        // Suche nach Building-Knoten
        const FXmlNode* BuildingNode = CityObjectMember->FindChildNode(TEXT("bldg:Building"));
        if (BuildingNode) {

            TArray<TArray<FVector>> BuildingVectors; // Ein Gebäude
            TArray<TArray<int32>> BuildingTriangles;
            FString BuildingID = BuildingNode->GetAttribute(TEXT("gml:id"));

            const FXmlNode* Lod1SolidNode = BuildingNode->FindChildNode(TEXT("bldg:lod1Solid"));
            if (Lod1SolidNode) {

                const FXmlNode* SolidNode = Lod1SolidNode->FindChildNode(TEXT("gml:Solid"));
                if (SolidNode) {

                    const FXmlNode* ExteriorNode = SolidNode->FindChildNode(TEXT("gml:exterior"));
                    if (ExteriorNode) {

                        const FXmlNode* CompositeSurfaceNode = ExteriorNode->FindChildNode(TEXT("gml:CompositeSurface"));
                        if (CompositeSurfaceNode) {

                            // Verarbeite alle gml:surfaceMember-Knoten also die einzelnen Waende oder Decken
                            for (const FXmlNode* SurfaceMemberNode : CompositeSurfaceNode->GetChildrenNodes()) {
                                TArray<FVector> Vertices; // Für eine Fläche
                                TArray<int32> Triangles;
                                const FXmlNode* PolygonNode = SurfaceMemberNode->FindChildNode(TEXT("gml:Polygon"));
                                if (PolygonNode) {
                                    const FXmlNode* PolygonExteriorNode = PolygonNode->FindChildNode(TEXT("gml:exterior"));
                                    if (PolygonExteriorNode) {
                                        const FXmlNode* LinearRingNode = PolygonExteriorNode->FindChildNode(TEXT("gml:LinearRing"));
                                        if (LinearRingNode) {
                                            const FXmlNode* PosListNode = LinearRingNode->FindChildNode(TEXT("gml:posList"));
                                            if (PosListNode) {
                                                FString PosList = PosListNode->GetContent();
                                                TArray<FString> PosArray;
                                                PosList.ParseIntoArray(PosArray, TEXT(" "), true);

                                                for (int32 i = 0; i < PosArray.Num() - 3; i += 3) { // Alle Punkte außer den letzten weil Kreis
                                                    Vertices.Add(ConvertUtmToUnreal(FCString::Atof(*PosArray[i]), FCString::Atof(*PosArray[i + 1]), FCString::Atof(*PosArray[i + 2]), OffsetVector));
                                                }
                                                
                                                if (Vertices.Num() >= 3) { // Polygon mit mindestens 3 Punkten, welche nicht konvex und nicht komplex sind (also Lod1)
                                                    for (int32 k = 1; k < Vertices.Num() - 1; ++k) {
                                                        // Füge Dreiecke hinzu
                                                        Triangles.Add(0);
                                                        Triangles.Add(k);
                                                        Triangles.Add(k + 1); 
                                                    }
                                                    // Berechne die Normalen für das Dreieck
                                                    FVector Vertex1 = Vertices[0];
                                                    FVector Vertex2 = Vertices[1];
                                                    FVector Vertex3 = Vertices[2];

                                                    // Berechne die Kantenvektoren
                                                    FVector Edge1 = Vertex2 - Vertex1;
                                                    FVector Edge2 = Vertex3 - Vertex1;

                                                    // Berechne die Normale durch Kreuzprodukt
                                                    FVector FaceNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

                                                    for (int32 i2 = 0; i2 < Vertices.Num(); i2++) {
                                                        Normalen.Add(FaceNormal);
                                                    }

                                                }
                                            }
                                        }
                                    }
                                }
                                BuildingVectors.Add(Vertices);
                                BuildingTriangles.Add(Triangles);
                            } // Wand bzw. Decke Ende

                        }
                    }
                }
            }
            BuildingIds.Add(BuildingID); // Hier könnte man Daten wie Adresse und Ort mitnehmen
            allBuildingsFromFile.Add(BuildingVectors);
            allBuildingsFromFileTriangles.Add(BuildingTriangles);
        }
    } // Gebäude zuende
    AllBuildings.Append(allBuildingsFromFile);
    AllTriangles.Append(allBuildingsFromFileTriangles);
}

void FCityGMLImporterModule::ProcessLoD2(const TArray<FXmlNode*>& CityObjectMembers, FVector OffsetVector)
{
    // Verarbeite die CityObjectMembers für LoD2
    UE_LOG(LogTemp, Log, TEXT("Processing LoD2"));

    TArray<TArray<TArray<FVector>>> allBuildingsFromFile; // Alle Gebäude
    TArray<TArray<TArray<int32>>> allBuildingsFromFileTriangles;
    TArray<FString> BuildingIds;

    for (FXmlNode* CityObjectMember : CityObjectMembers) {

        // Suche nach Building-Knoten
        const FXmlNode* BuildingNode = CityObjectMember->FindChildNode(TEXT("bldg:Building"));
        if (BuildingNode) {
            TArray<TArray<FVector>> BuildingVectors; // Ein Gebäude
            TArray<TArray<int32>> BuildingTriangles;
            FString BuildingID = BuildingNode->GetAttribute(TEXT("gml:id"));

            for (const FXmlNode* BoundedByNode : BuildingNode->GetChildrenNodes()) {

                if (BoundedByNode->GetTag() == "bldg:boundedBy") { 
                    TArray<FVector> Vertices; // Für eine Fläche
                    TArray<int32> Triangles;

                    const FXmlNode* SurfaceNode = BoundedByNode->GetFirstChildNode();
                    if (SurfaceNode) {

                        const FXmlNode* Lod2MultiSurfaceNode = SurfaceNode->FindChildNode(TEXT("bldg:lod2MultiSurface"));
                        if (Lod2MultiSurfaceNode) {

                            const FXmlNode* MultiSurfaceNode = Lod2MultiSurfaceNode->FindChildNode(TEXT("gml:MultiSurface"));
                            if (MultiSurfaceNode) {

                                const FXmlNode* SurfaceMemberNode = MultiSurfaceNode->FindChildNode(TEXT("gml:surfaceMember"));
                                if (SurfaceMemberNode) {

                                    const FXmlNode* PolygonNode = SurfaceMemberNode->FindChildNode(TEXT("gml:Polygon"));
                                    if (PolygonNode) {

                                        const FXmlNode* ExteriorNode = PolygonNode->FindChildNode(TEXT("gml:exterior"));
                                        if (ExteriorNode) {

                                            const FXmlNode* LinearRingNode = ExteriorNode->FindChildNode(TEXT("gml:LinearRing"));
                                            if (LinearRingNode) {
                                                const FXmlNode* PosListNode = LinearRingNode->FindChildNode(TEXT("gml:posList"));
                                                if (PosListNode) {
                                                    FString PosList = PosListNode->GetContent();
                                                    TArray<FString> PosArray;
                                                    PosList.ParseIntoArray(PosArray, TEXT(" "), true);

                                                    for (int32 i = 0; i < PosArray.Num() - 3; i += 3) { // Alle Punkte außer den letzten weil Kreis
                                                        Vertices.Add(ConvertUtmToUnreal(FCString::Atof(*PosArray[i]), FCString::Atof(*PosArray[i + 1]), FCString::Atof(*PosArray[i + 2]), OffsetVector));
                                                    }

                                                    FVector FaceNormal;
                                                    if (Vertices.Num() >= 3) { // Vermutlich hier erweitert
                                                        for (int32 k = 1; k < Vertices.Num() - 1; ++k) {
                                                            // Füge Dreiecke hinzu
                                                            Triangles.Add(0);
                                                            Triangles.Add(k);
                                                            Triangles.Add(k + 1);
                                                        }

                                                        // Berechne die Normalen für das Dreieck
                                                        FVector Vertex1 = Vertices[0];
                                                        FVector Vertex2 = Vertices[1];
                                                        FVector Vertex3 = Vertices[2];

                                                        // Berechne die Kantenvektoren
                                                        FVector Edge1 = Vertex2 - Vertex1;
                                                        FVector Edge2 = Vertex3 - Vertex1;

                                                        // Berechne die Normale durch Kreuzprodukt
                                                        FaceNormal = FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();

                                                        for (int32 i2 = 0; i2 < Vertices.Num(); i2++) {
                                                            Normalen.Add(FaceNormal);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    BuildingVectors.Add(Vertices);
                    BuildingTriangles.Add(Triangles);
                }
            } // Dach / Bodenflaeche / Wand Ende
            allBuildingsFromFile.Add(BuildingVectors);
            allBuildingsFromFileTriangles.Add(BuildingTriangles);
            BuildingIds.Add(BuildingID);
        }
    } // Gebäude Ende Schleife
    AllBuildings.Append(allBuildingsFromFile);
    AllTriangles.Append(allBuildingsFromFileTriangles);
}

FVector FCityGMLImporterModule::ConvertUtmToUnreal(float UTM_X, float UTM_Y, float UTM_Z, FVector OriginOffset)
{
    // Vertausche die Achsen: X -> Y und Y -> X ( X Osten/ Y Norden)
    // Skalierung 1:100 ohne den Faktor
    float UnrealX = (UTM_Y - OriginOffset.Y) * Skalierung;
    float UnrealY = (UTM_X - OriginOffset.X) * Skalierung;

    return FVector(UnrealX, UnrealY, UTM_Z * Skalierung );
}

void FCityGMLImporterModule::CreateMeshFromPolygon(TArray<TArray<TArray<FVector>>>& Buildings, TArray<TArray<TArray<int32>>>& Triangles, TArray<FString> BuildingIds) {
    // Wenn nicht jedes Gebäude ein Actor sein soll diese Methode raus
    // Und Vertices und Triangles direkt in ein Array
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (World) {
        for (int32 i = 0; i < Buildings.Num(); ++i) { // Jedes Gebäude durchlaufen
            const TArray<TArray<FVector>>& Building = Buildings[i];
            const TArray<TArray<int32>>& BuildingTriangles = Triangles[i];
            FString BuildingID = BuildingIds.IsValidIndex(i) ? BuildingIds[i] : FString::Printf(TEXT("Building_%d"), i);

            AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform());
            if (MeshActor) {
                // Erstelle ein neues ProceduralMeshComponent für das Gebäude
                UProceduralMeshComponent* ProceduralMesh = NewObject<UProceduralMeshComponent>(MeshActor);
                MeshActor->SetRootComponent(ProceduralMesh);
                ProceduralMesh->RegisterComponent();

                // Für jeden Abschnitt des Gebäudes (jede Fläche/Wand)
                for (int32 j = 0; j < Building.Num(); ++j) {
                    const TArray<FVector>& Vertices = Building[j];
                    const TArray<int32>& TrianglesArray = BuildingTriangles[j];

                    ProceduralMesh->CreateMeshSection(j, Vertices, TrianglesArray, TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
                }

                // Setze einen Label oder eine ID für das Actor-Objekt (optional) und Adresse geht in Lod1 auch
                MeshActor->SetActorLabel(BuildingID);
            }
        }
    }
}

void FCityGMLImporterModule::CreateOneMeshFromPolygon(TArray<TArray<TArray<FVector>>>& Buildings, TArray<TArray<TArray<int32>>>& Triangles) { 
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (World) {
        // Ein MeshActor pro File
        AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform());

        if (MeshActor) {
            UProceduralMeshComponent* ProceduralMesh = NewObject<UProceduralMeshComponent>(MeshActor);
            MeshActor->SetRootComponent(ProceduralMesh);
            ProceduralMesh->RegisterComponent();

            TArray<FVector> AllVerticesMesh;
            TArray<int32> AllTrianglesMesh;

            // Offsets für Triangles, pro Gebäude
            int32 VertexOffset = 0;

            for (int32 i = 0; i < Buildings.Num(); ++i) { // Jedes Gebäude
                const TArray<TArray<FVector>>& Building = Buildings[i];
                const TArray<TArray<int32>>& BuildingTriangles = Triangles[i];

                // Für jede Fläche/Wand
                for (int32 j = 0; j < Building.Num(); ++j) {
                    const TArray<FVector>& Vertices = Building[j];
                    const TArray<int32>& TrianglesArray = BuildingTriangles[j];

                    AllVerticesMesh.Append(Vertices);

                    for (int32 TriIndex : TrianglesArray) {
                        AllTrianglesMesh.Add(TriIndex + VertexOffset);
                    }

                    VertexOffset += Vertices.Num();
                }
            }
            
            //UE_LOG(LogTemp, Log, TEXT("Anzahl der Vertices: %d"), AllVerticesMesh.Num());
            //UE_LOG(LogTemp, Log, TEXT("Anzahl der Normalen: %d"), Normalen.Num());

            TArray<FVector2D> UVs;
            for (const FVector& Vertex : AllVerticesMesh)
            {
                UVs.Add(FVector2D(Vertex.X, Vertex.Y));
            }


            ProceduralMesh->CreateMeshSection(0, AllVerticesMesh, AllTrianglesMesh, Normalen, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), true);

            // ID wird noch der Dateiname
            MeshActor->SetActorLabel(TEXT("CityGMLMesh"));
        }
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCityGMLImporterModule, CityGMLImporter)