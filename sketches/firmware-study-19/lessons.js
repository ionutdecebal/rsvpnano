'use strict';
const lessonCopy={
  welcomeBody:['Try the gestures, explore the essentials, and find your physical buttons.','Probiere die Gesten, entdecke die Menüs und finde die Tasten am Gerät.','Încearcă gesturile, explorează meniurile și găsește butoanele dispozitivului.'],
  essentialMenu:['Your Menu','Dein Menü','Meniul tău'], essentialSettings:['Your settings','Deine Einstellungen','Setările tale'],
  pageLesson:['Find your place','Finde deine Lesestelle','Găsește locul în carte'],
  physicalButtons:['Find your buttons','Finde deine Tasten','Găsește butoanele'],
  locateButtons:['Locate the buttons','Tasten am Gerät finden','Localizează butoanele'],
  frontOrientation:['Screen facing you · USB on your right','Display zu dir · USB rechts','Ecranul spre tine · USB în dreapta'],
  leftThree:['Left button','Linke Taste','Butonul stâng'],
  middleThree:['Middle button','Mittlere Taste','Butonul din mijloc'],
  rightThree:['Right button','Rechte Taste','Butonul drept'],
  restartDevice:['Restart device','Gerät neu starten','Repornește dispozitivul'],
  playPause:['Read / pause','Lesen / Pause','Citește / pauză'],
  menuLibrary:['Browse your books and open one. Your reading place is saved for each book.','Durchsuche deine Bücher. Jedes Buch behält seine eigene Lesestelle.','Răsfoiește cărțile și deschide una. Locul de lectură se salvează pentru fiecare carte.'],
  menuChapters:['Jump to a chapter. Preview it first, then choose Read here to move your place.','Springe zu einem Kapitel. Sieh es dir erst an und bestätige dann mit Hier lesen.','Sari la un capitol. Vezi întâi începutul, apoi alege Citește aici.'],
  menuFocus:['Start a reading session or a timer on its own. Presets keep setup quick.','Starte eine Lesesitzung oder einen eigenen Timer. Vorlagen erleichtern den Start.','Pornește o sesiune de lectură sau un cronometru separat. Folosește presetările.'],
  menuSettings:['Reading, Appearance and Display shape your experience. Help is here too.','Lesen, Darstellung und Anzeige passen den Reader an. Hier findest du auch Hilfe.','Lectură, Aspect și Ecran personalizează experiența. Tot aici găsești Ajutor.'],
  menuBook:['Return to your saved word. The book stays paused until you start reading.','Kehre zu deinem gespeicherten Wort zurück. Starte die Wiedergabe selbst.','Revino la cuvântul salvat. Cartea rămâne pe pauză până pornești lectura.'],
  exploreHint:['Tap a tile to learn what it does.','Tippe auf ein Feld für mehr Infos.','Atinge o opțiune pentru detalii.'],
  readingExplain:['Choose one word or context chunks, tap or hold, and extra pauses for difficult text.','Wähle Einzelwörter oder Wortgruppen, Tippen oder Halten und Pausen für schweren Text.','Alege cuvinte sau grupuri, atingere sau apăsare lungă și pauze pentru text dificil.'],
  appearanceExplain:['Change font and size, guides and focal letter, or the information shown while reading and paused.','Ändere Schrift, Größe, Hilfslinien, Fokusbuchstaben und die Infos beim Lesen und Pausieren.','Schimbă fontul, mărimea, ghidajele, litera focală și informațiile afișate în lectură sau pauză.'],
  displayExplain:['Choose your theme, interface language, sleep delay and screensaver. Brightness is in quick controls.','Wähle Farben, Sprache, Ruhezeit und Bildschirmschoner. Helligkeit ist in der Lesesteuerung.','Alege tema, limba, repausul și economizorul. Luminozitatea e în comenzile rapide.'],
  connectionsExplain:['Add books through USB, Wi-Fi or companion sync. Connection setup lives here.','Füge Bücher über USB, WLAN oder die Begleit-App hinzu. Hier richtest du Verbindungen ein.','Adaugă cărți prin USB, Wi-Fi sau sincronizarea cu aplicația. Conexiunile se configurează aici.'],
  deviceExplain:['Check storage and firmware updates, and find device maintenance options.','Prüfe Speicher und Firmware-Updates und finde die Geräteverwaltung.','Verifică stocarea, actualizările firmware și opțiunile de întreținere.'],
  helpExplain:['A quick gesture reminder, a picture of your buttons, and this introduction whenever you need it.','Gesten auf einen Blick, ein Bild deiner Tasten und diese Einführung zum Wiederholen.','Un ghid al gesturilor, o imagine a butoanelor și această introducere, oricând ai nevoie.'],
  browseExplain:['Scroll the page to look around. Tap a word, then Back to book to read from there.','Scrolle durch den Text. Tippe auf ein Wort und dann Zurück zum Buch, um dort zu lesen.','Derulează pagina. Atinge un cuvânt, apoi Înapoi la carte pentru a citi de acolo.'],
  findExplain:['Find place gives you search, chapters and a position slider. Preview before confirming a jump.','Stelle finden bietet Suche, Kapitel und einen Positionsregler. Prüfe den Ausschnitt vor dem Sprung.','Găsește locul oferă căutare, capitole și un cursor de poziție. Verifică înainte de a sări.'],
  searchExplain:['Search for a word or phrase. The keyboard fills the screen, then results take its place.','Suche ein Wort oder einen Satz. Die Tastatur füllt den Bildschirm; danach erscheinen die Treffer.','Caută un cuvânt sau o expresie. Tastatura ocupă ecranul, apoi apar rezultatele.'],
  positionExplain:['Slide through the whole book. Paragraph steps let you refine the position before choosing Read here.','Schiebe durch das ganze Buch. Absatzschritte verfeinern die Stelle vor Hier lesen.','Glisează prin toată cartea. Pașii pe paragrafe ajustează poziția înainte de Citește aici.'],
  selectedWord:['Selected word','Gewähltes Wort','Cuvânt selectat'],
  demoBookReturn:['This is where reading would resume. Your real book has not moved.','Hier würde das Lesen weitergehen. Dein echtes Buch bleibt unverändert.','De aici ar continua lectura. Locul din cartea ta nu s-a schimbat.'],
  readFocusExplain:['The timer runs beside your book and across menus. At break time, reading pauses automatically.','Der Timer läuft beim Lesen und in Menüs weiter. Zur Pause hält das Lesen automatisch an.','Cronometrul merge în lectură și meniuri. Când începe pauza, lectura se oprește automat.'],
  timerOnlyExplain:['Use Focus for anything. Timer only stays on its own screen and keeps running when you leave.','Nutze Fokus für alles. Nur Timer bleibt auf seiner Anzeige und läuft auch beim Verlassen weiter.','Folosește Focus pentru orice. Doar cronometru rămâne pe ecranul său și continuă când îl părăsești.'],
  focusPresetExplain:['Presets: add or edit work, break and rounds.','Vorlagen: Fokus, Pausen und Runden bearbeiten.','Presetări: editează durata, pauzele și rundele.'],
  focusManual:['After a break, continue when you choose.','Nach der Pause setzt du selbst fort.','După pauză, continui când alegi tu.'],
  swipeHelp:['Swipe up for speed, brightness, Menu and Page.','Nach oben wischen für Tempo, Helligkeit, Menü und Seite.','Glisează în sus pentru viteză, luminozitate, meniu și pagină.'],
  practiceControls:['Try speed and brightness','Teste Tempo und Helligkeit','Încearcă viteza și luminozitatea'],
  swipeBody:['Swipe up from the word for speed, brightness, Menu and Page.','Vom Wort nach oben wischen: Tempo, Helligkeit, Menü und Seite.','Glisează în sus de pe cuvânt: viteză, luminozitate, meniu și pagină.'],
};
Object.assign(uiCopy,lessonCopy);

