'use strict';

const $ = (selector) => document.querySelector(selector);
const screen = $('#screen');
const projectLogo = $('#project-logo-template').innerHTML;
$('#masthead-logo').innerHTML = projectLogo;
const demoThemes = [{"id":"ink","name":"Ink","scheme":"dark","bg":"#151413","fg":"#f7f4f1","dim":"#b1a79f","edge":"#423b35","surface":"#27221e","accent":"#fd6f42","action":"#fd6f42","on-action":"#21130e","focus":"#fd6f42","danger":"#ff9b89","progress":"#fd6f42","break":"#ffb48a","page-accent":"#a63d1b","page":"#f3f1ec","page-ink":"#302a26","page-muted":"#776b62","page-line":"#d8d1c8","page-surface":"#e9e4dc"},{"id":"paper","name":"Paper","scheme":"light","bg":"#f6f0e6","fg":"#30271f","dim":"#726050","edge":"#c4b5a3","surface":"#eae0d2","accent":"#ae3e1b","action":"#ae3e1b","on-action":"#fff9f3","focus":"#ae3e1b","danger":"#a52c24","progress":"#ae3e1b","break":"#87522a","page-accent":"#ae3e1b","page":"#f6f0e6","page-ink":"#30271f","page-muted":"#726050","page-line":"#d8cdbc","page-surface":"#ede4d7"},{"id":"nord","name":"Nord","scheme":"dark","source":"themes/nord.toml","bg":"#2E3440","fg":"#ECEFF4","dim":"#D8DEE9","edge":"#81A1C1","surface":"#3B4252","accent":"#e49aa4","action":"#BF616A","focus":"#e49aa4","progress":"#88C0D0","break":"#88C0D0","on-action":"#171313","danger":"#BF616A","page-accent":"#8d414c","page":"#eceff4","page-ink":"#2e3440","page-muted":"#637085","page-line":"#c9d0da","page-surface":"#e0e5ec"},{"id":"dracula","name":"Dracula","scheme":"dark","source":"themes/dracula.toml","bg":"#282A36","fg":"#F8F8F2","dim":"#BFBFBF","edge":"#6272A4","surface":"#21222C","accent":"#FF79C6","action":"#FF79C6","focus":"#FF79C6","progress":"#BD93F9","break":"#8BE9FD","on-action":"#171313","danger":"#FF79C6","page-accent":"#964175","page":"#f1edf5","page-ink":"#352c3f","page-muted":"#776680","page-line":"#d8cddd","page-surface":"#e8e0ee"},{"id":"gruvbox-dark","name":"Gruvbox Dark","scheme":"dark","source":"themes/gruvbox-dark.toml","bg":"#282828","fg":"#EBDBB2","dim":"#A89984","edge":"#665C54","surface":"#1D2021","accent":"#FB4934","action":"#FB4934","focus":"#FB4934","progress":"#FABD2F","break":"#8EC07C","on-action":"#171313","danger":"#FB4934","page-accent":"#a73b22","page":"#f4ebd7","page-ink":"#3c3836","page-muted":"#796754","page-line":"#d7c8ae","page-surface":"#e9dec6"}];
function currentTheme(){return demoThemes.find(theme=>theme.id===state.theme)||demoThemes[0];}
function applyTheme(){
  const theme=currentTheme(),root=document.documentElement;
  root.dataset.theme=theme.id;
  for(const [key,value] of Object.entries(theme))if(key!=='id'&&key!=='name'&&key!=='scheme'&&key!=='source')root.style.setProperty('--'+key,value);
  root.style.setProperty('--logo-ink',theme.fg);
  root.style.setProperty('--logo-accent',theme.accent);
  screen.style.colorScheme=theme.scheme;
  screen.dataset.theme=theme.id;
  screen.classList.toggle('paper',theme.scheme==='light');
  document.querySelector('meta[name="theme-color"]').content=theme.page;
}
const icons = {
  book:'M3 4h6c2 0 3 1 3 2v15c0-2-2-3-4-3H3V4Zm18 0h-6c-2 0-3 1-3 2v15c0-2 2-3 4-3h5V4Z',
  chapters:'M7 5h14M7 12h14M7 19h14M3 5h.01M3 12h.01M3 19h.01',
  settings:'M4 7h16M4 17h16M9 4v6M16 14v6',
  sun:'M12 2v2M12 20v2M2 12h2M20 12h2M5 5l1.5 1.5M17.5 17.5l1.5 1.5M5 19l1.5-1.5M17.5 6.5l1.5-1.5M16 12a4 4 0 1 1-8 0 4 4 0 0 1 8 0',
  timer:'M9 2h6M12 5v2M18 6l2-2M12 10v5l3 2M21 14a9 9 0 1 1-18 0 9 9 0 0 1 18 0',
  type:'M3 20 9 4l6 16M5 15h8M16 12h6M19 12v8',
  connect:'M8 16l8-8M10 6l2-2a5 5 0 0 1 7 7l-2 2M14 18l-2 2a5 5 0 0 1-7-7l2-2',
  device:'M5 4h14v16H5ZM9 17h6',
  pace:'M3 17a9 9 0 1 1 18 0M12 13l5-5M6 17h12',
  battery:'M2 7h17v10H2ZM22 10v4M5 10h10v4H5Z',
  previous:'M15 5l-7 7 7 7',
  next:'M9 5l7 7-7 7',
};
function icon(name) { return `<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="${icons[name] || icons.book}"/></svg>`; }
function button(label, action, cls='', attrs='') { return `<button class="${cls}" data-action="${action}" ${attrs}>${label}</button>`; }
function go(label, destination, cls='') { return `<button class="${cls}" data-go="${destination}">${label}</button>`; }
function escapeHtml(text) { return String(text).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c])); }
const books = [
  {title:'The Secret Garden',author:'Frances Hodgson Burnett',spine:'SECRET GARDEN',color:'#526650',progress:42,chapter:3,chapters:['There Is No One Left','Mistress Mary Quite Contrary','Across the Moor','Martha','The Cry in the Corridor','There Was Someone Crying','The Key of the Garden','The Robin Who Showed the Way']},
  {title:'The Time Machine',author:'H. G. Wells',spine:'TIME MACHINE',color:'#50687e',progress:18,chapter:1,chapters:['Introduction','The Machine','The Time Traveller Returns','Time Travelling','In the Golden Age','The Sunset of Mankind','A Sudden Shock','Explanation']},
  {title:'Walden',author:'Henry David Thoreau',spine:'WALDEN',color:'#a07b4b',progress:6,chapter:0,chapters:['Economy','Where I Lived','Reading','Sounds','Solitude','Visitors','The Bean-Field','The Village']},
  {title:'A Room of One’s Own',author:'Virginia Woolf',spine:'A ROOM OF ONE’S OWN',color:'#79576c',progress:0,chapter:0,chapters:['Chapter One','Chapter Two','Chapter Three','Chapter Four','Chapter Five','Chapter Six']},
];
books.push({title:'The Quiet Garden',author:'Original demo story',spine:'THE QUIET GARDEN',color:'#a65e40',progress:0,chapter:0,chapters:demoChapters.map(ch=>ch.title)});
// The listed classics also use original placeholder text in this browser sketch.
const documents=books.map(b=>{
  const doc={words:[],paragraphs:[],chapters:[]};
  b.chapters.forEach((title,chapter)=>{
    doc.chapters.push({title,start:doc.words.length});
    demoChapters[chapter%demoChapters.length].paragraphs.forEach(text=>{
      const start=doc.words.length;doc.words.push(...text.split(/\s+/));
      doc.paragraphs.push({text,start,end:doc.words.length,chapter});
    });
  });
  return doc;
});
// Original demonstration copy, not an extract from the displayed books.
const passage = 'Beyond the garden wall, the morning moved slowly. Light settled on the leaves, and somewhere a small bird began to sing. She put her book beside the open window and listened. There was no need to hurry. For a moment, the world was simply this: a quiet room, a warm cup, and the possibility of another page.';
let words = documents[0].words;
const initialState = () => ({view:'menu',book:0,selectedBook:0,selectedChapter:3,playing:false,word:documents[0].chapters[3].start,pagePosition:null,browse:null,returnPlace:null,places:{},wpm:300,mode:'rsvp',readerSurface:'playback',keyboard:null,brightness:70,theme:'ink',appearanceTab:'type',appearancePage:0,fontSize:44,font:'Literata',tracking:0,anchor:44,guideWidth:1,guideGap:8,highlight:true,phantom:false,visibility:{reading:{chapter:false,battery:true,batteryLabel:true,progress:true,arrows:false},paused:{chapter:true,battery:true,batteryLabel:true,progress:true,arrows:true}},focusMinutes:25,presetIndex:0,presets:[{name:'Pomodoro',focus:25,rest:5,rounds:4},{name:'Deep reading',focus:45,rest:10,rounds:2},{name:'Short session',focus:15,rest:3,rounds:2}],focus:null,presetDraft:null,addOrigin:'library',uploadState:'waiting',connection:'USB',detailPage:0,pacing:true,longDelay:100,complexDelay:100,punctuationDelay:300,pauseMode:'Sentence end',handed:'Right',chapterScroll:'Normal',language:'English',standby:5,saver:'Life',bookFont:'Reader default',bookLanguage:'Auto',batteryFormat:'Percent',footerFormat:'Percent',startupCheck:true,info:null,loading:null});
let state = initialState();
let readingTimeout;
let uploadTimeout;
let loadingTimeout;
const descriptions = {
  menu:['01','Menu','Pick up where you left off.','The current book gets the most space. Chapters, Library, and Settings stay in a fixed column; brightness lives in the swipe-up controls.','Open the book, or start a Focus session before reading.'],
  reader:['02','Read','Let the words do the work.','A fixed focus letter keeps each word anchored. Reading and Paused can show different controls, using the new firmware’s visibility settings.','Press Start reading or Space. Tap the word to pause. M opens Menu.'],
  library:['03','Library','Give the title room to speak.','One book at a time, with a large title, author, and reading progress. Wide arrows make browsing easy on the narrow display.','Use the big arrows or your keyboard’s left/right keys, then Open book. Browsing does not interrupt Focus.'],
  chapters:['04','Chapters','Find your place, deliberately.','A scrollable chapter selector follows the current book. Opening a chapter returns to paused reading.','Scroll the list, or use the arrows. Then select Open chapter.'],
  appearance:['05','Appearance','See the change as you make it.','The latest firmware’s live appearance editor informs this screen. Typography, Reading, and Paused get separate previews.','Adjust the sliders. Use Guides → for the second control page. Tap the word to change its highlight.'],
  focus:['06','Focus','Make a little time for a book.','Proposed: one timer runs independently of the current screen. Start with book opens paused reading; playback remains your choice.','Start with book, start reading, then open the Focus badge.'],
  'focus-controls':['06','Focus','Same timer. Different view.','Proposed: moving between a book and a full-screen timer preserves the same session. Pausing the book does not pause Focus.','Switch to full screen, then back to the book. Watch the timer continue.'],
  'focus-full':['06','Focus','A little space to concentrate.','The full-screen view uses the same Focus session. It can accompany reading, writing, or anything else that needs your attention.','Pause and resume the timer, or return to your book.'],
  settings:['07','Settings','Settings you can find by purpose.','A proposed grouping of existing controls: Reading pace, Text & display, Connections, and Device & help. Detailed firmware options remain in scope.','Open Text & display to reach the live appearance editor.'],
  pace:['07','Reading pace','Find a comfortable rhythm.','Speed and reading mode live together. The sketch plays its original sample text at the selected words per minute.','Try 200 WPM, then return to the book and press Start.'],
  display:['07','Text & display','Make the reader feel like yours.','Appearance stays in one live editor, while theme and brightness remain device settings. Font rendering here is a browser approximation.','Try Paper, then compare your changes in the reader.'],
  connections:['07','Connections','A clear route from book to reader.','Companion transfers and USB disk mode remain different tasks. This sketch simulates the handoff without connecting to hardware.','Try Add a book from Library, then simulate a completed upload.'],
  add:['03','Add a book','Bring something worth reading.','A proposed guided handoff uses the existing browser or Wi-Fi companion. All transfer state in this sketch is simulated.','Choose a connection, then use Simulate upload to complete the flow.'],
  upload:['03','Add a book','Keep the next step visible.','Connection and transfer states are examples. No files, settings, or firmware are sent to a real Nano.','Simulate the upload and open the sample book.'],
  details:['03','Book details','The title has room to breathe.','Full book information gets its own space. Per-book font and language remain distinct from the global reader appearance.','Return to the shelf or open this book.'],
  device:['07','Device & help','Maintenance has a home.','Storage, updates, and help sit together here. These are navigation sketches; no hardware operations are performed.','Explore a detail view, then use Back to book.'],
};
Object.assign(descriptions,{
  loading:['08','Loading','Your logo leads the way.','The supplied SVG keeps its shape and takes its foreground and accent from the selected theme. A gentle pulse travels between the themed focus marks and N while the status line explains what is loading.','Replay startup below, or open a book from Library. These staged timings are for review; the device should never wait just to finish an animation.'],
  appearance:['05','Appearance','A page switch you can actually hit.','The editor keeps its live preview. Two fixed, 44-pixel-high controls sit underneath it, with the same targets on Typography, Reading, and Paused.','Try Font & position / Guides, then Visibility / Formats & battery. Tap the sample word to toggle highlighting.'],
  focus:['06','Focus','Your presets, ready to read.','Keep preset names, focus and break durations, and round counts. The sample presets are editable. Start with book begins the timer and opens paused reading.','Try a preset, or use the 10-second demo beneath the device.'],
  'focus-controls':['06','Focus','One timer, wherever you go.','Focus continues through reading and menus. Tapping the reader badge opens this view directly. Pause the timer separately from the book.','Go back to the book. Finish focus using the preview controls to see the automatic break.'],
  'focus-full':['06','Focus','Work, break, then your choice.','Focus ends by pausing reading and opening a full-screen break. After the break, the next round waits for Resume reading. This preview includes a break after the final round too.','Use Finish focus, then Finish break. The book stays paused until you resume.'],
  'preset-editor':['06','Edit preset','Keep the routine that suits you.','Names, focus minutes, break minutes and round counts can be changed. Saved presets remain for this browser session.','Adjust a preset, save it, and start it from the preset picker.'],
  settings:['07','Settings','Six doors. No hallway.','Reading, Appearance, Display & sleep, Connections, Book fonts, and Device & help are all directly available here.','Open Reading: behavior and all three pacing adjustments fit on one page.'],
  pace:['07','Reading','All your reading settings, together.','Mode, pause behavior, handedness, chapter scrolling and word delays share one screen. WPM stays on the reader. Delay sliders affect the demo; hardware gestures are illustrative.','Change a delay or mode, then return to the book. No additional reading-settings submenu.'],
  display:['07','Display & sleep','Four controls, one page.','Theme, interface language, standby and screensaver are directly editable. Brightness remains on the main menu. The browser previews theme only; language and sleep are illustrative.','Cycle Ink, Paper, Nord, Dracula and Gruvbox here, or switch instantly with the Theme selector above.'],
  'book-fonts':['07','Book fonts','An override for the open book.','Per-book font and language are directly reachable from Settings. These selectors are illustrative; the browser does not reproduce the device font catalog or shaping.','Compare this with Appearance, which sets the default reader look.'],
  device:['07','Device & help','Actions, without another hub.','Storage check, updates, help, RSS refresh, encryption and power off are visible together. Confirmation or result screens still belong to individual actions.','Try a device action. All results here are simulated.'],
});

