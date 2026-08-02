---
name: region-lombardia
description: >
  Livello regionale (Regione Lombardia) della gerarchia normativa Stato/Regione/Comune per
  architettura e urbanistica in Italia -- si applica in aggiunta a `country-italy`, che resta
  il riferimento per la normativa statale. Copre la LR 12/2005 (Legge per il governo del
  territorio) e il PGT tripartito (Documento di Piano, Piano dei Servizi, Piano delle Regole),
  la LR 31/2014 sul consumo di suolo, la LR 18/2019 sulla rigenerazione urbana, il Piano Casa
  lombardo (LR 13/2009 e proroghe), il PTR e il PPR (Piano Paesaggistico Regionale) con Rete
  Ecologica Regionale, il PTM della Città Metropolitana di Milano, la componente geologica
  idrogeologica e sismica obbligatoria del PGT (art. 57 LR 12/2005), la classificazione
  sismica dei comuni lombardi, il reticolo idrico e la polizia idraulica, il Regolamento
  Regionale 6/2017 sulla prestazione energetica e il portale CENED+2.0, e le fonti ufficiali
  regionali (BURL, geoportale SIT Lombardia, TAR Lombardia) per norme, delibere, sentenze e
  pareri aggiornati.
---

# Regione: Lombardia

**Ultimo aggiornamento contenuto**: 29 luglio 2026. Verificare sempre le fonti ufficiali regionali (Sezione 6) per norme, delibere o sentenze successive a questa data.

Livello **Regione** della gerarchia Stato/Regione/Comune (vedi `country-italy` Sezione 1 per il quadro generale delle competenze). Questa skill si attiva insieme a `country-italy` ogni volta che il progetto ricade in un Comune della Regione Lombardia: la normativa statale (TUE, NTC, Codice Beni Culturali, ecc.) resta interamente valida e va applicata per prima; questa skill aggiunge lo strato di dettaglio regionale che la integra o specifica.

**Stato del popolamento RAG**: al momento è popolata solo la Lombardia, come prova pilota. Le altre regioni italiane non sono ancora coperte -- se il progetto ricade fuori Lombardia, usare solo `country-italy` e segnalare che il livello regionale specifico non è ancora disponibile.

---

## 1. Legge per il governo del territorio -- LR 12/2005

**Legge Regionale 11 marzo 2005, n. 12** è la fonte primaria del governo del territorio in Lombardia, sostituisce integralmente la precedente disciplina urbanistica regionale (LR 51/1975) e recepisce/specifica il TUE a livello locale. Più volte modificata; le riforme più rilevanti sono la LR 31/2014 (consumo di suolo) e la LR 18/2019 (rigenerazione urbana), trattate nelle Sezioni 2-3.

### 1.1 Strumenti di pianificazione (art. 6 LR 12/2005)

```
PTR -- Piano Territoriale Regionale (Regione)
  |-- PPR -- Piano Paesaggistico Regionale (sezione tematica del PTR)
PTM -- Piano Territoriale Metropolitano (Città Metropolitana di Milano, ex PTCP)
PTCP -- Piano Territoriale di Coordinamento Provinciale (altre Province lombarde)
PGT -- Piano di Governo del Territorio (Comune) -- tripartito:
  |-- Documento di Piano (DdP)
  |-- Piano dei Servizi (PdS)
  |-- Piano delle Regole (PdR)
```

### 1.2 Il PGT tripartito (art. 7-10 LR 12/2005)

| Componente | Contenuto | Validità |
|---|---|---|
| **Documento di Piano (DdP)** | Quadro conoscitivo, obiettivi strategici, ambiti di trasformazione, dimensionamento del piano | 5 anni, decade se non attuato (art. 8 c.4) |
| **Piano dei Servizi (PdS)** | Dotazione di aree e attrezzature pubbliche/di interesse pubblico o generale (standard urbanistici) | A tempo indeterminato |
| **Piano delle Regole (PdR)** | Disciplina il tessuto urbano consolidato: destinazioni d'uso, indici edificatori, tipologie, distanze, aree agricole e non soggette a trasformazione | A tempo indeterminato |

**Nota pratica**: a differenza del PdS e del PdR (efficacia a tempo indeterminato, natura di piano regolatore), il Documento di Piano ha natura strategico-programmatica e **decade dopo 5 anni** se gli ambiti di trasformazione non vengono attuati -- verificare sempre la data di approvazione/vigenza del DdP prima di fare affidamento su un ambito di trasformazione.

