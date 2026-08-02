---
name: Workflow raster verificabili
description: Regole di sicurezza per evitare bande, scale, NoData e output raster plausibili solo in apparenza.
category: urban-green
tags:
  - raster
  - landsat
  - safety
  - validation
globs:
  - "*.tif"
  - "*.tiff"
  - "*.qgz"
alwaysApply: true
source: {"kind":"github","url":"https://github.com/francemazzi/strata","title":"Strata GIS repository","author":"Strata GIS team","year":2026}
license: CC-BY-4.0
attribution: Regola originale del progetto Strata; costanti Landsat verificate sulla documentazione U.S. Geological Survey.
trustLevel: VERIFIED
enabled: true
references:
  - https://www.usgs.gov/faqs/how-do-i-use-a-scale-factor-landsat-level-2-science-products
  - https://www.usgs.gov/landsat-missions/landsat-collection-2-quality-assessment-bands
---

# Workflow raster verificabili

Prima di eseguire un calcolo raster:

1. identifica sensore, Collection, livello prodotto, banda, unità, scale, offset e NoData da metadati o sidecar;
2. non riutilizzare costanti di un prodotto diverso e non indovinare una banda mancante;
3. applica maschere QA e scaling prima degli indici che lavorano su valori fisici;
4. preferisci `native:rastercalc` e algoritmi Processing disponibili; non presumere pandas o GDAL CLI nell’ambiente Strata;
5. esegui `fixgeometries` sulle cutline non validate prima di clip e statistiche zonali;
6. controlla CRS e trasformazioni, soprattutto quando `authid()` è vuoto;
7. verifica range fisici, NoData, estensione, risoluzione e statistiche dell’output;
8. se NDVI/NDBI escono da `[-1, 1]`, interrompi e correggi input o scaling: non mascherare l’errore con un clamp;
9. conserva formula, parametri, sorgenti e assunzioni nel log o nel report finale.

Chiedi conferma prima di sostituire output esistenti. Un raster visivamente credibile non è una verifica sufficiente.
