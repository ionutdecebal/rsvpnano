'use strict';
// Study 17 extends the working Study 08 prototype. Book offsets, page browsing,
// keyboard editing, preset storage and the Focus state machine remain shared.
const inherited={render,navigate,action,focusTick,queueWord,toggleReading,wordMarkup,pageView,findPlaceView};
function proposalDefaults(){
  const fresh=state.interfaceStyle===undefined;
  Object.assign(state,{interfaceStyle:'gestures',textAction:state.textAction??'tap',locale:state.locale??'en',editTab:state.editTab??'text',
    detailReading:state.detailReading??'essentials',detailPaused:state.detailPaused??'detailed',detailPreview:state.detailPreview??'paused'});
  if(fresh){state.fontSize=52;state.anchor=35;state.guideWidth=64;state.guideGap=8;state.longDelay=200;state.complexDelay=200;state.punctuationDelay=200;}
}
proposalDefaults();
const detailChoices=['quiet','essentials','detailed'];
function detailFlags(reading=state.playing){const level=state[reading?'detailReading':'detailPaused'];return {chapter:level==='detailed',battery:level!=='quiet',progress:level!=='quiet'};}
function h14(title,parent='settings',rightLabel=t('backBook')+' ↗',rightAction='reader'){
  return `<div class="header14">${go('‹ '+t(({pace:'reading'})[parent]||parent),parent,'back')}<strong>${title}</strong>${go(rightLabel,rightAction,'accent')}</div>`;
}
function tile14(label,value,actionName,attrs='',cls=''){
  return button(`<span>${label}</span><strong>${value}</strong>`,actionName,'tile14 '+cls,attrs);
}
function slider14(key,label,min,max,step=1,unit='',vertical=false){
  const value=key==='bookPosition'?Math.round(state.browse.candidate/Math.max(1,words.length-1)*1000):state[key];
  const shown=key==='bookPosition'?Math.round(value/10)+'%':value+unit;
  const content=`<span>${vertical?icon('sun'):label}</span><output>${shown}</output>`;
  return `<div class="full-slider ${vertical?'vertical':''}" role="slider" tabindex="0" data-slide="${key}" data-unit="${unit}" data-step="${step}" aria-label="${label}" aria-valuemin="${min}" aria-valuemax="${max}" aria-valuenow="${value}" aria-orientation="${vertical?'vertical':'horizontal'}" style="--fill:${(value-min)/(max-min)*100}%"><div class="slider-fill"></div><div class="slider-content">${content}</div><div class="slider-content filled" aria-hidden="true">${content}</div></div>`;
}
menuView=function(){return `<div class="menu-layout"><div class="current-book"><div class="micro">${state.focus?focusLabel():t('saved')}</div><h2>${book().title}</h2><div class="author">${book().author}</div><div class="book-progress"><div class="progress-line"><span style="width:${progress()}%"></span></div><span>${progress()}% · ${t('chapter')} ${book().chapter+1}</span></div><div class="book-actions">${go(t('backBook')+' ↗','reader','primary')}${button(focusLabel(),'focus-route','accent','data-focus-label')}</div></div><nav class="menu-nav">${go(icon('chapters')+t('chapters')+'<span class="chevron">›</span>','chapters')}${go(icon('book')+t('library')+'<span class="chevron">›</span>','library')}${go(icon('settings')+t('settings')+'<span class="chevron">›</span>','settings')}</nav></div>`;};
settingsView=function(){return h14(t('settings'),'menu')+`<div class="settings14">${[
  ['reading','readingHint','pace','pace'],['appearance','appearanceHint','appearance','type'],['display','displayHint','display','sun'],
  ['connections','connectionsHint','connections','connect'],['device','deviceHint','device','settings'],['help','helpHint','help','device']
].map(([key,hint,view,symbol])=>settingTile(t(key),t(hint),view,symbol)).join('')}</div>`;};
function readingCue(reading=state.playing){return t(reading?(gestureDrag?.holding?'releasePause':'tapPause'):state.textAction==='hold'?'holdRead':'tapRead');}
readerView=function(){
  if(state.readerSurface==='page')return pageView();
  const flags=detailFlags(),chapter=`${t('chapter')} ${book().chapter+1} · ${book().chapters[book().chapter]}`;
  return `<div class="reader reader14 gesture-reader quiet-reader"><div class="reader-top">${button(chapter,'chapters',flags.chapter?'':'hide')}${state.focus?button(focusLabel(),'focus-route','focus-badge','data-focus-label'):''}<span class="battery ${flags.battery?'':'hide'}">${icon('battery')} 84%</span></div>${button(playbackMarkup(),'play','word-zone',`aria-label="${readingCue()}"`)}<div class="reader-footer"><button class="reveal-controls" data-go="gesture-tray" aria-label="${t('trayTitle')}"><span class="gesture-handle" aria-hidden="true"></span></button><span class="reader-progress ${flags.progress?'':'hide'}">${footerText()}</span></div><div class="reader-track" style="width:${flags.progress?progress():0}%"></div></div>`;
};
function quickControlsView({practice=false}={}){
  const wpm=practice?tour.wpm:state.wpm;
  return `<div class="quick-layout"><div class="quick-pace"><div class="quick-title"><span class="micro">${practice?'2 / 9 · ':''}${t('trayTitle')}</span>${practice?button(t('skipTour'),'skip-intro','quick-skip'):''}</div><div class="quick-wpm">${button('−',practice?'intro-slower':'slower','',`aria-label="${t('slower')}"`)}<output>${wpm}<small>WPM</small></output>${button('+',practice?'intro-faster':'faster','',`aria-label="${t('faster')}"`)}</div><div class="quick-return">${practice?button('‹','intro-back','secondary',`aria-label="${t('tourBack')}"`)+button(t('next')+' ↗','intro-next','primary'):go(t('backBook')+' ↓','reader','primary')}</div></div><nav class="menu-nav quick-nav">${practice?button(icon('settings')+t('menu')+'<span class="chevron">›</span>','intro-menu',''):go(icon('settings')+t('menu')+'<span class="chevron">›</span>','menu')}${button(icon('book')+t('page')+'<span class="chevron">›</span>',practice?'intro-page':'page-mode')}</nav>${practice?tourBrightnessSlider():slider14('brightness',t('brightness'),5,100,1,'%',true)}</div>`;
}
function gestureTrayView(){return quickControlsView();}
function sample14(chrome=false){
  const reading=state.detailPreview==='reading',flags=detailFlags(reading);
  return `<div class="sample14 ${chrome?'with-chrome':''}">${chrome?`<div class="sample-top"><span style="visibility:${flags.chapter?'visible':'hidden'}">${t('chapter')} 4</span><span style="visibility:${flags.battery?'visible':'hidden'}">▰ 84%</span></div>`:''}<div class="word-zone">${wordMarkup(state.mode==='chunk'?'read on':'reading',true)}</div>${chrome?`<div class="sample-bottom"><span><span class="sample-handle"></span></span><span style="visibility:${flags.progress?'visible':'hidden'}">42%</span></div>`:''}<div class="sample-caption">${chrome?t(reading?'previewReading':'previewPaused'):state.mode==='chunk'?t('centred'):state.font+' · '+state.fontSize+' px'}</div></div>`;
}
appearanceView=function(){
  const tab=state.editTab,chunks=state.mode==='chunk';
  let controls;
  if(tab==='text')controls=slider14('tracking',t('tracking'),-2,4)+
    (chunks?`<div class="centred-note"><span>${t('anchor')}</span><strong>${t('centred')}</strong></div>`:slider14('anchor',t('anchor'),20,70,1,'%'))+
    `<div class="font-row14">${tile14('Font',state.font,'font')}${tile14(t('text'),t(({52:'large',43:'medium',33:'small',14:'compact'})[state.fontSize]),'font-size')}</div>`;
  else if(tab==='guides')controls=slider14('guideWidth',t('width'),0,140,2)+slider14('guideGap',t('gap'),0,18)+`<div class="guide-toggles14">${tile14(t('phantoms'),t(state.phantom?'on':'off'),'phantom',chunks?'disabled':'')}${tile14(t('focal'),t(chunks?'offChunks':state.highlight?'on':'off'),'highlight',chunks?'disabled':'')}</div>`;
  else controls=tile14(t('whileReading'),t(state.detailReading),'detail-choice','data-when="reading"',state.detailPreview==='reading'?'selected':'')+
    tile14(t('whilePaused'),t(state.detailPaused),'detail-choice','data-when="paused"',state.detailPreview==='paused'?'selected':'')+
    `<p class="detail-hint">${t(state[state.detailPreview==='reading'?'detailReading':'detailPaused']+'Hint')}</p>`;
  return h14(t('appearance'),'settings',t('try')+' ↗')+`<div class="editor14"><nav class="editor-tabs" aria-label="Appearance sections">${['text','guides','onScreen'].map(key=>button(t(key),'edit-section',key===tab?'selected':'',`data-tab="${key}" aria-pressed="${key===tab}"`)).join('')}</nav><div class="editor-preview">${sample14(tab==='onScreen')}</div><div class="editor-controls">${controls}</div></div>`;
};
displayView=function(){return h14(t('display'))+`<div class="display14">${tile14(t('theme'),currentTheme().name,'theme')}${button(`<span>${t('language')}</span><strong>${languageNames[state.locale]} ↗</strong>`,'open-language','tile14')}${tile14(t('sleep'),state.standby?state.standby+' '+t('minutes'):t('none'),'standby')}${tile14(t('screensaver'),state.saver,'screensaver')}</div>`;};
function languageView(){return h14(t('language'),'display')+`<div class="language14">${Object.entries(languageNames).map(([code,label])=>button(`<strong>${label}</strong><span>${state.locale===code?'✓':''}</span>`,'choose-language',state.locale===code?'selected':'',`data-locale="${code}" aria-pressed="${state.locale===code}"`)).join('')}</div><p class="language-note">${t('languageHint')}</p>`;}
paceView=function(){return h14(t('reading'))+`<div class="pace14"><div class="behavior14">${tile14(t('mode'),t(state.mode==='chunk'?'chunks':'word'),'reading-mode')}${tile14(t('pauseMode'),t(state.pauseMode==='Instant'?'instant':'sentence'),'pause-mode')}${tile14(t('textAction'),t(state.textAction),'text-action')}${go(t('bookSettings')+' ↗','book-fonts','secondary book-settings-link')}</div><div class="pacing14">${slider14('longDelay',t('longWords'),0,1000,50,' ms')}${slider14('complexDelay',t('complexWords'),0,1000,50,' ms')}${slider14('punctuationDelay',t('punctuation'),0,1000,50,' ms')}</div></div>`;};
bookFontsView=function(){return h14(t('bookSettings'),'pace')+`<div class="screen-body book-font-settings"><div class="book-font-title"><span class="micro">${t('bookSettings')}</span><h2>${escapeHtml(book().title)}</h2></div><div class="book-font-controls">${tile14(t('typeface'),state.bookFont==='Reader default'?t('readerDefault'):state.bookFont,'cycle-book-font')}${tile14(t('bookLanguage'),state.bookLanguage==='Auto'?t('auto'):state.bookLanguage,'cycle-book-language')}</div></div>`;};
focusLabel=function(){if(!state.focus)return t('startFocus');const f=state.focus;return t(f.phase==='break'?'break':f.phase==='ready'?'breakDone':f.phase==='complete'?'sessionDone':f.running?'focus':'paused')+(f.phase==='ready'||f.phase==='complete'?'':' '+timeLabel(remaining()));};
focusStartView=function(){const preset=state.presets[state.presetIndex];return h14(t('focus'),'menu',t('presets')+' ↗','focus-presets')+`<div class="focus-start14"><div><div class="micro">${escapeHtml(preset.name)}</div><div class="stepper">${button('‹','previous-preset') }<output>${preset.focus}</output><span>${t('minutes')}</span>${button('›','next-preset')}</div><p>${preset.rest} ${t('minutes')} ${t('break').toLowerCase()} · ${preset.rounds} ${t('rounds')}</p></div><div class="focus-actions14">${button(t('readFocus')+' ↗','start-focus-book','primary')}${button(t('timerOnly'),'start-focus-full','secondary')}</div></div>`;};
focusFullView=function(){
  const f=state.focus,rest=f.phase==='break',ready=f.phase==='ready',complete=f.phase==='complete';
  const statusKey=rest?'timeBreak':ready?'breakDone':complete?'sessionDone':f.running?'timeFocus':'timerPaused';
  const primary=ready?button(t(f.withBook?'resumeReading':'nextRound'),'resume-round','primary'):complete?button(t('finish'),'finish-session','primary'):rest?`<p>${t('breakHint')}</p>`:button(t(f.running?'pauseTimer':'resumeTimer'),'toggle-focus','secondary');
  return `<div class="timer-layout timer14 ${rest?'break-timer':''}">${go('‹ '+t(f.withBook&&!rest&&!ready?'backBook':'menu'),f.withBook&&!rest&&!ready?'reader':'menu')}<div class="timer-main"><div class="micro" data-focus-status>${t(statusKey)}</div><div class="timer-value" data-timer>${timeLabel(remaining())}</div><div class="progress-line"><span data-timer-progress style="width:${100-remaining()/f.total*100}%"></span></div><div class="timer-caption">${t(rest||ready?'manualResume':f.withBook?'keepsRunning':'yourTime')}</div></div><div class="action-stack"><div class="session-meta">${escapeHtml(f.name)}<small>${t('round')} ${f.round} / ${f.rounds}</small></div>${primary}${go(t('endFocus'),'focus-end','accent')}</div></div>`;
};
function focusEndView(){return h14(t('endFocus'),'focus',t('keepFocus'),'focus-full')+`<div class="end-focus14"><div><h2>${t('endQuestion')}</h2><p>${t('endHint')}</p></div>${button(t('endFocus'),'end-focus','danger secondary')}</div>`;}
focusPresetsView=function(){return h14(t('presets'),'focus', '+ '+t('addPreset'),'preset-add')+`<div class="preset-list scroll-list">${state.presets.map((p,i)=>`<div class="preset-row">${button(`<strong>${escapeHtml(p.name)}</strong><small>${p.focus} ${t('minutes')} ${t('focus')} · ${p.rest} ${t('minutes')} ${t('break')} · ${p.rounds} ${t('rounds')}</small>`,'choose-preset',state.presetIndex===i?'selected':'',`data-index="${i}"`)}${button(t('edit'),'edit-preset','secondary',`data-index="${i}"`)}</div>`).join('')}</div>`;};
presetEditorView=function(){const d=state.presetDraft;return `<div class="header14">${go('‹ '+t('cancel'),'focus-presets','back')}<strong>${t(d.index===null?'addPreset':'edit')}</strong>${button(t('save'),'save-preset','accent',d.name.trim()?'':'disabled')}</div><div class="preset-edit14"><label>${t('name')}<input id="preset-name" value="${escapeHtml(d.name)}" maxlength="24" aria-label="${t('name')}"></label>${[['focus','duration'],['rest','breakMinutes'],['rounds','rounds']].map(([key,label])=>`<div><span>${t(label)}</span><div class="mini-step">${button('−','preset-value','',`data-key="${key}" data-delta="-1"`)}<output>${d[key]}</output>${button('+','preset-value','',`data-key="${key}" data-delta="1"`)}</div></div>`).join('')}</div>`;};
pageView=function(){let html=inherited.pageView();const start=html.indexOf('<div class="page-toolbar">'),end=html.indexOf('<div class="page-content">');return html.slice(0,start)+`<div class="page-toolbar page-toolbar14">${go('‹ '+t('menu'),'menu','muted')}<span class="page-context" data-page-context></span>${button(t('find'),'find-place','secondary')}${button(t('backBook')+' ↗','word-mode','accent')}</div>`+html.slice(end);};
findPlaceView=function(){return `<div class="screen-header locator-header">${button('‹ '+t('cancel'),'cancel-browse','back')}<span class="heading">${t('find')}</span>${go(t('chapters'),'chapters','secondary')}${button(t('search'),'open-search','secondary')}${button(t('readHere')+' ↗','read-here','primary')}</div><div class="locator-body"><div class="location-preview" aria-live="polite"><div class="location-caption" data-location-caption></div><p data-location-text></p></div><div class="location-tools">${slider14('bookPosition',t('position'),0,1000)}<div class="paragraph-steps">${button('‹ '+t('paragraph'),'browse-paragraph','secondary','data-delta="-1"')}${button(t('paragraph')+' ›','browse-paragraph','secondary','data-delta="1"')}</div><span hidden data-location-percent></span><input hidden id="book-position" type="range" min="0" max="1000"></div></div>`;};