function book(){return books[state.book];}
function progress(){return Math.round(state.word/Math.max(1,words.length-1)*100);}
function remaining(){if(!state.focus)return 0;return state.focus.running ? Math.max(0,Math.ceil((state.focus.until-Date.now())/1000)) : state.focus.remaining;}
function timeLabel(seconds){return `${String(Math.floor(seconds/60)).padStart(2,'0')}:${String(seconds%60).padStart(2,'0')}`;}
function focusLabel(){
  if(!state.focus)return 'Start Focus';
  const f=state.focus;
  if(f.phase==='ready')return 'Focus · Ready';
  if(f.phase==='complete')return 'Focus · Done';
  return `${f.phase==='break'?'Break':f.running?'Focus':'Paused'} ${timeLabel(remaining())}`;
}

function header(title,parent='menu',parentLabel='Menu',extra='') {return `<div class="screen-header">${go('‹ '+parentLabel,parent,'back')}<span class="heading">${title}</span>${extra}${go('Back to book ↗','reader','back-book')}</div>`;}
function wordMarkup(word, preview=false){
  const focus=Math.min(word.length-1,word.length<=1?0:word.length<=5?1:word.length<=9?2:3);
  const left=escapeHtml(word.slice(0,focus)),letter=escapeHtml(word[focus]),right=escapeHtml(word.slice(focus+1));
  const font = state.font==='Literata' ? 'Georgia, serif' : 'Arial, sans-serif';
  return `<div class="word-layer" style="font-family:${font}">${state.phantom?'<span class="word-phantom before">a</span><span class="word-phantom after">world</span>':''}<div class="guide top" style="left:${state.anchor}%;width:${state.guideWidth}px;height:${state.guideGap}px"></div><div class="word-anchor" style="left:${state.anchor}%;font-size:${state.fontSize}px;letter-spacing:${state.tracking}px"><span class="word-before">${left}</span><span class="${state.highlight?'focus-letter':''}">${letter}</span><span class="word-after">${right}</span></div><div class="guide bottom" style="left:${state.anchor}%;width:${state.guideWidth}px;height:${state.guideGap}px"></div></div>`;
}
function placePhantomWords(){
  const layer=screen.querySelector('.word-layer');
  if(!layer||!state.phantom)return;
  const bounds=layer.getBoundingClientRect(),scale=bounds.width/layer.clientWidth;
  for(const side of ['before','after']){
    const ghost=layer.querySelector('.word-phantom.'+side);
    const word=layer.querySelector('.word-'+side).getBoundingClientRect();
    const width=ghost.getBoundingClientRect().width;
    const left=side==='before'?word.left-18*scale-width:word.right+18*scale;
    const fits=left>=bounds.left+8*scale&&left+width<=bounds.right-8*scale;
    ghost.style.visibility=fits?'visible':'hidden';
    ghost.style.left=(left-bounds.left)/scale+'px';
    ghost.style.right='auto';
  }
}
function menuView(){return `<div class="menu-layout"><div class="current-book"><div class="micro">YOUR PLACE, SAVED</div><h2>${book().title}</h2><div class="author">${book().author}</div><div class="book-progress"><div class="progress-line"><span style="width:${progress()}%"></span></div><span>${progress()}% · Chapter ${book().chapter+1}</span></div><div class="book-actions">${go('Back to book ↗','reader','primary')}${button(focusLabel(),'focus-route','accent','data-focus-label')}</div></div><nav class="menu-nav" aria-label="Device menu">${go(icon('chapters')+'Chapters<span class="chevron">›</span>','chapters')}${go(icon('book')+'Library<span class="chevron">›</span>','library')}${go(icon('settings')+'Settings<span class="chevron">›</span>','settings')}</nav><div class="brightness">${button('+','brighter','','aria-label="Increase brightness"')}<div class="brightness-value">${icon('sun')}<span>${state.brightness}%</span></div>${button('−','dimmer','','aria-label="Decrease brightness"')}</div></div>`;}
function readerView(){
  if(state.readerSurface==='page')return pageView();
  const visible=state.visibility[state.playing?'reading':'paused'];
  const playbackLabel=state.playing?'Pause reading':'Start reading';
  const chapterTitle=`Ch. ${book().chapter+1} · ${book().chapters[book().chapter]}`;
  const text=button(playbackMarkup(),'play','word-zone',`aria-label="${playbackLabel}"`);
  return `<div class="reader"><div class="reader-top">${button(chapterTitle,'chapters',visible.chapter?'':'hide',`title="${escapeHtml(chapterTitle)}"`)}${state.mode==='chunk'?'<span class="reading-kind">Chunks</span>':''}${button('Page ↗','page-mode','page-mode-button','aria-label="Switch to page reading at this word"')}${state.focus?button(focusLabel(),'focus-route','focus-badge','data-focus-label'):''}<span class="battery"><span class="${visible.battery?'':'hide'}">${icon('battery')}</span> <span class="${visible.batteryLabel?'':'hide'}">${batteryText()}</span></span></div>${text}<div class="reader-footer">${state.playing?button('Menu','menu','muted'):button('‹ Menu','menu','muted')}<div>${button('−','slower',visible.arrows?'':'hide','aria-label="Read slower"')}<span>${state.wpm} <small>WPM</small></span>${button('+','faster',visible.arrows?'':'hide','aria-label="Read faster"')}</div>${button(state.playing?'Ⅱ Pause':'▷ Start reading','play','play-button')}<span class="${visible.progress?'':'hide'}">${footerText()}</span></div><div class="reader-track" style="width:${progress()}%"></div></div>`;
}
function libraryView(){
  const selected=books[state.selectedBook];
  const selectedProgress=state.selectedBook===state.book?progress():selected.progress;
  const count=`<span class="library-count" aria-label="Book ${state.selectedBook+1} of ${books.length}">${String(state.selectedBook+1).padStart(2,'0')} <span>/ ${String(books.length).padStart(2,'0')}</span></span>`;
  return header('Library','menu','Menu',count)+`<div class="screen-body library-body">
    ${button(icon('previous'),'previous-book','library-arrow','aria-label="Previous book" title="Previous book (←)"')}
    <div class="library-detail" aria-live="polite" aria-atomic="true">
      <h2 title="${escapeHtml(selected.title)}">${escapeHtml(selected.title)}</h2>
      <p class="library-author">${escapeHtml(selected.author)}</p>
      <div class="library-progress"><div class="progress-line"><span style="width:${selectedProgress}%"></span></div><span>${selectedProgress?selectedProgress+'% read':'Not started'}</span></div>
    </div>
    ${button(icon('next'),'next-book','library-arrow','aria-label="Next book" title="Next book (→)"')}
    <div class="action-stack compact library-actions">${button('Open book ↗','open-book','primary')}${go('Book details','details','secondary')}${button('+ Add a book','add-book','accent')}</div>
  </div>`;
}
function chaptersView(){return (state.browse?`<div class="screen-header">${go('‹ Find place','find-place','back')}<span class="heading">Chapters</span>${button('Cancel','cancel-browse','back-book')}</div>`:header('Chapters'))+`<div class="screen-body chapters-body"><div class="chapter-list" aria-label="Chapter selector">${book().chapters.map((name,i)=>button(`<span>${String(i+1).padStart(2,'0')}</span>${name}`,'select-chapter',i===state.selectedChapter?'selected':'',`data-index="${i}" aria-pressed="${i===state.selectedChapter}"`)).join('')}</div><div class="chapter-arrows">${button('⌃','previous-chapter','','aria-label="Previous chapter"')}${button('⌄','next-chapter','','aria-label="Next chapter"')}</div><div class="chapter-open"><small>CHAPTER ${state.selectedChapter+1} OF ${book().chapters.length}</small><div style="font-size:14px">${book().chapters[state.selectedChapter]}</div>${button('Preview chapter ↗','open-chapter','primary')}</div></div>`;}
function settingTile(title,subtitle,destination,symbol){return go(`${icon(symbol)}<span><strong>${title}</strong><small>${subtitle}</small></span><span class="chevron">›</span>`,destination);}
function settingsView(){return header('Settings')+`<div class="screen-body setting-grid direct-settings">${settingTile('Reading','Behavior & word pacing','pace','pace')}${settingTile('Appearance','Font, guides & visibility','appearance','type')}${settingTile('Display & sleep','Theme, language & rest','display','sun')}${settingTile('Connections','Wi-Fi, companion & USB','connections','connect')}${settingTile('Book fonts','For the current book','book-fonts','book')}${settingTile('Device & help','Storage, updates & help','device','device')}</div>`;}