// Physical order supplied by the user: screen facing you, buttons top right,
// reset / power / boot from left to right; USB on the right edge.
let highlightedButton=3;
const buttonPositions=[['leftThree','restartDevice'],['middleThree','menu'],['rightThree','playPause']];
function buttonMapMarkup(){
  const active=highlightedButton;
  return `<div class="button-map"><div class="device-drawing"><svg viewBox="0 0 370 112" role="img" aria-label="${t('frontOrientation')}"><rect class="map-case" x="12" y="26" width="320" height="70" rx="10"/><rect class="map-screen" x="25" y="38" width="293" height="44" rx="3"/><text class="map-word" x="171" y="65" text-anchor="middle">Nano</text><rect class="map-usb" x="333" y="55" width="25" height="17" rx="5"/><text class="map-usb-label" x="345" y="90" text-anchor="middle">USB</text>${[1,2,3].map((n,i)=>`<g class="map-button ${n===active?'selected':''}"><path d="M${255+i*25} 17v6"/><circle cx="${255+i*25}" cy="9" r="8"/><text x="${255+i*25}" y="12" text-anchor="middle">${n}</text><rect x="${248+i*25}" y="23" width="14" height="7" rx="2"/></g>`).join('')}</svg><small>${t('frontOrientation')}</small></div><div class="button-legend">${buttonPositions.map(([position,role],i)=>button(`<span class="button-number">${i+1}</span><span><small>${t(position)}</small><strong>${t(role)}</strong></span>`,'locate-physical',i+1===active?'selected':'',`data-physical="${i+1}" aria-pressed="${i+1===active}"`)).join('')}</div></div>`;
}
function buttonMapView(){return h14(t('physicalButtons'),'help')+buttonMapMarkup();}