wordMarkup=function(word,preview=false){
  const family=state.font==='Literata'?'Literata, Georgia, serif':'Atkinson, Arial, sans-serif';
  const centred=state.mode==='chunk',anchor=centred?50:state.anchor;
  const guides=state.guideWidth?`<div class="line-guide top" style="left:${anchor}%;width:${state.guideWidth}px;top:${Math.max(0,9-state.guideGap/2)}px"></div><div class="line-guide bottom" style="left:${anchor}%;width:${state.guideWidth}px;bottom:${Math.max(0,9-state.guideGap/2)}px"></div>`:'';
  if(centred)return `<div class="chunk-layer">${guides}<span class="chunk-line" style="left:50%;font-family:${family};font-size:${preview?Math.min(36,state.fontSize):state.fontSize}px;letter-spacing:${state.tracking}px">${escapeHtml(word)}</span></div>`;
  return inherited.wordMarkup(word,preview).replace(/Georgia, serif|Arial, sans-serif/g,family).replace(/<div class="guide (?:top|bottom)"[^>]*><\/div>/g,'').replace('</div></div>', '</div>'+guides+'</div>');
};
chunkGeometry=function(){const family=state.font==='Literata'?'Literata, Georgia, serif':'Atkinson, Arial, sans-serif';chunkMeasure.font=`${state.fontSize}px ${family}`;return {family,available:592,measure:text=>chunkMeasure.measureText(text).width+Math.max(0,Array.from(text).length-1)*state.tracking};};
playbackMarkup=function(){if(state.mode!=='chunk')return wordMarkup(words[state.word]);const {words:group}=playbackFrame();const {measure}=chunkGeometry();const text=group.join(' ');return wordMarkup(text).replace('font-size:'+state.fontSize+'px','font-size:'+(state.fontSize*Math.min(1,592/measure(text)))+'px');};
queueWord=function(){clearTimeout(readingTimeout);if(!state.playing||state.readerSurface==='page')return;const frame=playbackFrame();const delay=state.mode==='chunk'?Math.max(300,ChunkReader.duration(frame.words,state.wpm,state)):ChunkReader.duration(frame.words,state.wpm,state);readingTimeout=setTimeout(()=>{
  if(!state.playing||state.view!=='reader'||state.readerSurface==='page')return;
  if(frame.end>=words.length){pauseReading();render();return;}setWord(frame.end);
  const zone=screen.querySelector('.word-zone');if(zone)zone.innerHTML=playbackMarkup();placePhantomWords();
  const progressEl=screen.querySelector('.reader-progress');if(progressEl)progressEl.textContent=footerText();
  const track=screen.querySelector('.reader-track');if(track)track.style.width=(detailFlags().progress?progress():0)+'%';
  const chapter=screen.querySelector('[data-action="chapters"]');if(chapter)chapter.textContent=`${t('chapter')} ${book().chapter+1} · ${book().chapters[book().chapter]}`;
  updateChunkDemo();queueWord();
},delay);};
toggleReading=function(){if(state.focus&&!state.focus.withBook&&state.view==='focus-full'&&state.focus.phase==='focus'){action('toggle-focus');return;}inherited.toggleReading();};
focusTick=function(){const f=state.focus;if(!f)return;inherited.focusTick();};