### 1.3 Procedimento di approvazione (art. 13 LR 12/2005)

1. Adozione da parte del Consiglio Comunale
2. Deposito e pubblicazione (avviso su quotidiani/BURL)
3. Termine per **osservazioni** dei cittadini (30 gg dalla pubblicazione)
4. Controdeduzioni e approvazione definitiva del Consiglio Comunale
5. Pubblicazione sul BURL -- efficacia dal quindicesimo giorno successivo

### 1.4 Consumo di suolo -- LR 31/2014

**Legge Regionale 28 novembre 2014, n. 31** ("Disposizioni per la riduzione del consumo di suolo e per la riqualificazione del suolo degradato") -- riforma strutturale:
- Limita le nuove espansioni urbanistiche su suolo libero, imponendo soglie regionali/provinciali di riduzione progressiva del consumo di suolo
- Introduce priorità per il **recupero e la rigenerazione** del tessuto urbano consolidato e delle aree dismesse rispetto a nuove espansioni
- I Comuni devono adeguare il PGT (in particolare il DdP) ai criteri regionali di contenimento del consumo di suolo
- Ambiti di trasformazione su suolo libero non attuati entro i termini decadono e non sono automaticamente riproponibili

### 1.5 Rigenerazione urbana -- LR 18/2019

**Legge Regionale 26 novembre 2019, n. 18** ("Misure di semplificazione e incentivazione per la rigenerazione urbana e territoriale, nonchè per il recupero del patrimonio edilizio esistente") -- introduce, tramite modifica della LR 12/2005:
- **Ambiti di rigenerazione urbana**: individuati dai Comuni nel PGT o tramite delibera dedicata (aree dismesse, degradate, di riqualificazione)
- **Premialità volumetriche** (incrementi di indice edificatorio) per interventi di rigenerazione rispetto a nuova espansione
- **Riduzione oneri di urbanizzazione** per interventi qualificati come rigenerazione
- Semplificazioni procedurali per il recupero del patrimonio edilizio esistente, incluse deroghe puntuali a distanze/altezze per il solo adeguamento energetico/sismico (verificare disciplina comunale di dettaglio, spesso demandata a delibera comunale attuativa)

### 1.6 Piano Casa Lombardia -- LR 13/2009 e successive proroghe

**Legge Regionale 13/2009** (nata come misura anticongiunturale) e successive leggi di proroga/modifica: consente, a determinate condizioni e nei limiti fissati di volta in volta dalla legge vigente, **ampliamenti volumetrici una tantum** di edifici residenziali esistenti (percentuali variabili sulla volumetria/SLP esistente) in deroga puntuale agli strumenti urbanistici comunali, salvo esclusioni per immobili vincolati o in ambiti di particolare tutela. Le soglie percentuali e le finestre temporali sono state più volte modificate: **verificare sempre il testo vigente su Normattiva/BURL**, poiché la norma nasce come strumento emergenziale a termine e viene periodicamente riproposta con condizioni diverse.

---

## 2. Pianificazione territoriale sovraordinata

### 2.1 PTR -- Piano Territoriale Regionale

Approvato con **Deliberazione Consiglio Regionale n. 951/2010** e aggiornato periodicamente. Contiene: quadro strategico regionale, Rete Verde Regionale, Rete Ecologica Regionale (RER), obiettivi di sviluppo territoriale sovracomunale. Ha efficacia di orientamento e, per alcuni contenuti (tra cui il PPR), di prescrizione diretta sui piani comunali.

### 2.2 PPR -- Piano Paesaggistico Regionale

Sezione tematica del PTR, integrata ai sensi del D.Lgs 42/2004 (Codice Urbani, vedi `country-italy` Sezione 8). Individua:
- Ambiti ed elementi di tutela paesaggistica di livello regionale (oltre ai vincoli statali ex art. 136/142 Codice Urbani)
- **Rete Ecologica Regionale (RER)** -- corridoi ecologici, aree prioritarie per la biodiversità, da tenere in conto nella progettazione di ambiti di trasformazione
- Indirizzi di tutela per i paesaggi agrari, i laghi (fascia dei Grandi Laghi lombardi), le valli alpine e prealpine