function appearanceView(){
  const typography=state.appearanceTab==='type',page=state.appearancePage;
  const visible=state.visibility[typography?'paused':state.appearanceTab];
  const dial=(label,key,min,max,step=1)=>`<label class="large-dial">${label}<output>${state[key]}${key==='anchor'?'%':''}</output><input type="range" min="${min}" max="${max}" step="${step}" value="${state[key]}" data-setting="${key}" aria-label="${label}"></label>`;
  const toggle=(label,key)=>button(`<span>${label}</span><strong>${visible[key]?'Show':'Hide'}</strong>`,'visibility','control-tile',`data-key="${key}" aria-pressed="${visible[key]}"`);
  let controls;
  if(typography)controls=page===0
    ? `<div class="font-pair">${button(state.font+' ↻','font','secondary')}${button('Size: '+({28:'Small',36:'Medium',44:'Large',52:'XL'}[state.fontSize]||state.fontSize),'font-size','secondary')}</div>${dial('Tracking','tracking',0,4)}${dial('Anchor','anchor',32,62)}`
    : `${dial('Guide width','guideWidth',0,3)}${dial('Guide gap','guideGap',0,15)}${button('Phantom words <strong>'+(state.phantom?'On':'Off')+'</strong>','phantom','wide-toggle',`aria-pressed="${state.phantom}"`)}`;
  else controls=page===0
    ? `<div class="visibility-tiles">${toggle('Chapter','chapter')}${toggle('Progress','progress')}${toggle('Battery icon','battery')}${toggle('WPM controls','arrows')}</div>`
    : `<div class="format-controls">${toggle('Battery label','batteryLabel')}${button('<span>Battery format</span><strong>'+state.batteryFormat+'</strong>','battery-format','control-tile')}${button('<span>Progress format</span><strong>'+state.footerFormat+'</strong>','footer-format','control-tile')}</div>`;
  const pages=typography?['Font & position','Guides']:['Visibility','Formats & battery'];
  return `<div class="screen-header appearance-header">${go('‹ Settings','settings','back')}<span class="heading">Appearance</span>${['type','reading','paused'].map(tab=>button(tab==='type'?'Typography':tab[0].toUpperCase()+tab.slice(1),'appearance-tab','mode-button '+(state.appearanceTab===tab?'active':''),`data-tab="${tab}" aria-pressed="${state.appearanceTab===tab}"`)).join('')}${go('Try ↗','reader','accent')}</div>
  <div class="appearance-layout improved-appearance"><div class="appearance-preview ${typography?'':'chrome-preview'}">
  ${!typography?`<div class="small-chrome"><span style="opacity:${visible.chapter?1:.15}">Chapter 4 · Martha</span><span class="mini-battery"><span style="opacity:${visible.battery?1:.15}">▰</span> <span style="opacity:${visible.batteryLabel?1:.15}">${batteryText()}</span></span></div>`:''}
  ${button(wordMarkup('reading',true),'highlight','word-zone','aria-label="Toggle focus-letter highlighting"')}
  ${!typography?`<div class="visibility-specimen"><span style="opacity:${visible.arrows?1:.15}">− &nbsp; ${state.wpm} WPM &nbsp; +</span><span style="opacity:${visible.progress?1:.15}">${footerText()}</span></div>`:''}
  <div class="page-segments" role="group" aria-label="${typography?'Typography':'Visibility'} control pages">${pages.map((label,index)=>button(label,'appearance-page',page===index?'selected':'',`data-page="${index}" aria-pressed="${page===index}"`)).join('')}</div></div>
  <div class="appearance-controls">${controls}</div></div>`;
}

function focusStartView(){const preset=state.presets[state.presetIndex];return `<div class="screen-header">${go('‹ Menu','menu','back')}<span class="heading">Start Focus</span>${go('Presets ↗','focus-presets','back-book')}</div><div class="screen-body focus-start"><div><div class="micro">${escapeHtml(preset.name)} · ${state.presetIndex+1} / ${state.presets.length}</div><div class="stepper">${button('‹','previous-preset','','aria-label="Previous preset"')}<output>${preset.focus}</output><span class="unit">minutes</span>${button('›','next-preset','','aria-label="Next preset"')}</div><p>${preset.rest} min break · ${preset.rounds} rounds</p></div><div class="action-stack focus-start-actions">${button('Read with Focus ↗','start-focus-book','primary')}${button('Timer only','start-focus-full','secondary')}</div></div>`;}
function focusPresetsView(){return `<div class="screen-header">${go('‹ Focus','focus','back')}<span class="heading">Your presets · ${state.presets.length}</span>${button('+ Add preset','add-preset','back-book',state.presets.length>=6?'disabled':'')}</div><div class="preset-list scroll-list" aria-label="Focus presets">${state.presets.map((p,i)=>`<div class="preset-row">${button(`<strong>${escapeHtml(p.name)}</strong><small>${p.focus} min focus · ${p.rest} min break · ${p.rounds} rounds</small>`,'choose-preset',i===state.presetIndex?'selected':'',`data-index="${i}" aria-label="Use ${escapeHtml(p.name)}"`)}${button('Edit','edit-preset','secondary',`data-index="${i}" aria-label="Edit ${escapeHtml(p.name)}"`)}</div>`).join('')}</div>`;}

function focusControlsView(){return focusFullView();}

function focusFullView(){
  const f=state.focus,rest=f.phase==='break',ready=f.phase==='ready',complete=f.phase==='complete';
  const status=rest?'TIME FOR A BREAK':ready?'BREAK COMPLETE':complete?'SESSION COMPLETE':f.running?'A LITTLE TIME, WELL SPENT':'TIMER PAUSED';
  const actions=rest?`<p>${f.withBook?'Reading is paused.':'Time to step away.'}<br>Rest for a moment.</p>${go('End session','focus-end','secondary')}`
    : ready?`${button(f.withBook?'Resume reading ↗':'Start next round','resume-round','primary')}${go('End session','focus-end','accent')}`
    : complete?`${button(f.withBook?'Back to book ↗':'Finish session','finish-session','primary')}${go('End session','focus-end','accent')}`
    : `${button(f.running?'Pause timer':'Resume timer','toggle-focus','secondary')}${go('End session','focus-end','accent')}`;
  return `<div class="timer-layout ${rest?'break-timer':''}">${rest||ready||!f.withBook?go('‹ Menu','menu'):go('‹ Back to book','reader')}<div class="timer-main"><div class="micro" data-focus-status>${status}</div><div class="timer-value" data-timer>${timeLabel(remaining())}</div><div class="progress-line"><span data-timer-progress style="width:${100-remaining()/f.total*100}%"></span></div><div class="timer-caption">${ready?`Next: ${timeLabel(f.focusSeconds)} focus. Start when ready.`:complete?'A little time, well spent.':rest?'Start again when you’re ready.':f.withBook?'Keeps running while you read.':'Your time. For anything.'}</div></div><div class="action-stack"><div class="session-meta">${escapeHtml(f.name)}<small>Round ${f.round} / ${f.rounds}</small></div>${actions}</div></div>`;
}

function paceView(){
  const control=(label,value,name)=>button(`<span>${label}</span><strong>${value}</strong>`,name,'control-tile');
  const delay=(label,key)=>`<label class="pace-dial">${label}<output>${state[key]} ms</output><input type="range" min="0" max="1000" step="50" value="${state[key]}" data-setting="${key}" aria-label="${label} delay"></label>`;
  return header('Reading','settings','Settings',button('Reset pacing','reset-pacing','accent header-reset'))+`<div class="screen-body reading-settings"><div class="behavior-grid">${control('Reading mode',state.mode==='rsvp'?'Word by word':'Context chunks','reading-mode')}${control('Pause',state.pauseMode,'pause-mode')}${control('Reader hand',state.handed,'handed')}${control('Chapter scroll',state.chapterScroll,'chapter-scroll')}</div><div class="pacing-fields">${delay('Long words','longDelay')}${delay('Complex words','complexDelay')}${delay('Punctuation','punctuationDelay')}</div></div>`;
}

function displayView(){
  const control=(label,value,name)=>button(`<span>${label}</span><strong>${value}</strong>`,name,'control-tile');
  return header('Display & sleep','settings','Settings')+`<div class="screen-body display-settings">${control('Theme',currentTheme().name,'theme')}${control('Interface language',state.language,'language-cycle')}${control('Standby after',state.standby?state.standby+' minutes':'Off','standby')}${control('Screensaver',state.saver,'screensaver')}</div>`;
}

