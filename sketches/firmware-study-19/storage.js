'use strict';

Object.assign(uiCopy,{
  sdCheck:['SD card check','SD-Karte prüfen','Verifică microSD'],
  sdChecking:['Checking your card…','Karte wird geprüft…','Verificăm cardul…'],
  sdCheckingBody:['Opening your card, preparing folders and checking a small test file.','Karte öffnen, Ordner vorbereiten und eine kleine Testdatei prüfen.','Deschidem cardul, pregătim dosarele și verificăm un fișier de test.'],
  sdReady:['Your card is ready.','Deine Karte ist bereit.','Cardul este pregătit.'],
  sdReadyBody:['Books, settings and your reading place can be saved. Let’s try reading.','Bücher, Einstellungen und Leseposition können gespeichert werden. Probiere das Lesen aus.','Cărțile, setările și locul din carte pot fi salvate. Hai să încercăm lectura.'],
  sdUnavailable:['Can’t open the card.','Karte nicht verfügbar.','Cardul nu se deschide.'],
  sdUnavailableBody:['Power off and reseat the microSD. Still failing? Check FAT32 on a computer.','Ausschalten und microSD neu einsetzen. Weiterhin ein Fehler? FAT32 am Computer prüfen.','Oprește Nano și reintrodu cardul. Tot nu merge? Verifică FAT32 pe un computer.'],
  sdFolders:['Folders aren’t ready.','Ordner nicht bereit.','Dosarele nu sunt gata.'],
  sdFoldersBody:['The card opens, but Nano can’t prepare its folders. Check free space and the card on a computer.','Die Karte lässt sich öffnen, aber die Ordner fehlen. Freien Platz und Karte am Computer prüfen.','Cardul se deschide, dar dosarele nu pot fi pregătite. Verifică spațiul liber și cardul pe computer.'],
  sdWrite:['Save check failed.','Speichertest fehlgeschlagen.','Testul de salvare a eșuat.'],
  sdWriteBody:['The test file could not be saved and read back correctly. Check free space or try another card.','Die Testdatei ließ sich nicht korrekt speichern und lesen. Freien Platz prüfen oder andere Karte testen.','Fișierul de test nu a fost salvat și citit corect. Verifică spațiul liber sau încearcă alt card.'],
  sdTimeout:['Check didn’t finish.','Prüfung nicht beendet.','Verificarea nu s-a încheiat.'],
  sdTimeoutBody:['Try again. If it still stalls, power off and reseat the card, or try another one.','Erneut versuchen. Hängt es wieder? Ausschalten, Karte neu einsetzen oder eine andere testen.','Încearcă din nou. Dacă se blochează, oprește Nano și reintrodu cardul sau încearcă altul.'],
  sdOpen:['Open card','Karte öffnen','Deschide cardul'],
  sdPrepare:['Prepare folders','Ordner vorbereiten','Pregătește dosarele'],
  sdSave:['Save & read back','Speichern & lesen','Salvează și citește'],
  sdPassed:['Passed','Bestanden','Reușit'],
  sdNotRun:['Not checked','Nicht geprüft','Neverificat'],
  sdWorking:['Checking…','Prüfung…','Se verifică…'],
  sdFailed:['Needs attention','Bitte prüfen','Necesită atenție'],
  sdIncomplete:['Not finished','Nicht beendet','Neîncheiat'],
  sdRetry:['Retry','Erneut prüfen','Reîncearcă'],
  sdContinue:['Continue','Weiter','Continuă'],
  sdDemo:['Try demo','Demo testen','Încearcă demo'],
  sdDemoNote:['Demo works; setup still needs a working card.','Demo möglich; für die Einrichtung fehlt eine nutzbare Karte.','Poți încerca demo; configurarea necesită un card funcțional.'],
});