### 2.3 PTM -- Piano Territoriale Metropolitano (Città Metropolitana di Milano)

Sostituisce il PTCP della ex Provincia di Milano a seguito della **LR 32/2015** (istituzione Città Metropolitana). Il PTM vigente della Città Metropolitana di Milano è stato approvato nel 2021: contiene disposizioni sovraordinate su mobilità, consumo di suolo, aree agricole strategiche, rischio idrogeologico, vincolanti per i PGT dei comuni dell'area metropolitana milanese.

### 2.4 PTCP -- altre Province lombarde

Le altre Province lombarde (Bergamo, Brescia, Como, Cremona, Lecco, Lodi, Mantova, Monza e Brianza, Pavia, Sondrio, Varese) mantengono il proprio **Piano Territoriale di Coordinamento Provinciale (PTCP)**, che detta indirizzi sovracomunali su viabilità, rischio idrogeologico, aree agricole di interesse strategico, paesaggio. Verificare sempre il PTCP della Provincia specifica, poiché contenuti e grado di prescrittività variano.

---

## 3. Componente geologica, idrogeologica e sismica del PGT

### 3.1 Obbligo ex art. 57 LR 12/2005

Ogni PGT lombardo deve essere corredato da uno **studio geologico, idrogeologico e sismico comunale**, che classifica il territorio comunale in **classi di fattibilità geologica** (di norma 1-4, da nessuna limitazione a fattibilità con gravi limitazioni), sulla base della normativa tecnica regionale attuativa (criteri D.G.R. periodicamente aggiornati).

| Classe di fattibilità | Significato indicativo |
|---|---|
| Classe 1 | Fattibilità senza particolari limitazioni |
| Classe 2 | Fattibilità con modeste limitazioni |
| Classe 3 | Fattibilità con consistenti limitazioni (approfondimenti geologici obbligatori in fase progettuale) |
| Classe 4 | Fattibilità con gravi limitazioni (edificazione subordinata a interventi di riqualificazione/messa in sicurezza, o esclusa) |

### 3.2 Classificazione sismica dei comuni lombardi

La classificazione sismica comunale in Lombardia è definita da delibere regionali attuative dell'OPCM 3274/2003 (aggiornate nel tempo, ultima riclassificazione organica **D.G.R. IX/2129 del 2014** e successivi aggiornamenti puntuali). Indicazioni generali:

| Zona sismica | Aree tipiche in Lombardia |
|---|---|
| Zona 2 | Porzioni di Oltrepò Pavese, alta Val Camonica, area del Garda bresciano |
| Zona 3 | Gran parte della fascia prealpina e collinare (Bergamasco, Brianza, Comasco, Bresciano non di zona 2) |
| Zona 4 | Gran parte della Pianura Padana lombarda (bassa sismicità) |

**Verificare sempre la classificazione puntuale del singolo Comune** (non desumerla dalla sola area geografica indicativa) tramite il geoportale regionale o la D.G.R. vigente, poiché la classificazione è per Comune e alcuni aggiornamenti sono intervenuti nel tempo.

### 3.3 PAI -- Piano di Assetto Idrogeologico

Il **PAI dell'Autorità di Bacino Distrettuale del Fiume Po** (già Autorità di Bacino del Po) individua fasce fluviali (A, B, C) e aree a rischio idrogeologico che si sovrappongono come vincolo sovraordinato al PGT comunale; le prescrizioni PAI prevalgono sulla pianificazione comunale nelle aree interessate.

### 3.4 Reticolo idrico e polizia idraulica

- **Reticolo idrico principale** -- competenza regionale/AIPO (Agenzia Interregionale per il fiume Po), corsi d'acqua di rilevanza sovracomunale
- **Reticolo idrico minore** -- competenza comunale, disciplinato da un **Regolamento di Polizia Idraulica comunale** con fasce di rispetto (tipicamente 4-10 m dal ciglio di sponda, variabile per regolamento comunale) entro cui l'edificazione è vietata o soggetta ad autorizzazione idraulica
- Individuazione del reticolo tramite lo **Studio del reticolo idrico minore**, allegato obbligatorio degli strumenti urbanistici comunali

---

## 4. Prestazione energetica -- Regolamento Regionale 6/2017

### 4.1 Quadro normativo regionale

