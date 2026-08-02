# Rules & Skills Cloud v1

La sincronizzazione Rules & Skills è una **copia/upsert esplicita**, non una replica distribuita. Il file Markdown completo nel workspace resta la fonte autoritativa per il desktop.

## Push to Strata Cloud

Il desktop risolve il workspace cloud, scarica l’indice remoto e confronta gli elementi per `slug`. Esegue `PATCH` usando l’ID remoto quando lo slug esiste e `POST` per gli elementi nuovi. Lo slug non viene inviato nel `PATCH`. Gli elementi presenti solo nel cloud restano intatti.

## Import from Strata Cloud

Prima di scrivere il desktop mostra una preview:

- remoto assente in locale: **Import**;
- stesso slug e Markdown equivalente: **Skip**;
- stesso slug e Markdown diverso: **Keep local**, con scelta esplicita **Replace local**.

Tutte le scelte vengono applicate solo dopo conferma e solo in un workspace trusted. I record legacy che contengono solo il body vengono trasformati in documenti completi ricostruendo il frontmatter dai metadati API.

## Limiti intenzionali

La v1 non introduce tombstone, cancellazioni cloud→locale, ETag, merge automatico del testo o sincronizzazione continua. Gli elementi solo locali e solo cloud non vengono cancellati implicitamente.