function tourBrightnessSlider(){
  const content=`<span>${icon('sun')}</span><output>${tour.brightness}%</output>`;
  return `<div class="full-slider vertical tour-brightness" role="slider" tabindex="0" aria-label="${t('brightness')}" aria-orientation="vertical" aria-valuemin="5" aria-valuemax="100" aria-valuenow="${tour.brightness}" style="--fill:${(tour.brightness-5)/95*100}%"><div class="slider-fill"></div><div class="slider-content">${content}</div><div class="slider-content filled" aria-hidden="true">${content}</div></div>`;
}
const menuLessons=[['library','menuLibrary','book'],['chapters','menuChapters','chapters'],['focus','menuFocus','pace'],['settings','menuSettings','settings'],['backBook','menuBook','book']];
const settingLessons=[['reading','readingExplain','pace'],['appearance','appearanceExplain','type'],['display','displayExplain','sun'],['connections','connectionsExplain','connect'],['device','deviceExplain','device'],['help','helpExplain','book']];
function tourExplorer(kind){
  const entries=kind==='menu'?menuLessons:settingLessons,key=kind==='menu'?'menuTopic':'settingTopic';
  const selected=entries.find(([name])=>name===tour[key])||entries[0];
  return `<div class="tour-explorer"><div class="intro-copy15"><h2>${t(selected[0])}</h2><p>${t(selected[1])}</p><small>${t('exploreHint')}</small></div><div class="tour-topic-grid">${entries.map(([name,,symbol])=>button(`${icon(symbol)}<strong>${t(name)}</strong>`,'intro-topic',name===selected[0]?'selected':'',`data-kind="${kind}" data-topic="${name}" aria-pressed="${name===selected[0]}"`)).join('')}</div></div>`;
}
function tourPageLesson(){
  const tool=tour.pageTool,copy=tool==='search'?'searchExplain':tool==='chapters'?'menuChapters':tool==='position'?'positionExplain':tool==='find'?'findExplain':tool==='return'?'demoBookReturn':'browseExplain';
  const title=tool==='browse'?'page':tool==='return'?'selectedWord':tool==='find'?'find':tool;
  const tools=tool!=='browse'&&tool!=='return';
  const excerpt=documents[0].paragraphs.slice(0,3).map(p=>p.text).join(' ');
  return `<div class="tour-page-lesson"><div class="intro-copy15"><h2>${t(title)}</h2><p>${t(copy)}</p></div><div class="tour-page-card">${tools?`<div class="tour-find-options">${[['search','search'],['chapters','chapters'],['position','position']].map(([name,label])=>button(t(label),'intro-page-tool',tool===name?'selected':'',`data-tool="${name}"`)).join('')}</div>${button('‹ '+t('page'),'intro-page-tool','tour-page-back', 'data-tool="browse"')}`:tool==='return'?`<div class="tour-chosen">${escapeHtml(tour.selectedWord||t('swipeWord'))}</div>${button('‹ '+t('page'),'intro-page-tool','tour-page-back','data-tool="browse"')}`:`<div class="tour-page-bar">${button(t('find'),'intro-page-tool','','data-tool="find"')}${button(t('backBook')+' ↗','intro-page-tool','accent','data-tool="return"')}</div><div class="tour-page-scroll" tabindex="0" aria-label="${t('page')}">${excerpt.split(/\s+/).map((word,i)=>`<span data-tour-word="${i}" class="${tour.selectedIndex===i?'selected':''}">${escapeHtml(word)}</span>`).join(' ')}</div>`}</div></div>`;
}
function tourFocusLesson(){
  const reading=tour.focusKind==='reading';
  return `<div class="tour-focus-lesson"><div class="intro-copy15"><h2>${t(reading?'readFocus':'timerOnly')}</h2><p>${t(reading?'readFocusExplain':'timerOnlyExplain')}</p><small>${t('focusManual')}</small></div><div class="tour-focus-options">${button(t('readFocus'),'intro-focus-kind',reading?'selected':'','data-kind="reading"')}${button(t('timerOnly'),'intro-focus-kind',reading?'':'selected','data-kind="timer"')}<small>${t('focusPresetExplain')}</small></div></div>`;
}
function detailedLesson(step){
  if(step===3||step===4)return tuningLesson(step);
  if(step===5)return tourExplorer('menu');
  if(step===6)return tourPageLesson();
  if(step===7)return tourExplorer('settings');
  if(step===8)return tourFocusLesson();
  if(step===9)return buttonMapMarkup();
  return '';
}

