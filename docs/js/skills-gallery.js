(function () {
  const ui = {
    it: {
      "meta.title": "Strata — Gallery di Skill & Rules",
      "meta.description": "Skill e rules GIS curate: apri, verifica la provenienza e copia il Markdown.",
      "hero.kicker": "FIELD NOTES / 001",
      "hero.title": "Conoscenza GIS<br>che mostra le fonti.",
      "hero.desc": "Skill e regole curate come documenti, non come scatole nere. Leggi il metodo, controlla la provenienza, copia il Markdown.",
      "hero.cloud": "Copia direttamente in Strata Cloud →",
      "filter.search": "Cerca nel catalogo",
      "filter.placeholder": "Landsat, NBS, raster…",
      "filter.type": "Tipo",
      "filter.category": "Verticale",
      "filter.static": "Catalogo statico · zero backend",
      "type.all": "Tutti",
      "category.all": "Tutte",
      "status.loading": "Caricamento catalogo…",
      "detail.empty": "Seleziona un documento per leggere metodo, fonte e Markdown completo.",
      "footer.home": "Home",
      "footer.cloud": "Gallery Cloud",
      "action.copy": "Copia Markdown",
      "action.copied": "Copiato",
      "action.download": "Scarica .md",
      "source": "Fonte",
      "license": "Licenza",
      "empty": "Nessun documento corrisponde ai filtri.",
      "error": "Il catalogo statico non è disponibile. Riprova dopo aver ricaricato la pagina.",
      "documents": "documenti",
      "category.urban-green": "Verde urbano",
    },
    en: {
      "meta.title": "Strata — Skill & Rules Gallery",
      "meta.description": "Curated GIS skills and rules: inspect provenance and copy the Markdown.",
      "hero.kicker": "FIELD NOTES / 001",
      "hero.title": "GIS knowledge<br>that shows its sources.",
      "hero.desc": "Curated skills and rules as documents, not black boxes. Read the method, inspect provenance, copy the Markdown.",
      "hero.cloud": "Copy directly to Strata Cloud →",
      "filter.search": "Search the catalog",
      "filter.placeholder": "Landsat, NBS, raster…",
      "filter.type": "Type",
      "filter.category": "Vertical",
      "filter.static": "Static catalog · zero backend",
      "type.all": "All",
      "category.all": "All",
      "status.loading": "Loading catalog…",
      "detail.empty": "Select a document to inspect its method, source and complete Markdown.",
      "footer.home": "Home",
      "footer.cloud": "Cloud gallery",
      "action.copy": "Copy Markdown",
      "action.copied": "Copied",
      "action.download": "Download .md",
      "source": "Source",
      "license": "License",
      "empty": "No document matches the current filters.",
      "error": "The static catalog is unavailable. Reload the page and try again.",
      "documents": "documents",
      "category.urban-green": "Urban green",
    },
  };

  const state = { items: [], selected: null, content: new Map(), lang: "it" };
  const el = {};
  const text = (key) => ui[state.lang]?.[key] || ui.it[key] || key;

  function node(tag, className, content) {
    const element = document.createElement(tag);
    if (className) element.className = className;
    if (content !== undefined) element.textContent = content;
    return element;
  }

  function categoryLabel(category) {
    return ui[state.lang]?.[`category.${category}`] || ui.it[`category.${category}`] || category;
  }

  function applyLanguage() {
    state.lang = document.documentElement.lang === "en" ? "en" : "it";
    document.querySelectorAll("[data-gallery-i18n]").forEach((element) => {
      const value = text(element.getAttribute("data-gallery-i18n"));
      if (element.getAttribute("data-gallery-i18n") === "hero.title") element.innerHTML = value;
      else element.textContent = value;
    });
    document.title = text("meta.title");
    document.querySelector('meta[name="description"]')?.setAttribute("content", text("meta.description"));
    if (el.search) el.search.placeholder = text("filter.placeholder");
    renderCategoryOptions();
    renderList();
    if (state.selected) void renderDetail(state.selected);
  }

  function filteredItems() {
    const query = el.search.value.trim().toLowerCase();
    return state.items.filter((item) => {
      if (el.type.value !== "all" && item.type !== el.type.value) return false;
      if (el.category.value !== "all" && item.category !== el.category.value) return false;
      const haystack = [item.slug, item.name, item.description, item.category, ...item.tags].join(" ").toLowerCase();
      return !query || haystack.includes(query);
    });
  }

  function renderCategoryOptions() {
    if (!el.category) return;
    const current = el.category.value || "all";
    el.category.replaceChildren();
    const all = node("option", "", text("category.all")); all.value = "all"; el.category.append(all);
    [...new Set(state.items.map((item) => item.category))].sort().forEach((category) => {
      const option = node("option", "", categoryLabel(category)); option.value = category; el.category.append(option);
    });
    el.category.value = [...el.category.options].some((option) => option.value === current) ? current : "all";
  }

  function renderList() {
    if (!el.list) return;
    const items = filteredItems();
    el.list.replaceChildren();
    el.status.textContent = `${items.length} / ${state.items.length} ${text("documents")}`;
    if (!items.length) {
      el.list.append(node("p", "gallery-empty-results", text("empty")));
      return;
    }
    items.forEach((item) => {
      const card = node("button", `gallery-card${state.selected?.slug === item.slug && state.selected?.type === item.type ? " is-active" : ""}`);
      card.type = "button";
      const meta = node("span", "gallery-card-meta");
      meta.append(node("span", "", item.type), node("span", "", categoryLabel(item.category)));
      card.append(meta, node("h2", "", item.name), node("p", "", item.description));
      const tags = node("span", "gallery-tags");
      item.tags.slice(0, 4).forEach((tag) => tags.append(node("span", "gallery-tag", tag)));
      card.append(tags);
      card.addEventListener("click", () => selectItem(item, true));
      el.list.append(card);
    });
  }

  async function markdownFor(item) {
    const key = `${item.type}:${item.slug}`;
    if (state.content.has(key)) return state.content.get(key);
    const response = await fetch(`./gallery/${item.path}`);
    if (!response.ok) throw new Error(`Markdown request failed: ${response.status}`);
    const markdown = await response.text();
    state.content.set(key, markdown);
    return markdown;
  }

  async function copyMarkdown(markdown, button) {
    try {
      await navigator.clipboard.writeText(markdown);
    } catch (_error) {
      const area = document.createElement("textarea");
      area.value = markdown; document.body.append(area); area.select(); document.execCommand("copy"); area.remove();
    }
    button.textContent = text("action.copied");
    window.setTimeout(() => { button.textContent = text("action.copy"); }, 1500);
  }

  function downloadMarkdown(item, markdown) {
    const link = document.createElement("a");
    link.href = URL.createObjectURL(new Blob([markdown], { type: "text/markdown;charset=utf-8" }));
    link.download = item.type === "skill" ? `${item.slug}-SKILL.md` : `${item.slug}.md`;
    link.click();
    URL.revokeObjectURL(link.href);
  }

  async function renderDetail(item) {
    el.detail.replaceChildren(node("div", "gallery-detail-empty", "…"));
    try {
      const markdown = await markdownFor(item);
      const header = node("header", "gallery-detail-header");
      header.append(node("p", "gallery-detail-eyebrow", `${item.type} / ${item.trustLevel}`), node("h2", "", item.name), node("p", "gallery-detail-description", item.description));
      const provenance = node("div", "gallery-provenance");
      const source = node("div");
      source.append(document.createTextNode(`${text("source")}: `));
      const sourceLink = node("a", "", item.source.title); sourceLink.href = item.source.url; sourceLink.target = "_blank"; sourceLink.rel = "noopener noreferrer"; source.append(sourceLink);
      provenance.append(source, node("span", "", `${text("license")}: ${item.license}`));
      const wrap = node("div", "gallery-code-wrap");
      const actions = node("div", "gallery-code-actions");
      const copy = node("button", "", text("action.copy")); copy.type = "button"; copy.addEventListener("click", () => copyMarkdown(markdown, copy));
      const download = node("button", "", text("action.download")); download.type = "button"; download.addEventListener("click", () => downloadMarkdown(item, markdown));
      actions.append(copy, download);
      const code = node("pre", "gallery-code", markdown);
      wrap.append(actions, code);
      el.detail.replaceChildren(header, provenance, wrap);
    } catch (_error) {
      const error = node("div", "gallery-detail-empty"); error.append(node("span", "", "!"), node("p", "", text("error"))); el.detail.replaceChildren(error);
    }
  }

  function selectItem(item, updateHash) {
    state.selected = item;
    if (updateHash) history.replaceState(null, "", `#${item.type}/${item.slug}`);
    renderList();
    void renderDetail(item);
  }

  function selectFromHash() {
    const match = location.hash.match(/^#(skill|rule)\/([a-z0-9_-]+)$/);
    const item = match && state.items.find((candidate) => candidate.type === match[1] && candidate.slug === match[2]);
    if (item) selectItem(item, false);
    else if (!state.selected && state.items[0]) selectItem(state.items[0], false);
  }

  async function init() {
    Object.assign(el, {
      count: document.querySelector("[data-gallery-count]"),
      search: document.querySelector("[data-gallery-search]"),
      type: document.querySelector("[data-gallery-type]"),
      category: document.querySelector("[data-gallery-category]"),
      status: document.querySelector("[data-gallery-status]"),
      list: document.querySelector("[data-gallery-list]"),
      detail: document.querySelector("[data-gallery-detail]"),
    });
    [el.search, el.type, el.category].forEach((control) => control.addEventListener("input", renderList));
    try {
      const response = await fetch("./gallery/manifest.json");
      if (!response.ok) throw new Error(`Manifest request failed: ${response.status}`);
      const manifest = await response.json();
      state.items = manifest.items.filter((item) => item.enabled);
      el.count.textContent = String(state.items.length).padStart(2, "0");
      applyLanguage();
      selectFromHash();
    } catch (_error) {
      el.status.textContent = text("error");
      el.list.append(node("p", "gallery-empty-results", text("error")));
    }
  }

  document.addEventListener("DOMContentLoaded", init);
  document.addEventListener("strata:langchange", applyLanguage);
  window.addEventListener("hashchange", selectFromHash);
})();