function connectionsView(){return header('Connections','settings','Settings')+`<div class="screen-body setting-grid connection-settings">${button(icon('connect')+'<span><strong>Wi-Fi network</strong><small>Choose or change network</small></span>','wifi')}${button(icon('book')+'<span><strong>Companion</strong><small>Add & manage books</small></span>','companion')}${button(icon('device')+'<span><strong>USB disk transfer</strong><small>Use Nano as a drive</small></span>','usb-disk')}${button(icon('connect')+'<span><strong>Forget Wi-Fi</strong><small>Remove saved network</small></span>','forget-wifi')}</div>`;}

function addView(){return header('Add a book',state.addOrigin,state.addOrigin==='library'?'Library':'Connections')+`<div class="screen-body connection-body"><div class="generic-content"><h2>From your shelf to your Nano.</h2><p>Open the web companion on your computer.<br>Choose how you want to connect.</p><div class="connection-state"><i></i>Connection preview · no device required</div></div><div class="action-stack">${button('USB cable →','connect-usb','primary')}${button('Wi-Fi →','connect-wifi','secondary')}</div></div>`;}
function uploadView(){const done=state.uploadState==='done',busy=state.uploadState==='uploading';return header('Add a book','add','Connection')+`<div class="screen-body connection-body"><div class="generic-content"><div class="micro">${state.connection} COMPANION · DEMO</div><h2>${done?'Your book is ready.':busy?'Adding your book…':'Ready for your next book.'}</h2><p>${done?'The Time Machine has been added.<br>Open it whenever you’re ready.':state.connection==='USB'?'In Chrome or Edge, open the companion.<br>Connect with a data cable and choose Upload.':'Join the same Wi-Fi network as your reader.<br>Open the companion and choose Upload.'}</p></div><div class="action-stack">${done?button('Open book ↗','open-upload','primary'):button(busy?'Simulating upload…':'Simulate upload','simulate-upload','primary',busy?'disabled':'')}${go('Back to Library','library','accent')}</div></div>`;}
function detailsView(){const b=books[state.selectedBook];return header('Book details','library','Library')+`<div class="screen-body two-column"><div class="generic-content"><h2>${b.title}</h2><p>${b.author}</p><p>${b.progress}% read · ${b.chapters.length} sample chapters</p></div><div class="action-stack compact">${button('Open book ↗','open-book','primary')}${go('Book fonts →','book-fonts','secondary')}${go('Back to Library','library','accent')}</div></div>`;}
function deviceView(){return header('Device & help','settings','Settings')+`<div class="screen-body setting-grid direct-settings">${button(`${icon('device')}<span><strong>Check storage</strong><small>4 books · SD card</small></span>`,'storage')}${button(`${icon('connect')}<span><strong>Updates</strong><small>Firmware & update policy</small></span>`,'updates')}${button(`${icon('book')}<span><strong>How to read</strong><small>Buttons & touch</small></span>`,'help')}${button(`${icon('book')}<span><strong>Refresh RSS</strong><small>Check your feeds</small></span>`,'rss')}${button(`${icon('settings')}<span><strong>Encryption</strong><small>Settings protection</small></span>`,'encryption')}${button(`${icon('sun')}<span><strong>Power off</strong><small>Save place & shut down</small></span>`,'power-off')}</div>`;}

function infoView(){const info=state.info;return header(info.title,info.parent,info.parentLabel)+`<div class="screen-body two-column"><div class="generic-content"><h2>${info.headline}</h2><p>${info.copy}</p></div><div class="action-stack">${go('Back to '+info.parentLabel,info.parent,'secondary')}${go('Back to book ↗','reader','accent')}</div></div>`;}

function batteryText(){return state.batteryFormat==='Voltage'?'3.90V':state.batteryFormat==='Time left'?'8.4h':'84%';}
function footerText(){return state.footerFormat==='Chapter time'?'Ch. 12m':state.footerFormat==='Book time'?'Book 2h':progress()+'%';}
function updatesView(){return header('Updates','device','Device')+`<div class="screen-body update-settings"><div class="update-source"><label>Repository owner<input value="${escapeHtml(state.updateOwner||'ionutdecebal')}" data-update="updateOwner" aria-label="Repository owner"></label><label>Release tag<input value="${escapeHtml(state.updateTag||'latest')}" data-update="updateTag" aria-label="Release tag"></label></div><div class="update-policy">${button('<span>Check on startup</span><strong>'+(state.startupCheck?'On':'Off')+'</strong>','startup-check','control-tile',`aria-pressed="${state.startupCheck}"`)}<p>Installed: Preview Edition<br>22 September 2026</p></div><div class="action-stack">${button('Check for updates','check-updates','primary')}${go('Back to Device','device','accent')}</div></div>`;}

function bookFontsView(){return header('Book fonts','settings','Settings')+`<div class="screen-body book-font-settings"><div class="book-font-title"><span class="micro">CURRENT BOOK</span><h2>${escapeHtml(book().title)}</h2></div><div class="book-font-controls">${button('<span>Typeface</span><strong>'+state.bookFont+'</strong>','cycle-book-font','control-tile')}${button('<span>Book language</span><strong>'+state.bookLanguage+'</strong>','cycle-book-language','control-tile')}</div></div>`;}
function presetEditorView(){
  const d=state.presetDraft;
  const field=(label,key,min,max)=>`<div class="preset-field"><span>${label}</span><div class="preset-stepper">${button('−','preset-value','',`data-key="${key}" data-delta="-1" aria-label="Decrease ${label}" ${d[key]<=min?'disabled':''}`)}<output>${d[key]}</output>${button('+','preset-value','',`data-key="${key}" data-delta="1" aria-label="Increase ${label}" ${d[key]>=max?'disabled':''}`)}</div></div>`;
  return header(d.index===null?'New preset':'Edit preset','focus-presets','Presets')+`<div class="screen-body preset-editor"><label class="preset-name">Name<input id="preset-name" maxlength="14" value="${escapeHtml(d.name)}" aria-label="Preset name"></label><div class="preset-values">${field('Focus · min','focus',1,180)}${field('Break · min','rest',1,60)}${field('Rounds','rounds',1,12)}</div><div class="preset-save">${button('Save preset','save-preset','primary',d.name.trim()?'':'disabled')}${go('Cancel','focus-presets','accent')}</div></div>`;
}
function focusTick(){
  const f=state.focus;if(!f)return;
  if(f.running&&remaining()===0){
    pauseReading();
    if(f.phase==='focus'){
      f.phase='break';f.total=f.restSeconds;f.remaining=f.total;f.until=Date.now()+f.total*1000;f.running=true;
    }else{
      f.phase=f.round>=f.rounds?'complete':'ready';f.running=false;
      f.total=f.focusSeconds;f.remaining=0;
    }
    navigate('focus-full',{keepLoading:true});
  }
  document.querySelectorAll('[data-timer]').forEach(el=>el.textContent=timeLabel(remaining()));
  document.querySelectorAll('[data-focus-label]').forEach(el=>el.textContent=focusLabel());
  const progressEl=$('[data-timer-progress]');if(progressEl)progressEl.style.width=(100-remaining()/f.total*100)+'%';
  updateDemoControls();
}
function updateDemoControls(){
  const f=state.focus;
  $('#demo-expire').disabled=!f||f.phase!=='focus';$('#demo-break').disabled=!f||f.phase!=='break';
  $('#demo-status').textContent=!f?'Use a 10-second demo, or jump ahead in a preset timer.':
    f.phase==='ready'?`Break finished. Press ${f.withBook?'Resume reading':'Start next round'} when ready.`:
    f.phase==='complete'?'Session complete. Return to your book when you’re ready.':
    `${f.demo?'Short demo':'Preset'} · ${f.phase==='break'?'Break':'Focus'} ${timeLabel(remaining())} · Round ${f.round} / ${f.rounds}${f.running?'':' · Paused'}`;
}

function loadingView(){
  const job=state.loading;
  return `<div class="loading-layout" data-loading-kind="${job.kind}">
    <div class="loading-brand"><span class="project-logo animated-logo" role="img" aria-label="RSVP Nano">${projectLogo}</span></div>
    <div class="loading-copy"><div class="micro loading-kicker">${job.kind==='startup'?'STARTING UP':'YOUR NEXT CHAPTER'}</div>
      <h2 data-loading-title>${job.kind==='startup'?'A little more reading.':'Opening your book'}</h2>
      <div class="loading-detail" data-loading-detail title="${escapeHtml(job.kind==='book'?books[job.bookIndex].title:'')}">${job.kind==='book'?escapeHtml(books[job.bookIndex].title):'Your library. Your place.'}</div>
      <div class="loading-status" role="status" aria-live="polite"><span class="loading-dot" aria-hidden="true"></span><span data-loading-stage>${job.stages[job.step]}</span></div>
      <div class="loading-rail" aria-hidden="true"><span></span></div>
    </div>
  </div>`;
}
function cancelLoading(){clearTimeout(loadingTimeout);state.loading=null;}
function beginLoading(kind,bookIndex=state.book,commitBook=false){
  cancelLoading();
  const job={kind,bookIndex,commitBook,step:0,startedAt:Date.now(),stages:kind==='startup'?['Checking storage','Loading preferences','Restoring your place']:['Preparing the text','Restoring your place']};
  state.loading=job;navigate('loading');
  const advance=()=>{
    if(state.loading!==job)return;
    job.step=Math.floor((Date.now()-job.startedAt)/(kind==='startup'?1300:1100));
    if(job.step<job.stages.length){
      const status=screen.querySelector('[data-loading-stage]');if(status)status.textContent=job.stages[job.step];
      loadingTimeout=setTimeout(advance,kind==='startup'?1300:1100);
      return;
    }
    state.loading=null;
    if(commitBook)openDocument(bookIndex);
    if(state.view==='loading')navigate(kind==='startup'?'menu':'reader');
    else render(); // A Focus break can take priority without being dismissed by load completion.
  };
  loadingTimeout=setTimeout(advance,kind==='startup'?1300:1100);
}

