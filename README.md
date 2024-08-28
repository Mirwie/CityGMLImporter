# CityGMLImporter

# Übersicht

Das **CityGML Importer Plugin** für die Unreal Engine ermöglicht das Einlesen und Verarbeiten von CityGML-Dateien direkt in der Unreal Engine.
CityGML ist ein standardisiertes Datenmodell zur Speicherung und Darstellung von 3D-Stadtmodellen.
Dieses Plugin unterstützt die Level of Detail (LoD) 1 bis 3.
und bietet eine Integration in den Unreal Engine Editor.

## Funktionen

- **CityGML-Datei Import**: Ermöglicht das Laden von .gml und .xml Dateien direkt in die Unreal Engine.
- **Unterstützung für LoD1, LoD2, und LoD3**: Verarbeitet diese 3 unterschiedlichen Detailstufen von Gebäudemodellen.
- **Konvertierung von ETRS89_UTM32 zu Unreal-Koordinaten**: Automatische Transformation der Stadtmodell-Koordinaten in UE Koordinaten.
- **Erstellung von 3D-Modellen**: Generiert statische Meshes aus den importierten Gebäudedaten.
- **Editor Integration**: Fügt den Menüeintrag für CityGML-Dateien in den Unreal Engine Editor ein.

## Installation

1. Klone dieses Repository in das Plugins-Verzeichnis deines Unreal Engine Projekts:
   ```bash
   git clone https://github.com/dein-username/CityGMLImporter.git Plugins/CityGMLImporter

2. Öffne das Projekt in Unreal Engine.
Das Plugin wird automatisch erkannt und kann über den Plugin-Manager aktiviert werden.

3. Stelle sicher, dass das Plugin aktiviert ist und starte das Projekt neu, um alle Änderungen zu übernehmen.

## Verwendung

1. Öffne den Unreal Engine Editor.

2. Wähle im Menü File > Select CityGML File aus. 

3. Wähle eine CityGML-Datei (.gml oder .xml) aus.

4. Die importierten Gebäude werden automatisch in die Szene eingefügt und angezeigt.

In der Klasse CityGMLImporter.cpp können oben bei den globalen Variablen folgende Variablen angepasst werden:
* Skalierung (float), skaliert die Stadt auf ein 1:1 Verhältnis, wenn der Wert auf 100 gesetzt ist.
* OneMesh (boolean), bei true wird ein Mesh pro "Button Click" erzeugt und bei false werden pro Gebäude Meshes erstellt.

## Voraussetzungen
* Unreal Engine 4.27 .
* C++ Kenntnisse, wenn man es selbst anpassen möchte.