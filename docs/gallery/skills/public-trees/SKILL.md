---
name: Censimento alberi pubblici
description: Usa questa skill per censire alberi pubblici in Lombardia (filari e parchi) tramite trees_detect. Non è un indice 3-30-300.
category: urban-green
tags:
  - public-trees
  - urban-forestry
  - cadastre
  - lombardia
  - deepforest
source: {"kind":"paper","url":"https://doi.org/10.48044/jauf.2012.024","doi":"10.48044/jauf.2012.024","title":"Urban tree growth modeling","author":"E. Gregory McPherson and Paula J. Peper","year":2012}
license: CC-BY-4.0
attribution: Allometria chioma v1 inquadrata su McPherson & Peper (2012); catasto da zip ufficiali Agenzia delle Entrate; detection DeepForest pretrained.
trustLevel: CURATED
enabled: true
references:
  - https://doi.org/10.48044/jauf.2012.024
  - https://www.agenziaentrate.gov.it/portale/accedi-al-servizio-cartografici
---

# Censimento alberi pubblici

## Quando usarla

Attivala per un **censimento di individui** in ambito pubblico: filari stradali, parchi, preventivi di endoterapia, PNR verde, alberi di pregio. Resta in chat. Non chiedere di aprire Processing o la Toolbox.

Non usarla per indici Landsat, copertura arborea percentuale o la regola 3-30-300: quello è `urban-green-nbs`.

v1 è **solo Lombardia**. Fuori regione spiega il limite e non improvvisare un detector locale.

## 1. Chiama `trees_detect`

1. Prendi il bbox del comune o del canvas (`get_active_canvas_extent` se serve).
2. Chiama `trees_detect` con `region: "lombardia"` e `format: "geojson"`.
3. Attendi il job. Poi `download_file` con `expected_sha256` e `add_layer_from_file`.
4. Non ricalcolare la detection in `run_python` e non lanciare DeepForest sul desktop.

Il backend usa lo zip catastale regionale AdE (`agenzia-entrate-catasto-lombardia` / `LOMBARDIA.zip`), una maschera pubblico/privato, ortofoto regionale e DeepForest pretrained. Il bbox viene ritagliato sul worker, non in app.

## 2. Cosa tenere e cosa escludere

Il layer deve descrivere alberi **pubblici** su filare (`street_row`) o parco (`park`).

- Escludi particelle private.
- Escludi canopy chiusa.
- In v1 contano solo individui con stima **altezza ≥ 6 m** e **DBH > 45 cm**. Alberi più piccoli possono mancare.
- `height_m_est` e `dbh_cm_est` sono **stime da diametro di chioma** (`estimate=true`), non misure da LiDAR o da campo. Cita McPherson & Peper (2012) e le proxy lineari `height ≈ 1.1 × crown_m`, `dbh_cm ≈ 7 × crown_m`.

## 3. Privacy e fonti

- Non usare, non allegare e non citare il file `Censimento_comunale.gpkg` di Casalmaggiore o altri ground-truth comunali sensibili.
- Non chiedere all'utente di caricare un censimento per "validare" il modello in chat.
- Dopo il layer: stile per contesto, conteggi filare/parco, nota metodologica (soglie, stima, esclusione privati e canopy chiusa, fonte catasto AdE).

## 4. Consegna

Restituisci il layer in progetto, i conteggi e i limiti. Se l'area è fuori Lombardia, o se l'utente chiede specie, sanità, Street View o fine-tuning, dillo come fuori da v1.