function render(){
  applyTheme();
  screen.style.filter=`brightness(${.72 + state.brightness*.0028})`;
  const views={'search-keyboard':searchKeyboardView,'find-place':findPlaceView,'search-book':searchBookView,'focus-presets':focusPresetsView,loading:loadingView,menu:menuView,reader:readerView,library:libraryView,chapters:chaptersView,settings:settingsView,appearance:appearanceView,focus:focusStartView,'focus-controls':focusControlsView,'focus-full':focusFullView,'preset-editor':presetEditorView,'book-fonts':bookFontsView,updates:updatesView,pace:paceView,display:displayView,connections:connectionsView,add:addView,upload:uploadView,details:detailsView,device:deviceView,info:infoView,'focus-end':()=>header('End Focus?','focus-controls','Focus')+`<div class="screen-body two-column"><div class="generic-content"><h2>Call it a session?</h2><p>The timer will end.<br>Your reading place stays saved.</p></div><div class="action-stack">${go('Keep focusing','focus-controls','primary')}${button('End session','end-focus','danger')}</div></div>`};
  Object.assign(views,window.studyViews||{});
  screen.innerHTML=(views[state.view]||menuView)();
  placePhantomWords();
  mountPage();
  if(state.view==='find-place')updateBrowse();
  if(state.view==='search-book')updateSearch();
  if(state.view==='search-keyboard')mountKeyboard();
  updateChunkDemo();
  screen.setAttribute('aria-busy',state.view==='loading'?'true':'false');
  screen.setAttribute('aria-label','Nano display: '+(descriptions[state.view]?.[1]||state.info?.title||'Focus'));
  const info=(state.view==='reader'&&state.readerSurface==='page'?descriptions.page:descriptions[state.view])||descriptions[state.view.startsWith('focus')?'focus-controls':'settings'];
  $('#screen-caption').textContent=`${info[0]} / ${info[1]}`;
  $('#view-number').textContent=`${info[0]} / ${info[1].toUpperCase()}`;
  $('#view-title').textContent=info[2];$('#view-description').textContent=info[3];$('#view-hint').textContent=info[4];
  const family=state.view==='add-books'?'help':state.view==='loading'?'loading':state.view.startsWith('focus')?'focus':['pace','display','connections','device','info','book-fonts','updates'].includes(state.view)?'settings':['details','add','upload'].includes(state.view)?'library':state.view;
  document.querySelectorAll('.screen-tabs button').forEach(b=>{b.classList.toggle('active',b.dataset.go===family);if(b.dataset.go===family)b.setAttribute('aria-current','page');else b.removeAttribute('aria-current');});
  const startupTab=$('.screen-tabs [data-action="preview-startup"]');
  startupTab.classList.toggle('active',state.view==='loading');
  if(state.view==='loading')startupTab.setAttribute('aria-current','page');else startupTab.removeAttribute('aria-current');
  $('#theme').value=state.theme;
  updateDemoControls();
  $('#boot small').textContent=state.readerSurface==='page'?'Next page':'Play / pause';
  $('#boot').title=state.readerSurface==='page'?'Next page (Space)':'Start or pause reading (Space)';
  if(state.view==='chapters'){
    const list=screen.querySelector('.chapter-list'),selected=list.querySelector('.selected');
    list.scrollTop=Math.max(0,selected.offsetTop-list.offsetTop-(list.clientHeight-selected.clientHeight)/2);
  }
}
function navigate(view,{hash=true,keepLoading=false,capture=true}={}){
  if(view==='startup'){beginLoading('startup');return;}
  if(view==='loading'&&!state.loading)view='menu';
  if(view!=='loading'&&!keepLoading)cancelLoading();
  const allowed=['add-books','button-map','help','onboarding','language','gesture-tray','search-keyboard','find-place','search-book','focus-presets','menu','reader','library','chapters','settings','appearance','focus','focus-controls','focus-full','focus-end','pace','display','connections','add','upload','details','device','info','preset-editor','book-fonts','updates','loading'];
  if(!allowed.includes(view))view='menu';
  if(view==='focus'&&state.focus)view='focus-full';
  if(view==='focus-controls')view='focus-full';
  if(view==='preset-editor'&&!state.presetDraft)view='focus';
  if(view==='reader'&&state.focus&&['break','ready'].includes(state.focus.phase))view='focus-full';
  if(view.startsWith('focus-')&&view!=='focus-presets'&&!state.focus)view='focus';
  if(view==='info'&&!state.info)view='settings';
  if(capture)capturePage();
  if(state.browse&&!['find-place','search-book','search-keyboard','chapters'].includes(view))state.browse=null;
  if(['find-place','search-book','search-keyboard'].includes(view)&&!state.browse)view='reader';
  if(view==='search-keyboard'&&!state.keyboard)view='find-place';
  if(view!=='search-keyboard')state.keyboard=null;
  pauseReading();
  if(view==='library'&&state.view!=='library'&&state.view!=='details')state.selectedBook=state.book;
  if(view==='chapters'&&state.view!=='chapters')state.selectedChapter=chapterAt(state.browse?state.browse.candidate:state.word);
  if(state.view==='upload'&&view!=='upload'){clearTimeout(uploadTimeout);if(state.uploadState==='uploading')state.uploadState='waiting';}
  state.view=view;
  if(hash)history.replaceState(null,'','#'+view);
  render();
}
function pauseReading(){state.playing=false;clearTimeout(readingTimeout);}
function queueWord(){
  clearTimeout(readingTimeout);
  if(!state.playing||state.readerSurface==='page')return;
  const frame=playbackFrame();
  const delay=ChunkReader.duration(frame.words,state.wpm,state);
  readingTimeout=setTimeout(()=>{
    if(!state.playing||state.view!=='reader'||state.readerSurface==='page')return;
    if(frame.end>=words.length){pauseReading();render();return;}
    setWord(frame.end);
    screen.querySelector('.word-zone').innerHTML=playbackMarkup();
    placePhantomWords();
    screen.querySelector('.reader-footer>span:last-child').textContent=footerText();
    screen.querySelector('.reader-track').style.width=progress()+'%';
    const chapterButton=screen.querySelector('[data-action="chapters"]');
    chapterButton.textContent=`Ch. ${book().chapter+1} · ${book().chapters[book().chapter]}`;
    updateChunkDemo();queueWord();
  },delay);
}

function toggleReading(){
  if(state.focus?.phase==='break'){navigate('focus-full');return;}
  if(state.focus?.phase==='ready'){action('resume-round');return;}
  if(state.view!=='reader')navigate('reader');
  if(state.readerSurface==='page'){stepPage(1);return;}
  state.playing=!state.playing;render();queueWord();
}

function startFocus(destination,demo=false){
  const preset=state.presets[state.presetIndex],focusSeconds=demo?10:preset.focus*60;
  state.focus={name:demo?'Quick demo':preset.name,total:focusSeconds,remaining:focusSeconds,until:Date.now()+focusSeconds*1000,running:true,phase:'focus',round:1,rounds:demo?2:preset.rounds,focusSeconds,restSeconds:demo?8:preset.rest*60,demo,withBook:destination==='reader'};
  navigate(destination);
}