- **Regolamento Regionale 18 luglio 2017, n. 6** ("Regolamento recante criteri e metodi per il rispetto dei requisiti di prestazione energetica degli edifici e per la presentazione, la produzione e la validazione dei relativi attestati") -- attua in Lombardia il D.Lgs 192/2005 e il D.Lgs 48/2020 (vedi `country-italy` Sezione 7), specificando requisiti minimi regionali, spesso più stringenti o più dettagliati della sola normativa statale
- Sostituisce/aggiorna il precedente R.R. 6/2015

### 4.2 CENED+2.0

**CENED+2.0** è il **Catasto Energetico degli Edifici della Regione Lombardia** (portale cened.it): in Lombardia, a differenza di molte altre regioni, la produzione e il deposito dell'APE avvengono **obbligatoriamente tramite questo portale regionale dedicato**, con procedura di validazione informatica specifica. Il soggetto certificatore deve essere accreditato secondo i criteri regionali.

---

## 5. Regolamento Edilizio e prassi amministrativa

### 5.1 Regolamento Edilizio Tipo in Lombardia

Regione Lombardia ha recepito il **Regolamento Edilizio Tipo (RET)** nazionale (Intesa Stato-Regioni 20/10/2016, vedi `country-italy` Sezione 9) con propria delibera di Giunta Regionale, fissando le definizioni tecniche uniformi (Superficie Utile, Superficie Coperta, Volume, Altezza, Sagoma, ecc.). I singoli Comuni lombardi hanno l'obbligo di adeguare il proprio Regolamento Edilizio comunale a tali definizioni uniformi tramite le proprie **Norme Tecniche di Attuazione (NTA)**, integrando la parte seconda (disposizioni regolamentari comunali) del RET.

### 5.2 Pratiche edilizie telematiche

In Lombardia la generalità dei Comuni richiede la presentazione delle pratiche edilizie (CILA, SCIA, PdC) tramite **piattaforme telematiche** (portali comunali o consorziati, es. sistemi basati su iterPRO o analoghi) integrate con il **SUE (Sportello Unico per l'Edilizia)** comunale. La pubblicazione degli strumenti urbanistici comunali vigenti è consultabile tramite il **geoportale regionale** (Sezione 6).

### 5.3 Distanze e altezze

Il Piano delle Regole del PGT disciplina puntualmente distanze, altezze e indici per ogni ambito; in assenza di disciplina comunale più specifica restano applicabili i parametri nazionali (art. 9 D.M. 1444/1968, art. 873 c.c. -- vedi `country-italy` Sezione 3). Verificare sempre le NTA del PdR del Comune specifico prima di applicare i parametri nazionali di default.

---

## 6. Fonti ufficiali regionali per aggiornamenti

- **BURL** (Bollettino Ufficiale Regione Lombardia) -- burl.regione.lombardia.it -- pubblicazione leggi regionali, regolamenti, delibere
- **Normelombardia** -- normelombardia.consiglio.regione.lombardia.it -- testo vigente consolidato delle leggi regionali lombarde (equivalente regionale di Normattiva)
- **Geoportale Regione Lombardia** -- geoportale.regione.lombardia.it -- SIT, mosaico PGT comunali (MUTA), classificazione sismica, PAI, reticolo idrico, PPR
- **CENED** -- cened.it -- portale APE Lombardia
- **TAR Lombardia** (sedi di Milano e Brescia) -- giustizia-amministrativa.it -- giurisprudenza amministrativa di primo grado per i Comuni lombardi
- **Regione Lombardia -- Territorio e Urbanistica** -- portale istituzionale regionale, circolari esplicative su LR 12/2005, LR 31/2014, LR 18/2019
- **Consiglio di Stato** e **Corte Costituzionale** -- per orientamenti che incidono sulla legittimità di norme regionali lombarde rispetto ai principi statali (vedi `country-italy` Sezione 12)

---

*Cross-reference: caricare sempre `country-italy` come livello Stato di base (TUE, NTC, Codice Beni Culturali, D.M. 1444/1968, accessibilità, prevenzione incendi restano validi e non duplicati qui). Questa skill si limita al livello Regione Lombardia; il livello Comune (Regolamento Edilizio locale, NTA di PGT specifiche, Regolamento di Polizia Idraulica comunale) va sempre verificato caso per caso sul singolo Comune, poiché non è materia coperta da una skill generale.*
