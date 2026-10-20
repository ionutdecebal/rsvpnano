'use strict';

Object.assign(uiCopy,{
  addBooks:['Add books','Bücher hinzufügen','Adaugă cărți'],
  scanBooks:['Scan to open RSVP Nano','Scannen, um RSVP Nano zu öffnen','Scanează pentru a deschide RSVP Nano'],
  uploadWifiOne:['Nano: Menu → Connect → Companion Sync.','Nano: Menü → Verbinden → Companion Sync.','Nano: Meniu → Conectare → Companion Sync.'],
  uploadWifiTwo:['Use the same Wi-Fi. Connect on the site with the address Nano shows.','Im selben WLAN: auf der Website mit der Nano-Adresse verbinden.','Pe același Wi-Fi: conectează-te pe site cu adresa afișată de Nano.'],
  uploadWifiThree:['Site: Library → Add → Book. Choose a file; keep Sync open until it finishes.','Website: Library → Add → Book. Datei wählen; Sync bis zum Ende offen lassen.','Site: Library → Add → Book. Alege un fișier; lasă Sync deschis până la final.'],
  uploadUsbOne:['Open this page in Chrome or Edge on your laptop.','Öffne diese Seite in Chrome oder Edge auf deinem Laptop.','Deschide această pagină în Chrome sau Edge pe laptop.'],
  uploadUsbTwo:['Plug in a USB data cable, then choose Connect with USB on the site.','USB-Datenkabel anschließen; auf der Website Connect with USB wählen.','Conectează un cablu USB de date; pe site alege Connect with USB.'],
  uploadUsbThree:['Library → Add → Book. Choose a file; leave the cable in until it finishes.','Library → Add → Book. Datei wählen; Kabel bis zum Ende angeschlossen lassen.','Library → Add → Book. Alege un fișier; lasă cablul conectat până la final.'],
});
let bookUploadMethod='wifi';
function bookUploadLesson(){
  const keys=bookUploadMethod==='wifi'?['uploadWifiOne','uploadWifiTwo','uploadWifiThree']:['uploadUsbOne','uploadUsbTwo','uploadUsbThree'];
  return `<div class="book-upload18"><a class="companion-qr" href="${companionUrl}" target="_blank" rel="noopener noreferrer" aria-label="${t('scanBooks')}">${companionQrSvg}</a><div class="upload-guide18"><div class="upload-methods18" role="group" aria-label="${t('connections')}">${button('Wi-Fi','upload-method',bookUploadMethod==='wifi'?'selected':'','data-method="wifi" aria-pressed="'+(bookUploadMethod==='wifi')+'"')}${button('USB','upload-method',bookUploadMethod==='usb'?'selected':'','data-method="usb" aria-pressed="'+(bookUploadMethod==='usb')+'"')}<a href="${companionUrl}" target="_blank" rel="noopener noreferrer">ionutdecebal.github.io/rsvpnano ↗</a></div><ol class="upload-steps18">${keys.map((key,i)=>`<li><span aria-hidden="true">${i+1}</span><p>${t(key)}</p></li>`).join('')}</ol></div></div>`;
}
const beforeBooksLesson=detailedLesson;
detailedLesson=function(step){return step===10?bookUploadLesson():beforeBooksLesson(step);};
window.studyViews['add-books']=()=>h14(t('addBooks'),'help')+bookUploadLesson();
helpView=function(){return h14(t('help'))+`<div class="help15 help18"><div class="help-gestures"><div><strong>${t(state.textAction==='hold'?'hold':'tap')}</strong><span>${t(state.textAction==='hold'?'touchHelpHold':'touchHelpTap')}</span></div><div><strong>↑</strong><span>${t('swipeHelp')}</span></div></div><div class="help-replay">${go(t('addBooks')+' ↗','add-books','secondary add-books-entry')}${go(t('locateButtons'),'button-map','secondary')}${button(t('replayIntro'),'replay-intro','secondary')}</div></div>`;};
window.studyViews.help=helpView;
descriptions['add-books']=['04','Add your books','A link, then three clear steps.','Scan the real QR code to open the RSVP Nano web companion. Wi-Fi and USB each have their own short instructions. The QR opens the website; it does not pair the Nano or transfer a file.','Switch Wi-Fi / USB. On a laptop, you can click the QR or the address instead. Find this page again in Settings → Help → Add books.'];
descriptions.onboarding[3]='Ten lessons cover reading, controls, speed, pacing, Menu, Page, Settings, Focus, physical buttons and adding books. The final lesson has a real companion QR and instructions you can revisit from Help.';
descriptions.help[3]='Gesture reminders, the physical-button diagram, a replayable introduction and a permanent Add books guide. Upload instructions remain easy to find after first setup.';
const beforeBooksAction=action;
action=function(name,element){
  if(name==='upload-method'){bookUploadMethod=element.dataset.method;render();return;}
  if(name==='try-book-upload'){beginIntroduction();tour.step=10;render();return;}
  beforeBooksAction(name,element);
};
render();