function info(title,headline,copy,parent='device',parentLabel='Device & help'){state.info={title,headline,copy,parent,parentLabel};navigate('info');}
function action(name,element){
  switch(name){
    case 'choose-preset':state.presetIndex=Number(element.dataset.index);navigate('focus');return;
    case 'page-mode':capturePage();pauseReading();state.readerSurface='page';navigate('reader');return;
    case 'word-mode':capturePage();state.readerSurface='playback';navigate('reader');return;
    case 'try-page':openDocument(books.length-1);state.readerSurface='page';navigate('reader',{capture:false});return;
    case 'find-place':beginBrowse();return;
    case 'cancel-browse':cancelBrowse();return;
    case 'read-here':commitBrowse();return;
    case 'return-place':{const saved=state.returnPlace;state.returnPlace=null;if(saved&&saved.book===state.book){setWord(saved.word);state.pagePosition=saved.pagePosition;}navigate('reader',{capture:false});return;}
    case 'browse-paragraph':{const doc=documents[state.book],index=doc.paragraphs.findIndex(p=>p.end>state.browse.candidate);const target=doc.paragraphs[Math.max(0,Math.min(doc.paragraphs.length-1,index+Number(element.dataset.delta)))];state.browse.candidate=target.start;state.browse.atChapter=false;updateBrowse();return;}
    case 'search-result':state.browse.candidate=Number(element.dataset.word);state.browse.atChapter=false;navigate('find-place');return;
    case 'open-search':openSearchKeyboard();return;
    case 'keyboard-key':keyboardKey(element.dataset.key);return;
    case 'keyboard-shift':state.keyboard.shifted=!state.keyboard.shifted;render();return;
    case 'keyboard-mode':state.keyboard.mode=(state.keyboard.mode+1)%3;render();return;
    case 'clear-search':state.keyboard.value='';state.keyboard.cursor=0;render();return;
    case 'cancel-keyboard':state.keyboard=null;navigate(state.browse.query?'search-book':'find-place');return;
    case 'submit-search':if(!state.keyboard?.value.trim())return;state.browse.query=state.keyboard.value.trim();state.keyboard=null;navigate('search-book');return;
    case 'try-chunks':openDocument(books.length-1);state.readerSurface='playback';state.mode='chunk';navigate('reader',{capture:false});return;
    case 'demo-single':pauseReading();state.readerSurface='playback';state.mode='rsvp';navigate('reader');return;
    case 'demo-chunks':pauseReading();state.readerSurface='playback';state.mode='chunk';navigate('reader');return;
    case 'previous-chunk':stepChunk(-1);return;
    case 'next-chunk':stepChunk(1);return;
    case 'preview-startup':beginLoading('startup');return;
    case 'preview-book':beginLoading('book');return;
    case 'previous-preset':state.presetIndex=(state.presetIndex+state.presets.length-1)%state.presets.length;break;
    case 'next-preset':state.presetIndex=(state.presetIndex+1)%state.presets.length;break;
    case 'edit-preset':{const index=Number(element?.dataset.index??state.presetIndex);state.presetDraft={...state.presets[index],index};} navigate('preset-editor');return;
    case 'add-preset':state.presetDraft={name:'New preset',focus:25,rest:5,rounds:4,index:null};navigate('preset-editor');return;
    case 'preset-value':{const d=state.presetDraft,key=element.dataset.key,max={focus:180,rest:60,rounds:12}[key];d[key]=Math.max(1,Math.min(max,d[key]+Number(element.dataset.delta)));break;}
    case 'save-preset':{const {index,...preset}=state.presetDraft;preset.name=preset.name.trim();if(!preset.name)return;if(index===null){state.presets.push(preset);state.presetIndex=state.presets.length-1;}else state.presets[index]=preset;state.presetDraft=null;navigate('focus-presets');return;}
    case 'resume-round':{const f=state.focus;if(!f||f.phase!=='ready')return;f.round++;f.phase='focus';f.remaining=f.focusSeconds;f.total=f.focusSeconds;f.until=Date.now()+f.total*1000;f.running=true;navigate(f.withBook?'reader':'focus-full');state.playing=f.withBook&&state.readerSurface==='playback';render();queueWord();return;}
    case 'finish-session':{const withBook=state.focus.withBook;state.focus=null;navigate(withBook?'reader':'menu');return;}
    case 'pause-mode':state.pauseMode=state.pauseMode==='Sentence end'?'Instant':'Sentence end';break;
    case 'handed':state.handed=state.handed==='Right'?'Left':'Right';break;
    case 'chapter-scroll':state.chapterScroll=state.chapterScroll==='Normal'?'Reversed':'Normal';break;
    case 'reset-pacing':state.longDelay=100;state.complexDelay=100;state.punctuationDelay=300;break;
    case 'language-cycle':state.language=state.language==='English'?'Romanian':'English';break;
    case 'standby':{const values=[0,1,5,10,30];state.standby=values[(values.indexOf(state.standby)+1)%values.length];break;}
    case 'screensaver':{const values=['Life','Maze','Voronoi','Reaction','Screen off'];state.saver=values[(values.indexOf(state.saver)+1)%values.length];break;}
    case 'font-size':{const sizes=[28,36,44,52];state.fontSize=sizes[(sizes.indexOf(state.fontSize)+1)%sizes.length];break;}
    case 'battery-format':{const values=['Percent','Time left','Voltage'];state.batteryFormat=values[(values.indexOf(state.batteryFormat)+1)%values.length];break;}
    case 'footer-format':{const values=['Percent','Chapter time','Book time'];state.footerFormat=values[(values.indexOf(state.footerFormat)+1)%values.length];break;}
    case 'cycle-book-font':state.bookFont=state.bookFont==='Reader default'?'Literata':state.bookFont==='Literata'?'Sans serif':'Reader default';break;
    case 'cycle-book-language':state.bookLanguage=state.bookLanguage==='Auto'?'English':'Auto';break;
    case 'forget-wifi':info('Forget Wi-Fi','Remove the saved network?','This is a preview of the confirmation step.<br>No device connection is changed.','connections','Connections');return;
    case 'rss':info('Refresh RSS','Feeds checked.','Preview result: no new articles.<br>No feed request was made.');return;
    case 'encryption':info('Settings encryption','Protect saved settings.','Enabling encryption is permanent on the device.<br>The existing confirmation flow will stay in place.<br>This preview does not enable encryption.');return;
    case 'power-off':info('Power off','Your place is saved.','On the device this shuts down the Nano.<br>This preview stays open for review.');return;

    case 'menu':navigate('menu');return;
    case 'chapters':navigate('chapters');return;
    case 'play':toggleReading();return;
    case 'brighter':state.brightness=Math.min(100,state.brightness+10);break;
    case 'dimmer':state.brightness=Math.max(20,state.brightness-10);break;
    case 'cycle-brightness':state.brightness=state.brightness>=100?20:state.brightness+10;break;
    case 'slower':state.wpm=Math.max(100,state.wpm-25);queueWord();break;
    case 'faster':state.wpm=Math.min(900,state.wpm+25);queueWord();break;
    case 'previous-book':state.selectedBook=(state.selectedBook+books.length-1)%books.length;break;
    case 'next-book':state.selectedBook=(state.selectedBook+1)%books.length;break;
    case 'open-book':beginLoading('book',state.selectedBook,true);return;
    case 'select-chapter':state.selectedChapter=Number(element.dataset.index);break;
    case 'previous-chapter':state.selectedChapter=Math.max(0,state.selectedChapter-1);break;
    case 'next-chapter':state.selectedChapter=Math.min(book().chapters.length-1,state.selectedChapter+1);break;
    case 'open-chapter':if(!state.browse)beginBrowse(false);state.browse.candidate=documents[state.book].chapters[state.selectedChapter].start;state.browse.atChapter=true;navigate('find-place');return;
    case 'focus-route':navigate(state.focus?'focus-controls':'focus');return;
    case 'focus-shorter':state.focusMinutes=Math.max(5,state.focusMinutes-5);break;
    case 'focus-longer':state.focusMinutes=Math.min(90,state.focusMinutes+5);break;
    case 'start-focus-book':startFocus('reader');return;
    case 'start-focus-full':startFocus('focus-full');return;
    case 'toggle-focus':if(!state.focus||state.focus.phase!=='focus')return;if(state.focus.running){state.focus.remaining=remaining();state.focus.running=false;}else{state.focus.until=Date.now()+state.focus.remaining*1000;state.focus.running=true;}break;
    case 'end-focus':state.focus=null;navigate('menu');return;
    case 'appearance-tab':state.appearanceTab=element.dataset.tab;state.appearancePage=0;break;
    case 'appearance-page':state.appearancePage=Number(element.dataset.page);break;
    case 'highlight':state.highlight=!state.highlight;break;
    case 'phantom':state.phantom=!state.phantom;break;
    case 'font':state.font=state.font==='Literata'?'Sans serif':'Literata';break;
    case 'visibility':state.visibility[state.appearanceTab][element.dataset.key]=!state.visibility[state.appearanceTab][element.dataset.key];break;
    case 'reading-mode':state.mode=state.mode==='rsvp'?'chunk':'rsvp';pauseReading();break;
    case 'next-page':stepPage(1);return;
    case 'previous-page':stepPage(-1);return;
    case 'pacing':state.pacing=!state.pacing;break;
    case 'theme':state.theme=demoThemes[(demoThemes.findIndex(theme=>theme.id===state.theme)+1)%demoThemes.length].id;break;
    case 'add-book':state.addOrigin='library';navigate('add');return;
    case 'companion':state.addOrigin='connections';navigate('add');return;
    case 'connect-usb':case 'connect-wifi':state.connection=name==='connect-usb'?'USB':'Wi-Fi';state.uploadState='waiting';navigate('upload');return;
    case 'simulate-upload':state.uploadState='uploading';clearTimeout(uploadTimeout);uploadTimeout=setTimeout(()=>{state.uploadState='done';render();},1300);break;
    case 'open-upload':beginLoading('book',1,true);return;
    case 'wifi':info('Wi-Fi network','One network. Your books.','Network selection and passwords belong here.<br>This sketch does not scan or join a network.','connections','Connections');return;
    case 'usb-disk':info('USB disk transfer','Eject before returning.','Disk mode makes the card available to a computer.<br>The final flow must preserve the firmware’s safe eject.<br>Disk mode is not started in this sketch.','connections','Connections');return;
    case 'storage':info('Storage','A home for your library.','Sample: 4 books · 3.2 GB free.<br>SD diagnostics and settings-encryption controls<br>remain separate, deliberate device actions.');return;
    case 'updates':navigate('updates');return;
    case 'startup-check':state.startupCheck=!state.startupCheck;break;
    case 'check-updates':info('Updates','You’re up to date.','Sample result for this UI preview.<br>No update check or installation was performed.','updates','Updates');return;
    case 'help':info('How to read','Find your rhythm.','BOOT / Space starts and pauses reading.<br>PWR / M switches between Menu and paused reading.<br>Tap the word to start or pause.');return;
    case 'sleep':info('Sleep & power','Pause, and come back.','Sleep behaviour is still a design decision,<br>especially with a running Focus timer.<br>This preview leaves your session running.');return;
    case 'language':info('Interface language','Read in your own language.','The real firmware supports installable locale packs.<br>This first sketch uses English interface text.','display','Text & display');return;
    case 'book-font':info('Book font','A font for this book.','The firmware supports per-book font selection.<br>This sketch uses the global preview font.','details','Book details');return;
    case 'book-language':info('Book language','Let the text lead.','Per-book language and shaping stay in the firmware.<br>This sketch does not simulate multilingual shaping.','details','Book details');return;
  }
  render();
}
// The page and locator share real word offsets. Only Read here commits a preview.
function chapterAt(index){
  return documents[state.book].chapters.findLastIndex(ch=>ch.start<=index);
}
function setWord(index){
  state.word=Math.max(0,Math.min(words.length-1,index));
  book().chapter=chapterAt(state.word);book().progress=progress();
  state.places[state.book]=state.word;state.pagePosition=null;
}
function openDocument(index){
  capturePage();state.places[state.book]=state.word;
  state.book=index;words=documents[index].words;
  setWord(state.places[index]??documents[index].chapters[books[index].chapter].start);
  state.returnPlace=null;state.browse=null;
}
function pageView(){
  const doc=documents[state.book];
  return `<div class="page-reader"><div class="page-toolbar">${go('‹ Menu','menu','muted')}<span class="page-context" data-page-context></span>${state.focus?button(focusLabel(),'focus-route','focus-badge','data-focus-label'):''}${state.returnPlace?button('↶ Return','return-place','return-place','aria-label="Return to the place before your jump"'):''}${button('Find place','find-place','secondary')}${button(state.mode==='chunk'?'Chunks ↗':'Word ↗','word-mode','accent','aria-label="Return to playback from the selected word"')}</div>
    <div class="page-content"><div class="page-scroll" tabindex="0" role="region" aria-label="Book text. Scroll to read. Tap a word to select your place." style="font-family:${state.font==='Literata'?'Georgia, serif':'Arial, sans-serif'}"><div class="page-text">${doc.paragraphs.map((p,i)=>`${i===0||doc.paragraphs[i-1].chapter!==p.chapter?`<div class="page-chapter-title">${p.chapter+1} / ${escapeHtml(doc.chapters[p.chapter].title)}</div>`:''}<p>${doc.words.slice(p.start,p.end).map((w,j)=>`<span data-word="${p.start+j}"${p.start+j===state.word?' class="chosen-word"':''}>${escapeHtml(w)}</span>`).join(' ')}</p>`).join('')}<div class="book-end">End of ${escapeHtml(book().title)}</div></div></div>
    <div class="page-rail">${button('↑','previous-page','','aria-label="Previous page, with one line of overlap"')}${button('↓','next-page','','aria-label="Next page, with one line of overlap"')}</div></div><div class="page-progress" aria-hidden="true"><span style="width:${progress()}%"></span></div></div>`;
}
let pageWordTops=[];
function mountPage(){
  const scroller=screen.querySelector('.page-scroll');if(!scroller)return;
  pageWordTops=[...scroller.querySelectorAll('[data-word]')].map(el=>el.offsetTop);
  const saved=state.pagePosition;
  const chapterStart=documents[state.book].chapters.some(ch=>ch.start===state.word);
  const heading=chapterStart?scroller.querySelector(`[data-word="${state.word}"]`).closest('p').previousElementSibling:null;
  scroller.scrollTop=saved?.book===state.book&&saved.word===state.word&&saved.font===state.font?saved.top:Math.max(0,heading?heading.offsetTop-7:pageWordTops[state.word]-8);
  state.pagePosition={book:state.book,word:state.word,top:scroller.scrollTop,font:state.font};
  updatePageChrome(scroller);
  scroller.addEventListener('scroll',()=>capturePage(),{passive:true});
  // Touch uses the browser's native pan/inertia. Mouse dragging emulates a finger.
  let drag=null;
  scroller.addEventListener('pointerdown',event=>{
    if(event.pointerType!=='mouse'||event.button!==0)return;
    drag={id:event.pointerId,y:event.clientY,top:scroller.scrollTop,moved:false};
  });
  scroller.addEventListener('pointermove',event=>{
    if(!drag)return;
    const distance=(event.clientY-drag.y)/(screen.getBoundingClientRect().width/640);
    if(Math.abs(distance)>5){drag.moved=true;scroller.setPointerCapture(drag.id);}
    if(drag.moved){event.preventDefault();scroller.scrollTop=drag.top-distance;}
  });
  scroller.addEventListener('pointerup',()=>{if(drag?.moved){scroller.dataset.suppressClick='true';}drag=null;});
  scroller.addEventListener('pointercancel',()=>{drag=null;});
  scroller.addEventListener('lostpointercapture',()=>{drag=null;});
  scroller.addEventListener('click',event=>{
    if(scroller.dataset.suppressClick){delete scroller.dataset.suppressClick;return;}
    const word=event.target.closest('[data-word]');if(!word)return;
    scroller.querySelector('.chosen-word')?.classList.remove('chosen-word');word.classList.add('chosen-word');
    setWord(Number(word.dataset.word));state.pagePosition={book:state.book,word:state.word,top:scroller.scrollTop,font:state.font};updatePageChrome(scroller);
  });
}
function capturePage(){
  const scroller=screen.querySelector('.page-scroll');if(!scroller||!state.pagePosition)return;
  if(state.pagePosition?.top!==scroller.scrollTop){
    // First complete line at the top; bottom padding also makes the last line reachable.
    let low=0,high=pageWordTops.length-1;
    while(low<high){const mid=(low+high)>>1;if(pageWordTops[mid]<scroller.scrollTop+4)low=mid+1;else high=mid;}
    setWord(low);
    scroller.querySelector('.chosen-word')?.classList.remove('chosen-word');
    state.pagePosition={book:state.book,word:state.word,top:scroller.scrollTop,font:state.font};
    updatePageChrome(scroller);
  }
}
function updatePageChrome(scroller){
  const title=`Ch. ${book().chapter+1} · ${progress()}%`;
  screen.querySelector('[data-page-context]').textContent=title;
  screen.querySelector('[data-page-context]').title=book().chapters[book().chapter];
  screen.querySelector('[data-action="previous-page"]').disabled=scroller.scrollTop<=1;
  screen.querySelector('[data-action="next-page"]').disabled=scroller.scrollTop>=scroller.scrollHeight-scroller.clientHeight-1;
  screen.querySelector('.page-progress span').style.width=progress()+'%';
}
function stepPage(direction){
  const scroller=screen.querySelector('.page-scroll');if(!scroller)return;
  scroller.scrollBy({top:direction*100,behavior:matchMedia('(prefers-reduced-motion: reduce)').matches?'instant':'smooth'});
  capturePage();
}
function beginBrowse(show=true){
  capturePage();pauseReading();
  if(!state.browse)state.browse={origin:state.word,originMode:state.readerSurface,originPage:state.pagePosition?{...state.pagePosition}:null,candidate:state.word,query:'',atChapter:false};
  if(show)navigate('find-place');
}
function cancelBrowse(){
  const browse=state.browse;if(!browse)return;
  state.browse=null;setWord(browse.origin);state.readerSurface=browse.originMode;state.pagePosition=browse.originPage;navigate('reader');
}
function commitBrowse(){
  const browse=state.browse;if(!browse)return;
  const origin={book:state.book,word:browse.origin,pagePosition:browse.originPage};
  state.browse=null;setWord(browse.candidate);state.readerSurface='page';state.returnPlace=origin;navigate('reader');
}
function findPlaceView(){
  return `<div class="screen-header locator-header">${button('‹ Cancel','cancel-browse','back')}<span class="heading">Find place</span>${go('Chapters','chapters','secondary')}${button('Search','open-search','secondary')}${button('Read here ↗','read-here','primary')}</div><div class="locator-body"><div class="location-preview" aria-live="polite"><div class="location-caption" data-location-caption></div><p data-location-text></p></div><div class="location-tools"><label class="book-slider" for="book-position"><span>Book position</span><output for="book-position" data-location-percent></output><input id="book-position" type="range" min="0" max="1000" value="${Math.round(state.browse.candidate/Math.max(1,words.length-1)*1000)}" aria-label="Preview a position in the book"></label><div class="paragraph-steps">${button('‹ Paragraph','browse-paragraph','secondary','data-delta="-1"')}${button('Paragraph ›','browse-paragraph','secondary','data-delta="1"')}</div></div></div>`;
}
function updateBrowse(){
  if(state.view!=='find-place'||!state.browse)return;
  const candidate=state.browse.candidate,doc=documents[state.book],chapter=chapterAt(candidate);
  screen.querySelector('[data-location-caption]').textContent=`${chapter+1} / ${doc.chapters[chapter].title}`;
  screen.querySelector('[data-location-text]').textContent=words.slice(candidate,candidate+55).join(' ');
  const percent=Math.round(candidate/Math.max(1,words.length-1)*100);
  screen.querySelector('[data-location-percent]').textContent=percent+'%';
  const slider=screen.querySelector('#book-position');slider.value=Math.round(candidate/Math.max(1,words.length-1)*1000);
  slider.setAttribute('aria-valuetext',`${percent} percent, chapter ${chapter+1}, ${doc.chapters[chapter].title}`);
  const paragraph=doc.paragraphs.findIndex(p=>p.end>candidate);
  screen.querySelector('[data-delta="-1"]').disabled=paragraph===0;
  screen.querySelector('[data-delta="1"]').disabled=paragraph===doc.paragraphs.length-1;
}
function searchBookView(){
  return `<div class="screen-header search-header">${go('‹ Find place','find-place','back')}${button(`${escapeHtml(state.browse.query||'Search this book')} <span>Edit ↗</span>`,'open-search','search-query','aria-label="Edit search with the on-screen keyboard"')}${button('Cancel','cancel-browse','accent')}</div><div class="search-results scroll-list" aria-label="Search results"></div>`;
}
function updateSearch(){
  const query=state.browse.query.trim().toLocaleLowerCase(),list=screen.querySelector('.search-results');
  if(!query){list.innerHTML='<div class="search-empty"><strong>A few words can take you there.</strong><span>Try “brass key”, “rain”, or “listening bench”.</span></div>';return;}
  const doc=documents[state.book],results=[];
  for(const p of doc.paragraphs){
    const lower=p.text.toLocaleLowerCase();let from=0,index;
    while((index=lower.indexOf(query,from))!==-1){
      const start=p.start+(p.text.slice(0,index).match(/\S+/g)?.length||0)-(index>0&&/\S/.test(p.text[index-1])?1:0);
      const before=p.text.slice(Math.max(0,index-34),index),after=p.text.slice(index+query.length,index+query.length+65);
      results.push({word:start,chapter:p.chapter,excerpt:(index>34?'…':'')+escapeHtml(before)+'<mark>'+escapeHtml(p.text.slice(index,index+query.length))+'</mark>'+escapeHtml(after)+(index+query.length+65<p.text.length?'…':'')});
      from=index+query.length;
    }
  }
  list.innerHTML=results.length?`<div class="search-count" role="status">${results.length} ${results.length===1?'match':'matches'} · Tap to preview</div>${results.map(r=>button(`<small>${r.chapter+1} / ${escapeHtml(doc.chapters[r.chapter].title)}</small><span>${r.excerpt}</span>`,'search-result','search-result',`data-word="${r.word}"`)).join('')}`:`<div class="search-empty" role="status"><strong>No matches for “${escapeHtml(state.browse.query.trim())}”</strong><span>Try a shorter phrase or a chapter title in Chapters.</span></div>`;
}
Object.assign(descriptions,{
  page:['02','Page reading','Move through the text, naturally.','Scroll, swipe, or drag to browse continuously. The large arrows move by four lines, keeping one line for context. Tap a word to select an exact starting point, then use Word to return to paused word-by-word reading.','Try Find place. Preview a chapter, scrub through the book, or search “brass key”. Read here confirms your jump; Return takes you back.'],
  reader:['02','Read','A word, with a page around it.','Word-by-word reading keeps its playback controls. Page opens continuous text at the same word, so you can look around whenever you need context.','Tap Page to browse. In page mode, tap a word, then Word to resume from that word.'],
  'find-place':['04','Find place','Look around. Keep your place.','The slider explores the whole book. Paragraph buttons refine the preview. Chapters and text search offer direct routes to a section. Nothing moves your reading place until you press Read here.','Cancel returns untouched. After Read here, use Return for the place you came from.'],
  'search-book':['04','Search','Remember a phrase? Start there.','Search the original sample story. Results show the chapter and surrounding text. Selecting one opens a preview; Read here confirms it. Browser typing stands in for device text entry, which still needs its own design.','Try “brass key”, “rain”, or “listening bench”. No search history or network requests.'],
  chapters:['04','Chapters','A direct route into the book.','Select a chapter, then Preview chapter to see its opening. Read here commits the jump; Cancel keeps your old place.','Scroll the chapter list or use its large arrows.'],
  focus:['06','Focus','For reading. Or anything else.','Read with Focus starts the timer beside paused reading. Timer only opens a standalone timer. Both continue in the background and wait for a manual start after each break.','Choose a preset with the arrows. Open Presets to add or edit without crowding the start screen.'],
  'focus-presets':['06','Presets','Small routines, neatly kept.','Tap a preset to use it, or Edit to change its name, minutes and rounds. Add preset stays in the header; setup stays off the start screen.','Edit a routine, save it, then tap its row to start.'],
});