window.studyViews={language:languageView,'gesture-tray':gestureTrayView,'focus-end':focusEndView};
Object.assign(descriptions,{
  appearance:['05','Appearance','Fewer pages. A clearer preview.','Three large, stable sections replace the typography subpages and separate Reading/Paused editors. Grouped information presets show you what will change.','Try Text, Guides, and On screen. On screen cycles the Reading or Paused preset and previews that state immediately.'],
  language:['07','Interface language','Same layout. Different words.','English, German and Romanian copy exercises the proposed flows. Labels get room to wrap instead of shrinking the touch targets. Sample book text keeps its original language.','Switch language here or in the laptop toolbar. Then revisit the introduction, Appearance and Focus.'],
  'gesture-tray':['02','Reading controls','The controls are a swipe away.','Swiping up pauses reading and opens a full-screen controls panel. Speed and brightness sit alongside Menu and Page, using the same layout as the main menu. Back to book returns paused.','Change WPM or brightness, or open Menu and Page. Back to book returns paused.'],
  reader:['02','Read','Just you and the book.','The introduction teaches playback and the upward swipe. Everyday reading keeps the text, your chosen information and a small controls handle.','Tap the text to read or pause; hold if selected in Reading settings. Swipe up for WPM, Menu and Page.'],
  settings:['07','Settings','Settings grouped by the job they do.','Appearance handles the look of the reader. Reading covers mode and pacing, including a direct route to the current book’s font/language settings.','Help contains the quick reference and Replay introduction.'],
  focus:['06','Focus','A timer with its own place.','Read with Focus opens paused reading with a running badge. Timer only stays on the timer screen. Both continue when you visit Menu, and neither restarts reading automatically after a break.','Try Timer only. Leave for Menu and return through the active Focus badge.'],
  display:['07','Display','Language belongs in the interface.','Theme and interface language work throughout the proposed flows. Sleep and screensaver remain illustrative in this browser sketch. Brightness lives in the swipe-up controls.','Try German or Romanian, then revisit Appearance and Help.'],
  'find-place':['04','Find place','Look around. Keep your place.','The slider explores the whole book. Paragraph buttons refine the preview. Chapters and search offer direct routes to a section. Your reading place changes only when you choose Read here.','Cancel keeps your old place. Read here confirms the passage you have previewed.'],
  'search-book':['04','Search results','The keyboard makes room for results.','Search closes the keyboard and fills the display with results. Tap the query to edit it, or a result to preview that passage. Cancelling an edit keeps the previous search.','Choose a result, then Read here to confirm your new place.'],
  'book-fonts':['07','Current book','An override for this book.','Book font and language remain separate from interface language. These inherited selectors are illustrative; this sketch does not reproduce the device font catalog or shaping.','Return to Reading for playback mode and pacing.'],
  pace:['07','Reading','Mode and timing, together.','The full-tile sliders stay. WPM is a swipe away in the reading controls. The current book’s font and language are reached here without adding another Settings tile.','Switch to chunks and return to reading. Chunks keep a fixed centre and never show the red focal letter.'],
});

