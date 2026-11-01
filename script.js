// Frontend: talks to the C++ backend (dictionary.cpp) through a small JSON API.
// All dictionary logic (Trie insert / search / suggestions) lives in the C++ code.
const API = "/api";

const $ = id => document.getElementById(id);
const clean = s => s.trim().toLowerCase();
const valid = s => /^[a-z]+$/.test(s);
const esc = s => s.replace(/[&<>"]/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));

let active = -1;     // keyboard-selected suggestion
let reqId = 0;       // ignores out-of-date responses while typing fast
let online = false;

function setStatus(ok) {
  online = ok;
  const s = $("status");
  s.className = "status " + (ok ? "on" : "off");
  s.textContent = ok
    ? "C++ backend connected"
    : "C++ backend not running. Start the server (see README), then this page reconnects.";
}

async function call(path, method = "GET") {
  try {
    const res = await fetch(API + path, { method });
    const data = await res.json();
    setStatus(true);
    return data;
  } catch (err) {
    setStatus(false);
    return null;
  }
}

async function renderAll() {
  const words = await call("/words");
  if (!words) return;
  $("count").textContent = words.length;
  $("all").innerHTML = words.map(w =>
    '<button class="chip" data-w="' + w.word + '">' + w.word + "</button>").join("");
}

function renderPath(p) {
  let h = '<div class="node root">root</div>';
  for (const ch of p) h += '<div class="node">' + ch + "</div>";
  $("path").innerHTML = p ? h : "";
}

function showMeaning(word, meaning) {
  const box = $("meaning");
  box.innerHTML = "<b>" + esc(word) + "</b><span>" + (meaning ? esc(meaning) : "No meaning added yet.") + "</span>";
  box.hidden = false;
}

async function renderSuggestions() {
  const p = clean($("q").value);
  const id = ++reqId;
  active = -1;
  $("searchMsg").textContent = "";
  $("meaning").hidden = true;
  if (!p) { $("sugg").innerHTML = ""; renderPath(""); return; }
  if (!valid(p)) {
    $("sugg").innerHTML = "";
    $("searchMsg").innerHTML = '<span class="bad">Use letters a–z only.</span>';
    renderPath("");
    return;
  }
  renderPath(p);
  const data = await call("/suggest?prefix=" + encodeURIComponent(p));
  if (id !== reqId) return;                       // a newer keystroke already replaced this
  if (!data) return;
  if (!data.words.length) {
    $("sugg").innerHTML = "";
    $("searchMsg").innerHTML = '<span class="bad">No suggestions found!</span>';
    return;
  }
  $("sugg").innerHTML = data.words.map(w =>
    '<li class="s" data-w="' + w.word + '"><span><b>' + p + "</b>" + w.word.slice(p.length) + "</span>" +
    '<span class="m">' + esc(w.meaning) + "</span></li>"
  ).join("");
}

async function doSearch() {
  const w = clean($("q").value);
  if (!w || !valid(w)) return;
  const data = await call("/search?word=" + encodeURIComponent(w));
  if (!data) return;
  if (data.found) {
    $("searchMsg").innerHTML = '<span class="ok">Word found in dictionary!</span>';
    showMeaning(data.word, data.meaning);
  } else {
    $("searchMsg").innerHTML = '<span class="bad">Word not found!</span>';
    $("meaning").hidden = true;
  }
}

async function pick(w) {
  $("q").value = w;
  await renderSuggestions();
  await doSearch();
}

$("q").addEventListener("input", renderSuggestions);
$("q").addEventListener("keydown", e => {
  const items = [...document.querySelectorAll("li.s")];
  if (e.key === "ArrowDown" || e.key === "ArrowUp") {
    e.preventDefault();
    if (!items.length) return;
    active = (active + (e.key === "ArrowDown" ? 1 : -1) + items.length) % items.length;
    items.forEach((li, i) => li.classList.toggle("on", i === active));
  } else if (e.key === "Enter") {
    if (active >= 0 && items[active]) { pick(items[active].dataset.w); return; }
    doSearch();
  }
});
$("sugg").addEventListener("click", e => {
  const li = e.target.closest("li.s");
  if (li) pick(li.dataset.w);
});
$("all").addEventListener("click", e => {
  const chip = e.target.closest(".chip");
  if (chip) { pick(chip.dataset.w); window.scrollTo({ top: 0, behavior: "smooth" }); }
});
$("searchBtn").addEventListener("click", doSearch);

async function doAdd() {
  const w = clean($("add").value);
  const m = $("addMeaning").value.trim();
  const msg = $("addMsg");
  if (!w) { msg.innerHTML = '<span class="bad">Enter a word first.</span>'; return; }
  if (!valid(w)) { msg.innerHTML = '<span class="bad">Only letters a–z are allowed.</span>'; return; }
  const data = await call("/insert?word=" + encodeURIComponent(w) + "&meaning=" + encodeURIComponent(m), "POST");
  if (!data) { msg.innerHTML = '<span class="bad">Backend not reachable.</span>'; return; }
  if (data.error) { msg.innerHTML = '<span class="bad">' + esc(data.error) + "</span>"; return; }
  if (data.status === "inserted") msg.innerHTML = '<span class="ok">Word inserted successfully!</span>';
  else if (data.status === "updated") msg.innerHTML = '<span class="ok">Word already existed, meaning updated.</span>';
  else { msg.innerHTML = '<span class="muted">That word is already in the dictionary.</span>'; return; }
  $("add").value = "";
  $("addMeaning").value = "";
  renderAll();
  renderSuggestions();
}
$("addBtn").addEventListener("click", doAdd);
$("add").addEventListener("keydown", e => { if (e.key === "Enter") doAdd(); });
$("addMeaning").addEventListener("keydown", e => { if (e.key === "Enter") doAdd(); });

async function doDelete() {
  const w = clean($("del").value);
  const msg = $("delMsg");
  if (!w) { msg.innerHTML = '<span class="bad">Enter a word to delete.</span>'; return; }
  if (!valid(w)) { msg.innerHTML = '<span class="bad">Only letters a–z are allowed.</span>'; return; }
  if (!confirm('Delete "' + w + '" from the dictionary?')) return;
  const data = await call("/delete?word=" + encodeURIComponent(w), "POST");
  if (!data) { msg.innerHTML = '<span class="bad">Backend not reachable.</span>'; return; }
  if (data.error) { msg.innerHTML = '<span class="bad">' + esc(data.error) + "</span>"; return; }
  if (data.status === "deleted") {
    msg.innerHTML = '<span class="ok">Word deleted successfully!</span>';
    $("del").value = "";
    renderAll();
    renderSuggestions();
  } else {
    msg.innerHTML = '<span class="bad">Word not found!</span>';
  }
}
$("delBtn").addEventListener("click", doDelete);
$("del").addEventListener("keydown", e => { if (e.key === "Enter") doDelete(); });

renderAll();
setInterval(() => { if (!online) renderAll(); }, 3000);   // reconnect automatically once the server starts
