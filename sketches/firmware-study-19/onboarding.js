'use strict';

// The tour uses an isolated sample, never the current book or its progress.
Object.assign(uiCopy,{
  help:['Help','Hilfe','Ajutor'],helpHint:['Quick guide & introduction','Kurzanleitung & Einführung','Ghid rapid și introducere'],
  welcome:['Welcome to Nano','Willkommen bei Nano','Bun venit la Nano'],
  welcomeTitle:['A little practice.\nThen your book.','Kurz ausprobieren.\nDann dein Buch.','Un pic de practică.\nApoi cartea ta.'],
  welcomeBody:['Try reading and finding the controls. It only takes a moment.','Lerne das Lesen und die Steuerung kennen. Es dauert nur einen Moment.','Încearcă lectura și descoperă comenzile. Durează doar un moment.'],
  showMe:['Show me','Los geht’s','Arată-mi'],skipTour:['Skip tour','Überspringen','Omite turul'],
  next:['Next','Weiter','Continuă'],tourBack:['Back','Zurück','Înapoi'],
  hardwareButtons:['Hardware buttons','Gerätetasten','Butoanele dispozitivului'],
  swipeWord:['Read','Lesen','Citește'],
  practice:['Try reading','Lesen ausprobieren','Încearcă lectura'],
  tryTap:['Tap the word.','Tippe auf das Wort.','Atinge cuvântul.'],
  tryHold:['Hold the word.','Halte das Wort gedrückt.','Ține cuvântul apăsat.'],
  tryTapBody:['A tap starts reading. Try it on this short sample.','Ein Tippen startet das Lesen. Probiere es mit diesem kurzen Text.','O atingere pornește lectura. Încearcă pe acest text scurt.'],
  tryHoldBody:['Keep your finger on the word to read this short sample.','Lass deinen Finger auf dem Wort, um diesen kurzen Text zu lesen.','Ține degetul pe cuvânt pentru a citi acest text scurt.'],
  pauseTap:['Tap again to pause.','Tippe erneut für Pause.','Atinge din nou pentru pauză.'],
  pauseHold:['Release to pause.','Loslassen für Pause.','Eliberează pentru pauză.'],
  sampleRunning:['You control when the words move.','Du bestimmst, wann die Wörter wechseln.','Tu decizi când se schimbă cuvintele.'],
  pausedTitle:['That’s it.','Genau so.','Așa, exact.'],
  pausedBody:['Reading is paused. You can practise again, or continue.','Lesen pausiert. Probiere es noch einmal oder gehe weiter.','Lectura e oprită. Poți exersa din nou sau continua.'],
  demoWords:['A little time to read.','Zeit für dein Buch.','Un pic de timp pentru citit.'],
  swipeTitle:['Swipe up.','Wische nach oben.','Glisează în sus.'],
  swipeBody:['Start on the word and move upward to open your reading controls.','Wische vom Wort nach oben, um die Lesesteuerung zu öffnen.','Pornește de pe cuvânt și glisează în sus pentru comenzi.'],
  swipeAccessible:['Swipe up to open reading controls','Nach oben wischen für die Lesesteuerung','Glisează în sus pentru comenzile de lectură'],
  keyboardAlternative:['Open controls','Steuerung öffnen','Deschide comenzile'],
  controlsFound:['Your reading controls','Deine Lesesteuerung','Comenzile de lectură'],
  practiceSpeed:['Try changing the speed','Probiere ein anderes Tempo','Încearcă să schimbi viteza'],
  menuTip:['Your books and settings.','Deine Bücher und Einstellungen.','Cărțile și setările tale.'],
  pageTip:['Scroll and find a passage.','Blättern und Textstellen finden.','Răsfoiește și caută un pasaj.'],
  ready:['Ready for your book.','Bereit für dein Buch.','Ești gata de lectură.'],
  readyBody:['The hints step aside now. Your gestures stay the same.','Die Hinweise verschwinden jetzt. Die Gesten bleiben gleich.','Indicațiile dispar acum. Gesturile rămân aceleași.'],
  replayLocation:['Find this again in Settings → Help.','Erneut öffnen: Einstellungen → Hilfe.','Revino aici din Setări → Ajutor.'],
  openBook:['Open book','Buch öffnen','Deschide cartea'],
  bootHelp:['Start / pause reading','Lesen starten / pausieren','Pornește / oprește lectura'],
  powerHelp:['Open Menu','Menü öffnen','Deschide meniul'],
  resetHelp:['Restart the device','Gerät neu starten','Repornește dispozitivul'],
  replayIntro:['Replay introduction','Einführung wiederholen','Reia introducerea'],
  touchHelpTap:['Tap the text to read or pause.','Tippe auf den Text zum Lesen oder Pausieren.','Atinge textul pentru lectură sau pauză.'],
  touchHelpHold:['Hold the text to read. Release to pause.','Halte den Text zum Lesen. Loslassen pausiert.','Ține textul apăsat pentru lectură. Eliberează pentru pauză.'],
  swipeHelp:['Swipe up for speed, Menu and Page.','Nach oben wischen für Tempo, Menü und Seite.','Glisează în sus pentru viteză, meniu și pagină.'],
});
Object.assign(uiCopy,lessonCopy,tuningCopy);
const INTRO_KEY='rsvp-nano.sd-intro-preview.v1';
const PREFS_KEY='rsvp-nano.sd-intro-preferences.v1';
let introSeen=null,storageWorks=true,tour=null,tourPointer=null,tourInterval=null,tourClick=false;
try{
  const saved=JSON.parse(localStorage.getItem(INTRO_KEY)||'null');
  if(saved&&['completed','skipped'].includes(saved.status))introSeen=saved;
  const prefs=JSON.parse(localStorage.getItem(PREFS_KEY)||'{}');
  if(['tap','hold'].includes(prefs.textAction))state.textAction=prefs.textAction;
  if(['en','de','ro'].includes(prefs.locale))state.locale=prefs.locale;
  for(const key of ['wpm','longDelay','complexDelay','punctuationDelay'])if(Number.isInteger(prefs[key])&&prefs[key]>=(key==='wpm'?100:0)&&prefs[key]<=1000)state[key]=prefs[key];
}catch{storageWorks=false;}
function rememberPreferences(){
  try{localStorage.setItem(PREFS_KEY,JSON.stringify({locale:state.locale,textAction:state.textAction,wpm:state.wpm,longDelay:state.longDelay,complexDelay:state.complexDelay,punctuationDelay:state.punctuationDelay}));}catch{storageWorks=false;}
}
function saveIntro(status){
  introSeen={status};
  try{localStorage.setItem(INTRO_KEY,JSON.stringify(introSeen));}catch{storageWorks=false;}
  rememberPreferences();updateIntroStatus();
}
function updateIntroStatus(){
  $('#onboarding-status').textContent=!storageWorks?'Browser storage is unavailable. The introduction is remembered for this open page only.':
    !introSeen?'First launch: the introduction has not been completed.':
    `Introduction ${introSeen.status}. The next launch opens Menu. Replay is always available in Help.`;
}
function clearTourPointer(){
  const pointer=tourPointer;tourPointer=null;
  if(!pointer)return;
  clearTimeout(pointer.timer);
  if(screen.hasPointerCapture(pointer.id))screen.releasePointerCapture(pointer.id);
}
function stopTourPlayback(){stopTuningSample();clearInterval(tourInterval);tourInterval=null;if(tour)tour.playing=false;}
function beginIntroduction(){
  stopTourPlayback();clearTourPointer();pauseReading();
  tour={step:0,playing:false,started:false,paused:false,index:0,controls:false,wpm:state.wpm,longDelay:state.longDelay,complexDelay:state.complexDelay,punctuationDelay:state.punctuationDelay,brightness:70,tuningIndex:0,tuningPlaying:false,tuningMode:'paced',tuningDone:false,tuningTouched:null,menuTopic:'library',settingTopic:'reading',pageTool:'browse',focusKind:'reading',selectedIndex:null,selectedWord:''};
  navigate('onboarding');
}
function exitIntroduction(status){
  if(status)saveIntro(status);
  stopTourPlayback();clearTourPointer();tour=null;
  state.readerSurface='playback';navigate(status==='completed'?'reader':'menu');
}
function tourHeader(){
  const last=tour.step===10,canNext=tour.step===1?tour.paused&&!tour.playing:tour.step===2?tour.controls:true;
  return `<div class="intro-header">${tour.step?button('‹','intro-back','tour-back',`aria-label="${t('tourBack')}"`):'<span class="intro-brand">'+$('#project-logo-template').innerHTML+'</span>'}<span class="intro-step">${tour.step?`<span class="intro-dots" aria-hidden="true">${[1,2,3,4,5,6,7,8,9,10].map(n=>`<i class="${n<=tour.step?'filled':''}"></i>`).join('')}</span>${tour.step} / 10 · ${t(['welcome','practice','trayTitle','findSpeed','findPacing','essentialMenu','pageLesson','essentialSettings','focus','physicalButtons','addBooks'][tour.step])}`:t('welcome')}</span>${button(t(tour.step===4?'keepCurrent':'skipTour'),tour.step===4?'intro-next':'skip-intro','tour-skip')}${tour.step?button(t(last?'openBook':tour.step===4?'useSettings':'next')+' ↗',last?'finish-intro':tour.step===4?'apply-tuning':'intro-next','primary tour-next',canNext?'':'disabled'):''}</div>`;
}
function practiceCopy(){
  return {title:t(tour.playing?(state.textAction==='hold'?'pauseHold':'pauseTap'):tour.paused?'pausedTitle':state.textAction==='hold'?'tryHold':'tryTap'),
    body:t(tour.playing?'sampleRunning':tour.paused?'pausedBody':state.textAction==='hold'?'tryHoldBody':'tryTapBody')};
}
function tourWord(){return t('demoWords').split(' ')[tour.index%t('demoWords').split(' ').length];}
function onboardingView(){
  if(!tour)return '';
  const header=tourHeader();
  if(tour.step===0)return header+`<div class="intro-welcome"><div class="intro-copy15"><h2>${t('welcomeTitle').replace('\n','<br>')}</h2><p>${t('welcomeBody')}</p></div><div class="intro-start"><div class="intro-languages" aria-label="${t('language')}">${Object.entries(languageNames).map(([code,label])=>button(label,'intro-language',state.locale===code?'selected':'',`data-locale="${code}" aria-pressed="${state.locale===code}"`)).join('')}</div>${button(t('showMe')+' ↗','intro-next','primary')}</div></div>`;
  if(tour.step>=3)return header+detailedLesson(tour.step);
  if(tour.step===2&&tour.controls)return quickControlsView({practice:true});
  const swipe=tour.step===2,copy=swipe?{title:t('swipeTitle'),body:t('swipeBody')}:practiceCopy();
  return header+`<div class="intro-lesson"><div class="intro-copy15"><h2 data-practice-title>${copy.title}</h2><p data-practice-body>${copy.body}</p></div><div class="intro-practice ${swipe?'swipe-practice':''}" ${swipe?'role="region"':'role="button"'} tabindex="0" aria-label="${swipe?t('swipeAccessible'):readingCue(false)}"><div class="tour-word">${wordMarkup(swipe?t('swipeWord'):tourWord(),true)}</div>${swipe?'<span class="swipe-mark" aria-hidden="true">↑</span>':''}</div>${swipe?button(t('keyboardAlternative'),'intro-open-controls','keyboard-alternative'):''}</div>`;
}
function helpView(){return h14(t('help'))+`<div class="help15"><div class="help-gestures"><div><strong>${t(state.textAction==='hold'?'hold':'tap')}</strong><span>${t(state.textAction==='hold'?'touchHelpHold':'touchHelpTap')}</span></div><div><strong>↑</strong><span>${t('swipeHelp')}</span></div></div><div class="help-replay">${go(t('locateButtons')+' ↗','button-map','secondary')}${button(t('replayIntro')+' ↗','replay-intro','secondary')}</div></div>`;}
window.studyViews['button-map']=buttonMapView;
window.studyViews.onboarding=onboardingView;
window.studyViews.help=helpView;
Object.assign(descriptions,{
  'button-map':['04','Physical buttons','Match the picture to your device.','The diagram uses your confirmed layout: buttons along the top right, USB on the right edge. Highlight a button to identify its action without relying on printed labels.','Left button restarts the device; Middle button opens Menu; Right button starts or pauses reading. The diagram only highlights buttons; it never resets the preview.'],
  onboarding:['00','First launch','A little practice. Then your book.','The introduction teaches reading, quick controls, speed and pacing, then explains Menu, page navigation, Settings, Focus and the physical button positions. It can be skipped, and returns only when you ask for it in Help.','Try the actions on the display. Complete or skip, then use Simulate restart to check the next launch.'],
  help:['04','Help','A reminder when you need one.','Keep the everyday reader free of instructions. Help repeats the two gestures and the hardware shortcuts, with an introduction you can replay at any time.','Select Hold under Reading, then replay the introduction to try its matching lesson.'],
});
const beforeIntro={render,navigate,action,toggleReading};
render=function(options){cancelTourSlider();cancelTuningSlider();beforeIntro.render(options);updateIntroStatus();};
navigate=function(view,options){
  if(view==='onboarding'&&!tour){beginIntroduction();return;}
  if(state.view==='onboarding'&&view!=='onboarding'){stopTourPlayback();clearTourPointer();tour=null;}
  beforeIntro.navigate(view,options);
};
function updatePractice(){
  if(state.view!=='onboarding'||tour?.step!==1)return;
  const copy=practiceCopy();
  screen.querySelector('[data-practice-title]').textContent=copy.title;
  screen.querySelector('[data-practice-body]').textContent=copy.body;
  screen.querySelector('.tour-word').innerHTML=wordMarkup(tourWord(),true);
  screen.querySelector('[data-action="intro-next"]').disabled=!tour.paused||tour.playing;
  screen.querySelector('.intro-practice').setAttribute('aria-label',tour.playing?t(state.textAction==='hold'?'releasePause':'tapPause'):readingCue(false));
}
function startPractice(){
  if(!tour||tour.step!==1||tour.playing)return;
  tour.started=true;tour.playing=true;updatePractice();
  tourInterval=setInterval(()=>{if(tour?.playing){tour.index++;updatePractice();}},400);
}
function pausePractice(){if(!tour)return;const wasPlaying=tour.playing;stopTourPlayback();if(wasPlaying)tour.paused=true;updatePractice();}
function togglePractice(){if(tour?.playing)pausePractice();else startPractice();}
function revealPracticeControls(){if(tour?.step!==2)return;clearTourPointer();tour.controls=true;render();}
toggleReading=function(){if(state.view==='onboarding'){if(tour?.step===1)togglePractice();else if(tour?.step===3||tour?.step===4)toggleTuningSample(tour.step===3?'even':'paced');return;}beforeIntro.toggleReading();};
action=function(name,element){
  if(name==='help'){navigate('help');return;}
  if(name==='replay-intro'){beginIntroduction();return;}
  if(name==='simulate-restart'){stopTourPlayback();clearTourPointer();tour=null;pauseReading();if(introSeen)navigate('menu');else beginIntroduction();return;}
  if(name==='fresh-install'){
    introSeen=null;try{localStorage.removeItem(INTRO_KEY);localStorage.removeItem(PREFS_KEY);}catch{storageWorks=false;}
    state.locale='en';state.textAction='tap';state.wpm=300;state.longDelay=state.complexDelay=state.punctuationDelay=200;beginIntroduction();return;
  }
  if(name==='skip-intro'){exitIntroduction('skipped');return;}
  if(name==='finish-intro'){exitIntroduction('completed');return;}
  if(name==='intro-next'||name==='intro-back'){
    if(!tour)return;stopTourPlayback();clearTourPointer();tour.tuningIndex=0;tour.tuningDone=false;
    tour.step=Math.max(0,Math.min(10,tour.step+(name==='intro-next'?1:-1)));render();return;
  }
  if(name==='intro-menu'||name==='intro-page'){tour.step=name==='intro-menu'?5:6;render();return;}
  if(handleTuningAction(name,element))return;
  if(name==='intro-topic'){tour[element.dataset.kind==='menu'?'menuTopic':'settingTopic']=element.dataset.topic;render();return;}
  if(name==='intro-page-tool'){tour.pageTool=element.dataset.tool;render();return;}
  if(name==='intro-focus-kind'){tour.focusKind=element.dataset.kind;render();return;}
  if(name==='locate-physical'){highlightedButton=Number(element.dataset.physical);render();return;}
  if(name==='intro-language'){state.locale=element.dataset.locale;rememberPreferences();render();return;}
  if(name==='intro-slower'||name==='intro-faster'){tour.wpm=Math.max(100,Math.min(1000,tour.wpm+(name==='intro-slower'?-10:10)));render();return;}
  if(name==='intro-open-controls'){revealPracticeControls();return;}
  beforeIntro.action(name,element);
  if(['text-action','choose-language','slower','faster','reset-pacing'].includes(name))rememberPreferences();
};
$('#preview-language').addEventListener('change',()=>{stopTourPlayback();clearTourPointer();rememberPreferences();if(tour)render();});
screen.addEventListener('pointerdown',event=>{
  if(event.isPrimary)tourClick=false;
  if(state.view!=='onboarding'||!event.target.closest('.intro-practice')||!event.isPrimary||event.button!==0)return;
  const pointer={id:event.pointerId,x:event.clientX,y:event.clientY,moved:false,started:false,wasPlaying:tour.playing};
  tourPointer=pointer;tourClick=false;screen.setPointerCapture(pointer.id);
  if(tour.step===1&&state.textAction==='hold'&&!tour.playing)pointer.timer=setTimeout(()=>{
    if(tourPointer===pointer&&!pointer.moved){pointer.started=true;startPractice();}
  },350);
});
screen.addEventListener('pointermove',event=>{
  const pointer=tourPointer;if(!pointer||pointer.id!==event.pointerId)return;
  const scale=screen.getBoundingClientRect().width/640;
  pointer.moved ||= Math.hypot(event.clientX-pointer.x,event.clientY-pointer.y)>12*scale;
  if(pointer.moved){clearTimeout(pointer.timer);if(pointer.started){pointer.started=false;pausePractice();}}
});
screen.addEventListener('pointerup',event=>{
  const pointer=tourPointer;if(!pointer||pointer.id!==event.pointerId)return;
  clearTourPointer();tourClick=true;
  const scale=screen.getBoundingClientRect().width/640,dx=event.clientX-pointer.x,dy=event.clientY-pointer.y;
  if(tour?.step===2&&dy<-30*scale&&Math.abs(dy)>Math.abs(dx)*1.3){revealPracticeControls();return;}
  if(tour?.step===1){if(pointer.started)pausePractice();else if(!pointer.moved&&(state.textAction==='tap'||pointer.wasPlaying))togglePractice();}
});
screen.addEventListener('click',event=>{
  if(state.view==='onboarding'&&(tourClick||event.target.closest('.intro-practice'))){tourClick=false;event.preventDefault();event.stopPropagation();}
},true);
screen.addEventListener('keydown',event=>{
  if(!event.target.closest('.intro-practice'))return;
  if(event.key===' '||event.key==='Enter'||event.key==='ArrowUp'){
    event.preventDefault();event.stopPropagation();
    if(tour?.step===2)revealPracticeControls();else togglePractice();
  }
});
function interruptPractice(){if(tour){clearTourPointer();pausePractice();updateTuningSample();}}
screen.addEventListener('pointercancel',interruptPractice);
screen.addEventListener('lostpointercapture',event=>{if(tourPointer?.id===event.pointerId)interruptPractice();});
window.addEventListener('blur',interruptPractice);
document.addEventListener('visibilitychange',()=>{if(document.hidden)interruptPractice();});
screen.addEventListener('contextmenu',event=>{if(event.target.closest('.intro-practice'))event.preventDefault();});
updateIntroStatus();
if(introSeen)navigate('menu');else beginIntroduction();