navigate=function(view,options){proposalDefaults();if(view==='preset-add'){action('add-preset');return;}inherited.navigate(view,options);};
action=function(name,element){
  if(name==='text-action'){state.textAction=state.textAction==='tap'?'hold':'tap';render();return;}
  if(name==='open-language'||name==='language-cycle'){navigate('language');return;}
  if(name==='choose-language'){state.locale=element.dataset.locale;render();return;}
  if(name==='edit-section'){state.editTab=element.dataset.tab;render();return;}
  if(name==='detail-choice'){const when=element.dataset.when,key=when==='reading'?'detailReading':'detailPaused';state[key]=detailChoices[(detailChoices.indexOf(state[key])+1)%detailChoices.length];state.detailPreview=when;render();return;}
  if(name==='font-size'){const sizes=[52,43,33,14];state.fontSize=sizes[(sizes.indexOf(state.fontSize)+1)%sizes.length];render();return;}
  if(name==='slower'||name==='faster'){state.wpm=Math.max(100,Math.min(1000,state.wpm+(name==='slower'?-10:10)));render();queueWord();return;}
  if(name==='demo-timer-only'){startFocus('focus-full',true);return;}
  if((name==='highlight'||name==='phantom')&&state.mode==='chunk')return;
  inherited.action(name,element);
};
function translateInherited(){
  if(state.locale==='en')return;
  const map=new Map(Object.values(uiCopy).map(row=>[row[0],row[{de:1,ro:2}[state.locale]]]));
  for(const [a,key] of [['Start Focus','startFocus'],['Back to book','backBook'],['End session','endFocus'],['Use preset','presets'],['Book fonts','bookSettings'],['Book language','bookLanguage']])map.set(a,t(key));
  const walker=document.createTreeWalker(screen,NodeFilter.SHOW_TEXT);const nodes=[];while(walker.nextNode())nodes.push(walker.currentNode);
  for(const node of nodes){if(node.parentElement.closest('.word-zone,.page-text,input,.keyboard-letter'))continue;const value=node.textContent.trim();const core=value.replace(/^[‹+]\s*/,'').replace(/\s*[↗→↑↓]$/,'');if(map.has(core))node.textContent=node.textContent.replace(core,map.get(core));}
}
render=function({keepReaderPointer=false}={}){
  if(sliderDrag&&screen.hasPointerCapture(sliderDrag.pointerId))screen.releasePointerCapture(sliderDrag.pointerId);
  sliderDrag=null;
  if(!keepReaderPointer)cancelReaderGesture();
  proposalDefaults();inherited.render();screen.dataset.controls=state.interfaceStyle;screen.lang=state.locale;screen.style.filter='none';$('#preview-language').value=state.locale;translateInherited();
  if(state.view==='focus-presets'){const add=screen.querySelector('[data-go="preset-add"]');if(add)add.disabled=state.presets.length>=6;}
  $('#boot small').textContent=state.view==='focus-full'&&!state.focus?.withBook?'Timer pause / resume':state.readerSurface==='page'?'Next page':'Play / pause';
};
$('#preview-language').addEventListener('change',event=>{state.locale=event.target.value;render();});

