---
name: Verde urbano e Nature-Based Solutions
description: Usa questa skill per impostare analisi riproducibili di verde urbano, copertura arborea, indici Landsat e accessibilità alle aree verdi.
category: urban-green
tags:
  - nbs
  - urban-forestry
  - landsat
  - ndvi
  - ndbi
  - surface-temperature
source: {"kind":"paper","url":"https://doi.org/10.3390/ijgi15060256","doi":"10.3390/ijgi15060256","title":"Beyond the 3-30-300 Rule: Construction of a Scalable Composite Index for the Evaluation of Urban Green—The Ferrara Case Study","author":"Giovanna Galeota Lanza et al.","year":2026}
license: CC-BY-4.0
attribution: Metodo 3-30-300 composito adattato da Galeota Lanza et al. (2026); fattori Landsat da U.S. Geological Survey.
trustLevel: CURATED
enabled: true
references:
  - https://doi.org/10.3390/ijgi15060256
  - https://www.usgs.gov/faqs/how-do-i-use-a-scale-factor-landsat-level-2-science-products
  - https://www.usgs.gov/landsat-missions/landsat-collection-2-quality-assessment-bands
---

# Verde urbano e Nature-Based Solutions

## Quando usarla

Usa questa procedura per costruire un dossier territoriale su vegetazione, calore urbano e accesso al verde. Il risultato deve restare verificabile: ogni soglia, banda e fonte va dichiarata nel report e collegata al proprio intervallo di validità.

## 1. Blocca il prodotto prima delle formule

1. Leggi nome scena e file MTL. Distingui sensore, Collection, livello e variante prodotto; non dedurli dalla sola presenza di una banda.
2. Per Landsat Collection 2 Level-2 applica i fattori ufficiali prima degli indici:
   - riflettanza superficiale: `SR = DN × 0.0000275 - 0.2`;
   - temperatura superficiale: `ST_K = DN × 0.00341802 + 149.0`, poi `ST_C = ST_K - 273.15`.
3. Escludi fill, cloud, cloud shadow e pixel saturi usando le bande QA pertinenti. Non trattare il valore DN `0` come osservazione valida.
4. Verifica i nomi banda del sensore. Per Landsat 8/9 OLI, RED è normalmente `SR_B4`, NIR `SR_B5`, SWIR1 `SR_B6` e la temperatura Level-2 `ST_B10`; per altri sensori ricava la mappatura dalla documentazione del prodotto.

## 2. Calcola e verifica gli indicatori raster

- `NDVI = (NIR - RED) / (NIR + RED)`
- `NDBI = (SWIR1 - NIR) / (SWIR1 + NIR)`

Usa `native:rastercalc` o API PyQGIS disponibili in Strata. Evita dipendenze implicite da pandas e comandi GDAL esterni. Prima di consegnare:

- controlla che NDVI e NDBI validi ricadano in `[-1, 1]`;
- non applicare un clamp per nascondere valori impossibili: un range errato indica spesso scaling, banda o NoData sbagliati;
- riporta min, max, media, percentili e quota di NoData;
- confronta un campione con ortofoto o altra fonte indipendente.

## 3. Costruisci il quadro 3-30-300 come indice, non come semaforo

La regola originaria è un punto di partenza, non una soglia universale. Organizza tre indicatori per edificio o unità di analisi:

- **3 — visibilità:** presenza e qualità del verde osservabile, dichiarando dati e approssimazioni usati;
- **30 — copertura arborea:** percentuale di canopy nel contesto scelto, con buffer e anno del dato espliciti;
- **300 — prossimità:** distanza di rete o pedonale verso un’area verde realmente accessibile, non semplice distanza euclidea se la rete è disponibile.

Normalizza gli indicatori, documenta eventuali pesi e conserva anche i tre valori separati. Integra popolazione o gruppi vulnerabili solo con dati aggregati adeguati alla scala: il punteggio composito non deve nascondere disuguaglianze locali.

## 4. Integra fonti territoriali con controlli espliciti

- Per DBGT, PGRA, ISTAT e canopy esterni verifica sempre versione, CRS, data, licenza e dizionario attributi.
- Non assumere che campi come `_SUP` o `TRITORNO` abbiano identico significato tra forniture: conferma la semantica nei metadati regionali prima del filtro.
- Esegui `fixgeometries` prima di cutline, overlay o statistiche zonali quando l’integrità geometrica non è già dimostrata.
- Per risorse remote via `/vsicurl/`, registra URL, data di accesso e fallback locale riproducibile.

## 5. Consegna

Produci layer intermedi nominati, tabella indicatori, mappa finale e nota metodologica. La nota deve elencare prodotto satellitare, scaling, maschere QA, formule, unità spaziale, fonti, limiti e controlli di plausibilità. Se un requisito non è verificabile, segnalalo come assunzione invece di trasformarlo in un risultato certo.
