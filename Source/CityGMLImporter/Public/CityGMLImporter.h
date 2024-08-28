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

	/**
	 * Fügt einen Menüeintrag in das Unreal Engine Editor-Menü hinzu,
	 * der es ermöglicht, CityGML-Dateien auszuwählen.
	 * 
	 * @param Builder Referenz auf den `FMenuBuilder`, welcher für den Aufbau des Menüs verwendet wird.
	 */
	void AddMenuExtension(FMenuBuilder& Builder);
	/**
	 * Öffnet XML- und GML-Dateien, setzt globale Variablen zurück und ruft für jede Datei die Methode `ProcessCityGML` auf.
	 */
	void PluginButtonClicked();
	/**
	 * Liest eine Datei mit dem nativen XML-Parser der Unreal Engine ein
	 * und wandelt diese in eine Node-Struktur um.
	 * Überprüft, ob es sich um CityGML handelt, und bestimmt den Startpunkt für die Anzeige der Gebäude.
	 * Am Ende wird geprüft, welches LoD (Level of Detail) vorliegt,
	 * um dann das passende LoD zu verarbeiten oder eine Fehlermeldung auszugeben.
	 *
	 * @param FilePath Pfad zur CityGML Datei
	 */
	void ProcessCityGML(const FString& FilePath);
	/**
	 * Verarbeitet CityGML LoD1-Daten.
	 *
	 * Durchläuft die Knoten, um die Vertices auszulesen.
	 * Mithilfe von Hilfsfunktionen können aus den Vertices weitere Werte berechnet werden.
	 * Die Gebäudedaten werden in Arrays gespeichert und in globale Variablen geschrieben.
	 * 
	 * @param CityObjectMembers Referenz auf die Children des RootNodes
	 * @param OffSetVector Ein `FVector`, der verwendet wird, um die Position der Gebäude in der Welt zu verschieben.
	 */
	void ProcessLoD1(const TArray<FXmlNode*>& CityObjectMembers, FVector Offsetvector);
	/**
	 * Verarbeitet CityGML LoD2-Daten.
	 *
	 * Durchläuft die Knoten, um die Vertices auszulesen.
	 * Mithilfe von Hilfsfunktionen können aus den Vertices weitere Werte berechnet werden.
	 * Die Gebäudedaten werden in Arrays gespeichert und in globale Variablen geschrieben.
	 *
	 * @param CityObjectMembers Referenz auf die Children des RootNodes
	 * @param OffSetVector Ein `FVector`, der verwendet wird, um die Position der Gebäude in der Welt zu verschieben.
	 */
	void ProcessLoD2(const TArray<FXmlNode*>& CityObjectMembers, FVector Offsetvector);
	/**
	 * Verarbeitet CityGML LoD3-Daten.
	 *
	 * Durchläuft die Knoten, um die Vertices auszulesen.
	 * Mithilfe von Hilfsfunktionen können aus den Vertices weitere Werte berechnet werden.
	 * Die Gebäudedaten werden in Arrays gespeichert und in globale Variablen geschrieben, nur die Addressdaten liegen in LoD3 noch nicht vor.
	 *
	 * @param CityObjectMembers Referenz auf die Children des RootNodes
	 * @param OffSetVector Ein `FVector`, der verwendet wird, um die Position der Gebäude in der Welt zu verschieben.
	 */
	void ProcessLoD3(const TArray<FXmlNode*>& CityObjectMembers, FVector Offsetvector);
	/**
	 * Konvertiert die Koordianten aus CityGMl, welcher im ETRS89_UTM32 Format vorliegen in Unreal Engine Koordinaten.
	 * Dafür werden X und Y vertauscht, die Sklaierung miteinbezogen und der OffsetVektor berücksichtigt.
	 * 
	 * @param UTM_X X Koordinate aus ETRS89_UTM32
	 * @param UTM_Y Y Koordiante aus ETRS89_UTM32
	 * @param UTM_Z Z Koordinate aus ETRS89_UTM32
	 * @param OriginOffset Startpunktsvektor, der als Offset für die Umrechnung dient.
	 * @return Ein `FVector`, der die umgerechneten Koordinaten für die Unreal Engine repräsentiert.
	 */
	FVector ConvertUtmToUnreal(float UTM_X, float UTM_Y, float UTM_Z,  FVector OriginOffset);
	/**
	 * Erstellt und spawnt statische Mesh-Objekte in der Unreal Engine basierend auf den gegebenen Gebäude-Polygonen und Dreieckslisten.
	 * Jedes Gebäude wird als ein eigenes AStaticMeshActor-Objekt erstellt.
	 *
	 * @param Buildings Ein TArray von Gebäuden, wobei jedes Gebäude eine Liste von Polygonen enthält, die wiederrum durch FVector-Arrays repräsentiert werden.
	 * @param Triangles Ein TArray von Dreiecklisten, die die Dreiecke der Polygone definieren.
	 * @param BuildingIds Ein TArray<FString>, das die ID der Gebäude enthält.
	 */
	void CreateMeshFromPolygon(TArray<TArray<TArray<FVector>>>& Buildings, TArray<TArray<TArray<int32>>>& Triangles, TArray<FString> BuildingIds);
	/**
	 * Erstellt ein einziges Mesh aus mehreren Gebäude-Polygonen und Dreieckslisten.
	 * Die Gebäude werden in ein einzelnes Mesh kombiniert, das als ein einziges statisches Mesh-Objekt in der Unreal Engine dargestellt wird.
	 *
	 * @param Buildings Ein TArray von Gebäuden, wobei jedes Gebäude eine Liste von Polygonen enthält, die wiederrum durch FVector-Arrays repräsentiert werden.
	 * @param Triangles Ein TArray von Dreiecklisten, die die Dreiecke der Polygone definieren.
	 */
	void CreateOneMeshFromPolygon(TArray<TArray<TArray<FVector>>>& Buildings, TArray<TArray<TArray<int32>>>& Triangles);
	/**
	 * Liest die Koordinaten eines Polygons aus einem CityGML-Dokument und konvertiert sie in Unreal Engine-Koordinaten unter Berücksichtigung des Offset-Vektors.
	 * Diese Methode kann von allen ProcessLoD Methoden durch die ähnliche Strucktur von CityGML genutzt werden und nutzt selbst ConvertUtmToUnreal.
	 *
	 * @param PolygonNode Ein FXmlNode, das das Polygon-Element des CityGML-Dokuments repräsentiert.
	 * @param OffsetVector Ein FVector, der den Verschiebungsvektor repräsentiert, der auf die Koordinaten angewendet wird.
	 * @return Ein TArray<FVector>, das die konvertierten Koordinaten des Polygons als TArray zurückgibt.
	 */
	TArray<FVector> ParsePolygon(const FXmlNode* PolygonNode, FVector OffsetVector);
	/**
	 * Generiert aus dem TArray an Vertices welches mitgegeben wird eine Liste von Indizes, die die Dreiecke des Polygons definieren.
	 * Der Fan-Algorithmus wird verwendet, um die Dreiecke aus den gegebenen Vertices zu erstellen.
	 *
	 * @param Vertices Ein TArray<FVector>, das die Eckpunkte des Polygons enthält.
	 * @return Ein TArray<int32>, das die Indizes der Vertices enthält, die die Dreiecke des Polygons definieren.
	 */
	TArray<int32> GenerateTriangles(const TArray<FVector>& Vertices);
	/**
	 * Berechnet den Normalenvektor für eine Fläche basierend auf den gegebenen Vertices.
	 * Der Vektor wird so oft in ein Normalen Array eingetragen, wie die Fläche Vertices besitzt.
	 *
	 * @param Vertices Ein TArray<FVector>, das die Eckpunkte des Polygons enthält.
	 */
	void GenerateNormals(const TArray<FVector>& Vertices);
	/**
	 * Generiert die UV-Koordinaten für die Vertices einer Fläche, die für die Texturierung in Unreal Engine verwendet werden.
	 *
	 * @param Vertices Ein TArray<FVector>, das die Eckpunkte des Polygons enthält.
	 */
	void GenerateUVs(const TArray<FVector>& Vertices);
	/**
	 * Berechnet die Tangenten für die Vertices einer Fläche, die für die Normalen- und Texturierungseffekte in Unreal Engine verwendet werden.
	 *
	 * @param Vertices Ein TArray<FVector>, das die Eckpunkte des Polygons enthält.
	 * @param Triangles Ein TArray<int32>, das die Indizes der Vertices enthält, die die Dreiecke der Fläche definieren.
	 */
	void GenerateTangents(const TArray<FVector>& Vertices, const TArray<int32> Triangles);
	 /**
	  * Extrahiert Adressinformationen wie Straße, Hausnummer und Postleitzahl aus CityGML Dokumenten.
	  * Die Methode durchsucht den mitgegebenen XML-Baum nach den relevanten Adressdaten und gibt diese in einem Array zurück.
	  *
	  * @param AddressNode Ein FXmlNode, der den Knoten des CityGML-Dokuments enthält, der die Adressinformationen repräsentiert.
	  * @return Ein TArray<FString>, das die extrahierten Adressinformationen enthält: Straße, Hausnummer und Postleitzahl.
	  *									Wenn eine Information fehlt, wird an dieser Stelle ein leerer String zurückgegeben.
	  */
	TArray<FString> GetAdress(const FXmlNode* AddressNode);



	TSharedPtr<FUICommandList> PluginCommands;
};
