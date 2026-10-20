'use strict';

// Practice uses draft values. Only Use settings copies them to the reader.
const tuningCopy={
  findSpeed:['Find your speed','Finde dein Tempo','Găsește-ți viteza'],
  findPacing:['Add pauses','Pausen hinzufügen','Adaugă pauze'],
  keepCurrent:['Keep current','Bisherige behalten','Păstrează valorile'],
  useSettings:['Use settings','Übernehmen','Aplică setările'],
  baseSpeed:['Base speed · words per minute','Grundtempo · Wörter pro Minute','Viteza de bază · cuvinte pe minut'],
  speedAdvice:['Losing the meaning? Go slower. Feels easy? Try a little faster. Replay until it feels comfortable.','Verlierst du den Sinn? Langsamer. Fällt es leicht? Etwas schneller. Probiere, bis es angenehm ist.','Pierzi sensul? Mai încet. E ușor? Încearcă puțin mai repede. Repetă până te simți confortabil.'],
  baseOnly:['No extra pauses yet','Noch ohne Zusatzpausen','Deocamdată fără pauze suplimentare'],
  playSample:['Play sample','Text abspielen','Citește exemplul'],
  replaySample:['Replay sample','Text wiederholen','Repetă exemplul'],
  stopSample:['Pause sample','Text pausieren','Oprește exemplul'],
  withPauses:['With pauses','Mit Pausen','Cu pauze'],
  withoutPauses:['Without pauses','Ohne Pausen','Fără pauze'],
  pacingAdvice:['Same WPM. Add pauses where words feel rushed, then compare both versions.','Gleiches Tempo. Ergänze Pausen, wo Wörter zu schnell sind. Vergleiche beide Varianten.','Aceeași viteză. Adaugă pauze unde cuvintele trec prea repede, apoi compară variantele.'],
  longDelayAdvice:['Long words stay longer. Use only as much extra time as you need.','Lange Wörter bleiben länger. Gib ihnen nur so viel Zusatzzeit wie nötig.','Cuvintele lungi rămân mai mult. Adaugă doar timpul de care ai nevoie.'],
  complexDelayAdvice:['More time for harder words. Too much can interrupt the flow.','Mehr Zeit für schwere Wörter. Zu viel kann den Lesefluss unterbrechen.','Mai mult timp pentru cuvinte dificile. Prea mult poate întrerupe ritmul.'],
  punctuationDelayAdvice:['Pauses at punctuation separate ideas. Start small and compare.','Pausen an Satzzeichen trennen Gedanken. Fang klein an und vergleiche.','Pauzele la punctuație separă ideile. Începe cu puțin și compară.'],
  sampleFinished:['Sample finished · replay to compare','Text zu Ende · erneut vergleichen','Exemplu terminat · repetă și compară'],
  tuningSample:[
    'The morning was quiet. I opened the window and listened to the rain. Somewhere outside, a bicycle rolled past. The rhythm was unpredictable, but listening carefully made it familiar. I took a breath, turned the page, and kept reading.',
    'Der Morgen war still. Ich öffnete das Fenster und hörte dem Regen zu. Draußen fuhr ein Fahrrad vorbei. Der Rhythmus war unregelmäßig, doch mit Aufmerksamkeit wurde er vertraut. Ich atmete durch, blätterte um und las in aller Ruhe weiter.',
    'Dimineața era liniștită. Am deschis fereastra și am ascultat ploaia. Undeva afară, a trecut o bicicletă. Ritmul era imprevizibil, dar ascultând cu atenție am început să îl înțeleg. Am respirat, am întors pagina și am continuat să citesc în liniște.'
  ]
};
const tuningKeys=['longDelay','complexDelay','punctuationDelay'];
let tuningTimer=null,tuningDrag=null;
function tuningWords(){return t('tuningSample').split(/\s+/);}
function tuningWord(){return tuningWords()[tour.tuningIndex]||tuningWords()[0];}
function tuningHint(){return t(tour.tuningTouched?tour.tuningTouched+'Advice':'pacingAdvice');}
function tuningDuration(word,mode){return ChunkReader.duration([word],tour.wpm,mode==='even'?{}:tour);}
function tuningSlider(key,label){
  const content=`<span>${label}</span><output>${tour[key]} ms</output>`;
  return `<div class="full-slider tuning-slider" role="slider" tabindex="0" data-tuning="${key}" aria-label="${label}" aria-valuemin="0" aria-valuemax="1000" aria-valuenow="${tour[key]}" aria-valuetext="${tour[key]} ms" aria-orientation="horizontal" style="--fill:${tour[key]/10}%"><div class="slider-fill"></div><div class="slider-content">${content}</div><div class="slider-content filled" aria-hidden="true">${content}</div></div>`;
}
function tuningPlayButton(mode){
  const active=tour.tuningPlaying&&tour.tuningMode===mode;
  const label=tour.step===3?t(active?'stopSample':tour.tuningDone?'replaySample':'playSample'):t(mode==='even'?'withoutPauses':'withPauses');
  return button(`<span aria-hidden="true">${active?'Ⅱ':'▷'}</span> ${label}`,'tuning-play-'+mode,tour.step===3?'primary':active?'selected':'secondary',`data-tuning-play="${mode}" aria-pressed="${active}"`);
}
function tuningLesson(step){
  const sample=`<div class="tuning-word">${wordMarkup(tuningWord(),true)}</div>`;
  if(step===3)return `<div class="tuning-lesson speed-lesson"><div class="tuning-adjust"><span class="tuning-label">${t('baseSpeed')}</span><div class="tuning-wpm">${button('−','tuning-slower','',`aria-label="${t('slower')}" ${tour.wpm<=100?'disabled':''}`)}<output>${tour.wpm}<small> WPM</small></output>${button('+','tuning-faster','',`aria-label="${t('faster')}" ${tour.wpm>=1000?'disabled':''}`)}</div><p>${t('speedAdvice')}</p></div><div class="tuning-player">${sample}<span class="tuning-caption">${t(tour.tuningDone?'sampleFinished':'baseOnly')}</span><div class="tuning-actions">${tuningPlayButton('even')}</div></div></div>`;
  return `<div class="tuning-lesson pacing-lesson"><div class="tuning-sliders">${tuningSlider('longDelay',t('longWords'))}${tuningSlider('complexDelay',t('complexWords'))}${tuningSlider('punctuationDelay',t('punctuation'))}</div><div class="tuning-player">${sample}<p class="tuning-hint">${tuningHint()}</p><div class="tuning-actions">${tuningPlayButton('paced')}${tuningPlayButton('even')}</div></div></div>`;
}
function stopTuningSample(){clearTimeout(tuningTimer);tuningTimer=null;if(tour)tour.tuningPlaying=false;}
function updateTuningSample(){
  if(state.view!=='onboarding'||!tour||![3,4].includes(tour.step))return;
  const word=screen.querySelector('.tuning-word');if(!word)return;
  word.innerHTML=wordMarkup(tuningWord(),true);
  const caption=screen.querySelector('.tuning-caption');if(caption)caption.textContent=t(tour.tuningDone?'sampleFinished':'baseOnly');
  const hint=screen.querySelector('.tuning-hint');if(hint)hint.textContent=tuningHint();
  screen.querySelectorAll('[data-tuning-play]').forEach(el=>{
    const mode=el.dataset.tuningPlay;
    // Keep focused buttons in place while the sample plays.
    const template=document.createElement('template');template.innerHTML=tuningPlayButton(mode);
    const next=template.content.firstElementChild;
    el.innerHTML=next.innerHTML;el.className=next.className;el.setAttribute('aria-pressed',next.getAttribute('aria-pressed'));
  });
}
function queueTuningWord(){
  clearTimeout(tuningTimer);
  if(!tour?.tuningPlaying)return;
  tuningTimer=setTimeout(()=>{
    if(state.view!=='onboarding'||!tour?.tuningPlaying)return;
    if(tour.tuningIndex+1>=tuningWords().length){stopTuningSample();tour.tuningDone=true;updateTuningSample();return;}
    tour.tuningIndex++;updateTuningSample();queueTuningWord();
  },tuningDuration(tuningWord(),tour.tuningMode));
}
function startTuningSample(mode){
  stopTuningSample();tour.tuningMode=mode;tour.tuningIndex=0;tour.tuningDone=false;tour.tuningPlaying=true;
  updateTuningSample();queueTuningWord();
}
function toggleTuningSample(mode){
  if(tour?.tuningPlaying&&tour.tuningMode===mode){stopTuningSample();updateTuningSample();}
  else if(tour&&[3,4].includes(tour.step))startTuningSample(mode);
}
function handleTuningAction(name){
  if(name==='try-tuning'){beginIntroduction();tour.step=3;render();return true;}
  if(!name.startsWith('tuning-')&&name!=='apply-tuning')return false;
  if(!tour||![3,4].includes(tour.step))return true;
  if(name==='tuning-slower'||name==='tuning-faster'){
    const playing=tour.tuningPlaying,mode=tour.tuningMode;
    stopTuningSample();tour.wpm=Math.max(100,Math.min(1000,tour.wpm+(name==='tuning-slower'?-10:10)));render();
    if(playing)startTuningSample(mode);
  }else if(name==='tuning-play-even'||name==='tuning-play-paced')toggleTuningSample(name.endsWith('even')?'even':'paced');
  else if(name==='apply-tuning'){
    stopTourPlayback();for(const key of ['wpm',...tuningKeys])state[key]=tour[key];
    rememberPreferences();tour.step=5;render();
  }
  return true;
}
function setTuningValue(el,value){
  if(!tour)return;
  const key=el.dataset.tuning;
  tour[key]=Math.max(0,Math.min(1000,Math.round(value/50)*50));tour.tuningTouched=key;
  el.setAttribute('aria-valuenow',tour[key]);el.setAttribute('aria-valuetext',tour[key]+' ms');el.style.setProperty('--fill',tour[key]/10+'%');
  el.querySelectorAll('output').forEach(output=>output.textContent=tour[key]+' ms');
  updateTuningSample();
}
function cancelTuningSlider(restore=false){
  const drag=tuningDrag;tuningDrag=null;if(!drag)return;
  if(restore)setTuningValue(drag.el,drag.original);
  drag.el.dataset.active='false';
  if(screen.hasPointerCapture(drag.id))screen.releasePointerCapture(drag.id);
}
function slideTuning(event){const rect=tuningDrag.el.getBoundingClientRect();setTuningValue(tuningDrag.el,(event.clientX-rect.left)/rect.width*1000);}
screen.addEventListener('pointerdown',event=>{
  const el=event.target.closest('[data-tuning]');
  if(!el||!event.isPrimary||event.button!==0||!tour)return;
  stopTuningSample();tuningDrag={el,id:event.pointerId,original:tour[el.dataset.tuning]};
  screen.setPointerCapture(event.pointerId);el.dataset.active='true';slideTuning(event);event.preventDefault();
});
screen.addEventListener('pointermove',event=>{if(tuningDrag?.id===event.pointerId)slideTuning(event);});
screen.addEventListener('pointerup',event=>{if(tuningDrag?.id===event.pointerId){slideTuning(event);cancelTuningSlider();}});
screen.addEventListener('pointercancel',()=>cancelTuningSlider(true));
screen.addEventListener('lostpointercapture',event=>{if(tuningDrag?.id===event.pointerId)cancelTuningSlider(true);});
window.addEventListener('blur',()=>cancelTuningSlider(true));
document.addEventListener('visibilitychange',()=>{if(document.hidden)cancelTuningSlider(true);});
screen.addEventListener('keydown',event=>{
  const el=event.target.closest('[data-tuning]');if(!el||!tour)return;
  const value=tour[el.dataset.tuning];let next;
  if(['ArrowRight','ArrowUp'].includes(event.key))next=value+50;
  else if(['ArrowLeft','ArrowDown'].includes(event.key))next=value-50;
  else if(event.key==='Home')next=0;else if(event.key==='End')next=1000;else return;
  event.preventDefault();event.stopPropagation();stopTuningSample();setTuningValue(el,next);
});

// Changes later made in Reading settings should replace the saved practice choice.
const beforeTuningSlider=setSliderValue;
setSliderValue=function(el,value){beforeTuningSlider(el,value);if(tuningKeys.includes(el.dataset.slide))rememberPreferences();};