// Browser-only scenarios. Actual firmware diagnostics must supply these outcomes.
let sdScenario='ready';
let sdCheck={status:'idle',stage:0,attempted:false,deferred:false};
let sdTimer=null,sdGeneration=0;
function cancelSdCheck(){
  clearTimeout(sdTimer);sdTimer=null;++sdGeneration;
  if(sdCheck.status==='checking')sdCheck.status='cancelled';
}
function startSdCheck(){
  cancelSdCheck();
  sdCheck={status:'checking',stage:0,attempted:true,deferred:false};
  const generation=sdGeneration,scenario=sdScenario;
  const advance=()=>{
    if(generation!==sdGeneration||state.view!=='onboarding'||!tour?.storageVisible)return;
    sdTimer=null;
    if(sdCheck.stage===0&&scenario==='unavailable')sdCheck.status='unavailable';
    else if(sdCheck.stage===1&&scenario==='folders')sdCheck.status='folders';
    else if(sdCheck.stage===2)sdCheck.status=scenario;
    else sdCheck.stage++;
    render();
    if(sdCheck.status==='checking')sdTimer=setTimeout(advance,scenario==='timeout'&&sdCheck.stage===2?2500:550);
  };
  sdTimer=setTimeout(advance,550);render();
}
function showSdCheck(retry=true){
  stopTourPlayback();clearTourPointer();
  tour.storageVisible=true;
  if(retry||sdCheck.status==='idle'||sdCheck.status==='cancelled')startSdCheck();else render();
}
function storageCheckView(){
  const busy=sdCheck.status==='checking',ready=sdCheck.status==='ready';
  const failure=!busy&&!ready;
  const copy=busy?'sdChecking':ready?'sdReady':({unavailable:'sdUnavailable',folders:'sdFolders',write:'sdWrite',timeout:'sdTimeout'}[sdCheck.status]||'sdTimeout');
  const rows=['sdOpen','sdPrepare','sdSave'].map((key,i)=>{
    const passed=ready||i<sdCheck.stage,active=busy&&i===sdCheck.stage,failed=failure&&i===sdCheck.stage;
    const label=passed?'sdPassed':active?'sdWorking':failed?(sdCheck.status==='timeout'?'sdIncomplete':'sdFailed'):'sdNotRun';
    const mark=passed?'✓':active?'':failed?'!':'–';
    return `<li class="${passed?'passed':active?'working':failed?'failed':'pending'}"><span class="sd-mark19" aria-hidden="true">${mark}</span><strong>${t(key)}</strong><span class="sd-row-status19">${t(label)}</span></li>`;
  }).join('');
  return `<div class="intro-header sd-header19">${button('‹','sd-back','tour-back',`aria-label="${t('tourBack')}"`)}<span class="intro-step">${t('sdCheck')}</span>${failure?button(t('sdDemo'),'sd-demo','tour-skip'):''}${button(t(ready?'sdContinue':busy?'sdWorking':'sdRetry'),ready?'sd-continue':'sd-retry','primary tour-next',busy?'disabled':'')}</div><div class="sd-content19"><div class="sd-copy19" role="status" aria-live="polite"><h2>${t(copy)}</h2><p>${t(copy+'Body')}</p>${failure?`<small>${t('sdDemoNote')}</small>`:''}</div><ol class="sd-checklist19" aria-label="${t('sdCheck')}">${rows}</ol></div>`;
}

const beforeStorage={beginIntroduction,exitIntroduction,navigate,action,onboardingView,updateIntroStatus,toggleReading};
beginIntroduction=function(){
  cancelSdCheck();sdCheck={status:'idle',stage:0,attempted:false,deferred:false};
  beforeStorage.beginIntroduction();
};
window.studyViews.onboarding=function(){return tour?.storageVisible?storageCheckView():beforeStorage.onboardingView();};
toggleReading=function(){if(tour?.storageVisible)return;beforeStorage.toggleReading();};
exitIntroduction=function(status){
  const incomplete=sdCheck.attempted&&sdCheck.status!=='ready';
  cancelSdCheck();
  beforeStorage.exitIntroduction(incomplete?undefined:status);
};
navigate=function(view,options){
  if(state.view==='onboarding'&&view!=='onboarding')cancelSdCheck();
  beforeStorage.navigate(view,options);
};
updateIntroStatus=function(){
  beforeStorage.updateIntroStatus();
  if(sdCheck.attempted&&sdCheck.status!=='ready'&&sdCheck.status!=='checking')$('#onboarding-status').textContent='Demo only: SD setup is unfinished. A first-time introduction will return on restart.';
};
action=function(name,element){
  if(name==='try-sd-check'){beginIntroduction();showSdCheck();return;}
  if(name==='sd-retry'){if(sdCheck.status!=='checking')startSdCheck();return;}
  if(name==='sd-back'){cancelSdCheck();tour.storageVisible=false;tour.step=0;render();return;}
  if(name==='sd-continue'){if(sdCheck.status!=='ready')return;tour.storageVisible=false;tour.step=1;render();return;}
  if(name==='sd-demo'){cancelSdCheck();sdCheck.deferred=true;tour.storageVisible=false;tour.step=1;render();return;}
  if(name==='intro-next'&&tour?.step===0&&!tour.storageVisible){showSdCheck();return;}
  if(name==='intro-back'&&tour?.step===1){showSdCheck(false);return;}
  // No ordinary tour action may advance past an active storage check.
  if(tour?.storageVisible&&['intro-next','finish-intro','skip-intro','apply-tuning'].includes(name))return;
  beforeStorage.action(name,element);
};
$('#sd-scenario').addEventListener('change',event=>{
  sdScenario=event.target.value;beginIntroduction();showSdCheck();
});
descriptions.onboarding[3]='First check that the card opens and can save data. Then learn the gestures, speed, pacing and menus, and scan the companion QR to add books. A failed check offers Retry or demo practice without marking setup complete.';
descriptions.onboarding[4]='Choose an SD scenario below the display to preview recovery. These outcomes are simulated; the HTML cannot test a real card.';
render();