const chunkMeasure=document.createElement('canvas').getContext('2d');
const chunkPlans=new Map();
function chunkGeometry(){
  const family=state.font==='Literata'?'Georgia, serif':'Arial, sans-serif';
  const available=Math.max(160,2*Math.min(640*state.anchor/100,640*(1-state.anchor/100))-48);
  chunkMeasure.font=`${state.fontSize}px ${family}`;
  const measure=text=>chunkMeasure.measureText(text).width+Math.max(0,Array.from(text).length-1)*state.tracking;
  return {family,available,measure};
}
function chunkPlan(){
  const key=[state.book,state.font,state.fontSize,state.tracking,state.anchor].join(':');
  if(chunkPlans.has(key))return chunkPlans.get(key);
  const {available,measure}=chunkGeometry();
  const groups=documents[state.book].paragraphs.flatMap(p=>ChunkReader.segment(words.slice(p.start,p.end),{fits:group=>measure(group.join(' '))<=available}).map(g=>({...g,start:g.start+p.start,end:g.end+p.start})));
  // Avoid retaining every slider position in a long browser session.
  if(chunkPlans.size>=24)chunkPlans.delete(chunkPlans.keys().next().value);
  chunkPlans.set(key,groups);return groups;
}
function playbackFrame(){
  if(state.mode!=='chunk')return {start:state.word,end:state.word+1,words:[words[state.word]]};
  const group=chunkPlan().find(g=>g.end>state.word);
  return {start:state.word,end:group.end,words:words.slice(state.word,group.end)};
}
function playbackMarkup(){
  if(state.mode!=='chunk')return wordMarkup(words[state.word]);
  const frame=playbackFrame(),text=frame.words.join(' '),{family,available,measure}=chunkGeometry();
  const fontSize=state.fontSize*Math.min(1,available/Math.max(1,measure(text)));
  return `<div class="chunk-layer" style="font-family:${family}"><div class="guide top" style="left:${state.anchor}%;width:${state.guideWidth}px;height:${state.guideGap}px"></div><span class="chunk-line" style="left:${state.anchor}%;font-size:${fontSize}px;letter-spacing:${state.tracking}px">${escapeHtml(text)}</span><div class="guide bottom" style="left:${state.anchor}%;width:${state.guideWidth}px;height:${state.guideGap}px"></div></div>`;
}
function stepChunk(direction){
  if(state.view!=='reader'||state.readerSurface!=='playback')return;
  pauseReading();
  if(state.mode==='chunk'){
    const groups=chunkPlan(),index=groups.findIndex(g=>g.end>state.word);
    if(direction<0&&state.word>groups[index].start)setWord(groups[index].start);
    else setWord(groups[Math.max(0,Math.min(groups.length-1,index+direction))].start);
  }else setWord(state.word+direction);
  render();
}
function updateChunkDemo(){
  const status=$('#chunk-status');if(!status)return;
  const frame=playbackFrame(),duration=Math.round(ChunkReader.duration(frame.words,state.wpm,state));
  status.textContent=`${state.mode==='chunk'?'Context chunks':'Single word'} · ${frame.words.length} ${frame.words.length===1?'word':'words'} · ${duration} ms at ${state.wpm} WPM, including pauses`;
  document.querySelectorAll('.chunk-demo [data-action="previous-chunk"],.chunk-demo [data-action="next-chunk"]').forEach(b=>{
    b.disabled=state.view!=='reader'||state.readerSurface!=='playback'||(b.dataset.action==='previous-chunk'?state.word===0:frame.end>=words.length);
  });
  for(const [action,mode] of [['demo-single','rsvp'],['demo-chunks','chunk']]){
    const b=document.querySelector(`.chunk-demo [data-action="${action}"]`);b.setAttribute('aria-pressed',state.mode===mode);
  }
}
function openSearchKeyboard(){
  state.keyboard={value:state.browse.query,cursor:state.browse.query.length,shifted:false,mode:0};navigate('search-keyboard');
}
function searchKeyboardView(){
  const kb=state.keyboard;
  const rows=kb.mode===0?['qwertyuiop'.split(''),[...'asdfghjkl','backspace'],['shift',...'zxcvbnm','mode',"'"]]:kb.mode===1?['1234567890'.split(''),[...'@#$%&*()-','backspace'],['mode',...'+=_/:;?!', '.']]:['!@#$%^&*()'.split(''),[...'[]{}<>+=~','backspace'],['mode','\\','|',';',':',"'",'"',',','/','?']];
  const keyButton=key=>{
    const label=key==='backspace'?'⌫':key==='shift'?(kb.shifted?'⇧ On':'⇧'):key==='mode'?['123','#+=','ABC'][kb.mode]:kb.shifted?key.toUpperCase():key;
    const action=key==='shift'?'keyboard-shift':key==='mode'?'keyboard-mode':'keyboard-key';
    return button(escapeHtml(label),action,['shift','mode','backspace'].includes(key)?'keyboard-special':'keyboard-letter',`data-key="${escapeHtml(key)}" aria-label="${key==='backspace'?'Backspace':key==='shift'?'Shift':key==='mode'?'Change keyboard symbols':escapeHtml(label)}"${key==='shift'?` aria-pressed="${kb.shifted}"`:''}`);
  };
  return `<div class="keyboard-screen"><div class="keyboard-input-row"><span>Search</span><input id="book-search" type="text" inputmode="none" autocomplete="off" autocapitalize="off" spellcheck="false" maxlength="80" value="${escapeHtml(kb.value)}" placeholder="Word or phrase…" aria-label="Search this book">${button('Clear','clear-search','accent')}</div><div class="keyboard-body"><div class="keyboard-letters">${rows.map(row=>`<div class="keyboard-row">${row.map(keyButton).join('')}</div>`).join('')}</div><div class="keyboard-actions">${button('Space','keyboard-key','secondary','data-key=" " aria-label="Space"')}${button('Cancel','cancel-keyboard','secondary')}${button('Search ↗','submit-search','primary',kb.value.trim()?'':'disabled')}</div></div></div>`;
}
function mountKeyboard(){
  const input=screen.querySelector('#book-search');if(!input)return;
  input.focus({preventScroll:true});input.setSelectionRange(state.keyboard.cursor,state.keyboard.cursor);
}
function keyboardKey(key){
  const kb=state.keyboard,input=screen.querySelector('#book-search');if(!kb||!input)return;
  let start=input.selectionStart??kb.value.length,end=input.selectionEnd??start;
  if(key==='backspace'){
    if(start===end&&start>0)start-=Array.from(kb.value.slice(0,start)).at(-1).length;
    kb.value=kb.value.slice(0,start)+kb.value.slice(end);kb.cursor=start;
  }else{
    const text=kb.shifted?key.toUpperCase():key;
    const next=kb.value.slice(0,start)+text+kb.value.slice(end);
    if(next.length>80)return;
    kb.value=next;kb.cursor=start+text.length;kb.shifted=false;
  }
  render();
}
Object.assign(descriptions,{
  reader:['02','Read','A word. Or a small thought.','Choose Word by word or Context chunks in Reading settings. Chunks use English phrase cues and the available width to group up to three words. Page opens manual browsing without changing your chosen playback mode.','Use Try context chunks below. Step through, compare Single word, or press Start reading. WPM counts words in both modes.'],
  page:['02','Page browsing','Your place, with context.','Page is a manual browsing surface, separate from playback mode. Scroll or step through the text; tap a word and return to Word or Chunks at that exact position.','Find place opens chapters, a position slider, or search with an on-screen keyboard.'],
  pace:['07','Reading','Two playback modes. One place.','Reading mode switches between Word by word and Context chunks. Page browsing is accessed from the reader. Your word-pacing controls apply in both playback modes.','At 300 WPM, two words get 400 ms and three get 600 ms before any extra word or punctuation pauses.'],
  'search-keyboard':['04','Search keyboard','Give typing the whole display.','A dedicated keyboard uses three rows of letters and a side column for Space, Cancel, and Search. Your query stays visible. Results get their own full screen after submission.','Tap the keys to enter “brass key”, then Search. Shift, numbers, symbols, backspace, and Clear work. A physical browser keyboard also works.'],
  'search-book':['04','Search results','The keyboard makes room for results.','After Search, the keyboard closes and results fill the display. Tap the query to edit it, or a result to preview that passage. Editing can be cancelled without changing the previous search.','Choose a result, then Read here. Your previous reading position stays available through Return.'],
});