let tourSlider=null;
function cancelTourSlider(){
  const drag=tourSlider;tourSlider=null;
  if(drag&&screen.hasPointerCapture(drag.id))screen.releasePointerCapture(drag.id);
}
function setTourBrightness(el,value){
  if(!tour)return;
  tour.brightness=Math.max(5,Math.min(100,Math.round(value)));
  el.setAttribute('aria-valuenow',tour.brightness);el.style.setProperty('--fill',(tour.brightness-5)/95*100+'%');
  el.querySelectorAll('output').forEach(output=>output.textContent=tour.brightness+'%');
}
function slideTourBrightness(event){const rect=tourSlider.el.getBoundingClientRect();setTourBrightness(tourSlider.el,5+95*(1-(event.clientY-rect.top)/rect.height));}
screen.addEventListener('pointerdown',event=>{
  const el=event.target.closest('.tour-brightness');if(!el||!event.isPrimary||event.button!==0)return;
  tourSlider={el,id:event.pointerId,original:tour.brightness};screen.setPointerCapture(event.pointerId);slideTourBrightness(event);event.preventDefault();
});
screen.addEventListener('pointermove',event=>{if(tourSlider?.id===event.pointerId)slideTourBrightness(event);});
screen.addEventListener('pointerup',event=>{if(tourSlider?.id===event.pointerId){slideTourBrightness(event);cancelTourSlider();}});
screen.addEventListener('pointercancel',()=>{if(tourSlider){setTourBrightness(tourSlider.el,tourSlider.original);cancelTourSlider();}});
screen.addEventListener('keydown',event=>{
  const el=event.target.closest('.tour-brightness');if(!el)return;
  const values={ArrowUp:tour.brightness+1,ArrowRight:tour.brightness+1,ArrowDown:tour.brightness-1,ArrowLeft:tour.brightness-1,Home:5,End:100};
  if(event.key in values){event.preventDefault();event.stopPropagation();setTourBrightness(el,values[event.key]);}
});
screen.addEventListener('click',event=>{
  const word=event.target.closest('[data-tour-word]');if(!word||!tour)return;
  screen.querySelector('.tour-page-scroll .selected')?.classList.remove('selected');
  word.classList.add('selected');tour.selectedIndex=Number(word.dataset.tourWord);tour.selectedWord=word.textContent;
});