// Sliders reuse the full-tile fill and text treatment from Study 12. Capture the
// pointer on the stable screen element so the tile may update without losing it.
let sliderDrag=null,gestureDrag=null,suppressGestureClick=false;
function cancelReaderGesture(){
  const drag=gestureDrag;gestureDrag=null;
  if(!drag)return;
  clearTimeout(drag.holdTimer);
  if(drag.holding)pauseReading();
  if(screen.hasPointerCapture(drag.pointerId))screen.releasePointerCapture(drag.pointerId);
}
function setSliderValue(el,value){
  const min=Number(el.getAttribute('aria-valuemin')),max=Number(el.getAttribute('aria-valuemax')),step=Number(el.dataset.step);
  value=Math.max(min,Math.min(max,Math.round((value-min)/step)*step+min));
  const key=el.dataset.slide;if(key==='bookPosition'){state.browse.candidate=Math.round(value/1000*(words.length-1));state.browse.atChapter=false;updateBrowse();}else state[key]=value;
  el.setAttribute('aria-valuenow',value);el.style.setProperty('--fill',(value-min)/(max-min)*100+'%');
  el.querySelectorAll('output').forEach(output=>output.textContent=key==='bookPosition'?Math.round(value/10)+'%':value+el.dataset.unit);
  if(state.view==='appearance'){const preview=screen.querySelector('.editor-preview');preview.innerHTML=sample14(state.editTab==='onScreen');placePhantomWords();}
}
function slideAt(event){const el=sliderDrag.el,rect=el.getBoundingClientRect();const fraction=el.classList.contains('vertical')?1-(event.clientY-rect.top)/rect.height:(event.clientX-rect.left)/rect.width;setSliderValue(el,Number(el.getAttribute('aria-valuemin'))+fraction*(Number(el.getAttribute('aria-valuemax'))-Number(el.getAttribute('aria-valuemin'))));}
screen.addEventListener('pointerdown',event=>{
  if(!event.isPrimary||event.button!==0)return;
  suppressGestureClick=false;
  const el=event.target.closest('[data-slide]');
  if(el){sliderDrag={el,pointerId:event.pointerId,original:Number(el.getAttribute('aria-valuenow'))};screen.setPointerCapture(event.pointerId);el.dataset.active='true';slideAt(event);event.preventDefault();return;}
  if(state.view==='reader'&&state.readerSurface==='playback'&&event.target.closest('.word-zone')){
    const drag={x:event.clientX,y:event.clientY,pointerId:event.pointerId,moved:false,holding:false,wasPlaying:state.playing};
    gestureDrag=drag;screen.setPointerCapture(event.pointerId);
    if(state.textAction==='hold'&&!state.playing)drag.holdTimer=setTimeout(()=>{
      if(gestureDrag!==drag||drag.moved||state.view!=='reader')return;
      drag.holding=true;state.playing=true;
      render({keepReaderPointer:true});queueWord();
    },350);
  }
});
screen.addEventListener('pointermove',event=>{
  if(sliderDrag&&event.pointerId===sliderDrag.pointerId){slideAt(event);return;}
  if(gestureDrag&&event.pointerId===gestureDrag.pointerId){
    const drag=gestureDrag,scale=screen.getBoundingClientRect().width/640;
    drag.moved ||= Math.hypot(event.clientX-drag.x,event.clientY-drag.y)>12*scale;
    if(drag.moved){
      clearTimeout(drag.holdTimer);
      if(drag.holding){drag.holding=false;pauseReading();render({keepReaderPointer:true});}
    }
  }
});
screen.addEventListener('pointerup',event=>{
  if(sliderDrag&&event.pointerId===sliderDrag.pointerId){slideAt(event);sliderDrag.el.dataset.active='false';sliderDrag=null;screen.releasePointerCapture(event.pointerId);return;}
  if(gestureDrag&&event.pointerId===gestureDrag.pointerId){const g=gestureDrag;gestureDrag=null;clearTimeout(g.holdTimer);screen.releasePointerCapture(event.pointerId);const scale=screen.getBoundingClientRect().width/640,dx=event.clientX-g.x,dy=event.clientY-g.y;
    suppressGestureClick=true;
    if(state.interfaceStyle==='gestures'&&dy < -30*scale&&Math.abs(dy)>Math.abs(dx)*1.3)navigate('gesture-tray');
    else if(g.holding){pauseReading();render();}
    else if(!g.moved&&(state.textAction==='tap'||g.wasPlaying))toggleReading();
  }
});
screen.addEventListener('pointercancel',()=>{
  if(sliderDrag){setSliderValue(sliderDrag.el,sliderDrag.original);sliderDrag.el.dataset.active='false';sliderDrag=null;}
  const held=gestureDrag?.holding;cancelReaderGesture();suppressGestureClick=true;
  if(held)render();
});
screen.addEventListener('lostpointercapture',event=>{
  if(gestureDrag?.pointerId===event.pointerId){cancelReaderGesture();render();}
});
window.addEventListener('blur',()=>{if(gestureDrag){cancelReaderGesture();render();}});
document.addEventListener('visibilitychange',()=>{if(document.hidden&&gestureDrag){cancelReaderGesture();render();}});
screen.addEventListener('contextmenu',event=>{if(event.target.closest('.word-zone'))event.preventDefault();});
screen.addEventListener('click',event=>{const gestureClick=suppressGestureClick&&event.detail>0;suppressGestureClick=false;if(gestureClick||event.target.closest('[data-slide]')){event.preventDefault();event.stopPropagation();}},true);
screen.addEventListener('keydown',event=>{const el=event.target.closest('[data-slide]');if(!el)return;const step=Number(el.dataset.step),value=Number(el.getAttribute('aria-valuenow'));let next;
  if(['ArrowRight','ArrowUp'].includes(event.key))next=value+step;else if(['ArrowLeft','ArrowDown'].includes(event.key))next=value-step;else if(event.key==='Home')next=Number(el.getAttribute('aria-valuemin'));else if(event.key==='End')next=Number(el.getAttribute('aria-valuemax'));else return;event.preventDefault();event.stopPropagation();setSliderValue(el,next);
});
cancelLoading();navigate('menu');
document.fonts.ready.then(()=>{chunkPlans.clear();render();fitScreen();});