document.addEventListener('click',event=>{
  const element=event.target.closest('button');if(!element||element.disabled)return;
  if(element.dataset.go)navigate(element.dataset.go);
  else if(element.dataset.action)action(element.dataset.action,element);
});
screen.addEventListener('input',event=>{
  if(event.target.id==='book-position'){state.browse.candidate=Math.round(Number(event.target.value)/1000*(words.length-1));state.browse.atChapter=false;updateBrowse();return;}
  if(event.target.id==='book-search'&&state.keyboard){state.keyboard.value=event.target.value;state.keyboard.cursor=event.target.selectionStart;screen.querySelector('[data-action="submit-search"]').disabled=!state.keyboard.value.trim();return;}
  if(event.target.dataset.update){state[event.target.dataset.update]=event.target.value;return;}
  if(event.target.id==='preset-name'){
    state.presetDraft.name=event.target.value;
    screen.querySelector('[data-action="save-preset"]').disabled=!event.target.value.trim();return;
  }
  const key=event.target.dataset.setting;if(!key)return;
  state[key]=Number(event.target.value);
  event.target.parentElement.querySelector('output').textContent=state[key]+(key==='anchor'?'%':key.endsWith('Delay')?' ms':'');
  const specimen=screen.querySelector('.word-zone');if(specimen)specimen.innerHTML=wordMarkup('reading',true);
  placePhantomWords();
});
$('#theme').addEventListener('change',event=>{state.theme=event.target.value;render();});
$('#boot').addEventListener('click',()=>toggleReading());
$('#power').addEventListener('click',()=>navigate(state.view==='menu'?'reader':'menu'));
$('#reset-demo').addEventListener('click',()=>{pauseReading();clearTimeout(uploadTimeout);state=initialState();words=documents[0].words;books.forEach((b,i)=>{b.progress=[42,18,6,0,0][i];b.chapter=[3,1,0,0,0][i];});navigate('menu');});
document.addEventListener('keydown',event=>{
  if(state.view==='search-keyboard'){if(event.key==='Enter'){event.preventDefault();action('submit-search');}else if(event.key==='Escape'){event.preventDefault();action('cancel-keyboard');}return;}
  if(event.target.matches('input,select,textarea,[contenteditable]'))return;
  if(state.view==='library'&&(event.key==='ArrowLeft'||event.key==='ArrowRight')){
    event.preventDefault();action(event.key==='ArrowLeft'?'previous-book':'next-book');return;
  }
  if(state.readerSurface==='page'&&state.view==='reader'&&['ArrowDown','ArrowUp','PageDown','PageUp'].includes(event.key)){event.preventDefault();stepPage(['ArrowUp','PageUp'].includes(event.key)?-1:1);return;}
  if(event.code==='Space'&&event.target.matches('button,a'))return;
  if(event.code==='Space'){event.preventDefault();toggleReading();}
  else if(event.key.toLowerCase()==='m'){event.preventDefault();navigate(state.view==='menu'?'reader':'menu');}
  else if(event.key==='Escape'){event.preventDefault();if(state.browse)cancelBrowse();else navigate('menu');}
});
window.addEventListener('hashchange',()=>navigate(location.hash.slice(1),{hash:false}));
function fitScreen(){
  const stage=$('.device-stage'),shell=$('.device-shell');
  const stageStyle=getComputedStyle(stage),shellStyle=getComputedStyle(shell);
  const usable=stage.clientWidth-parseFloat(stageStyle.paddingLeft)-parseFloat(stageStyle.paddingRight)-parseFloat(shellStyle.paddingLeft)-parseFloat(shellStyle.paddingRight)-2;
  const scale=Math.max(.25,Math.min($('#scale').value==='native'?1:1.65,usable/640));
  const viewport=$('.screen-viewport');viewport.style.width=640*scale+'px';viewport.style.height=172*scale+'px';screen.style.transform=`scale(${scale})`;
}
$('#scale').addEventListener('change',fitScreen);
new ResizeObserver(fitScreen).observe($('.device-stage'));
$('#demo-start').addEventListener('click',()=>{state.readerSurface='playback';state.mode='rsvp';startFocus('reader',true);state.playing=true;render();queueWord();});
$('#demo-expire').addEventListener('click',()=>{if(state.focus?.phase==='focus'){state.focus.until=Date.now();state.focus.running=true;focusTick();}});
$('#demo-break').addEventListener('click',()=>{if(state.focus?.phase==='break'){state.focus.until=Date.now();focusTick();}});
setInterval(()=>focusTick(),200);
if(location.hash&&location.hash!=='#startup')navigate(location.hash.slice(1));
else navigate('menu');
fitScreen();
