#!/usr/bin/env python3
"""Package production raster captures and a portable, annotatable review board."""
import argparse,base64,json,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
GROUPS=[
 {'id':'onboarding','title':'First use','description':'Welcome → SD check → gestures → speed and pacing → navigation → buttons → books → dictionary.'},
 {'id':'reading','title':'Reading & navigation','description':'Book ↔ middle-button controls → menu, page browser or dictionary. Find place previews a jump before committing.'},
 {'id':'library','title':'Library & books','description':'Menu → Library → book details/open. Chapters and search help you find a passage.'},
 {'id':'dictionary','title':'Dictionary','description':'Controls → Dictionary → word search or explicit online lookup. Connections → Dictionary setup.'},
 {'id':'focus','title':'Focus','description':'Menu → Focus → read with focus or timer only. Presets → edit/add. Breaks pause reading.'},
 {'id':'appearance','title':'Appearance & pace','description':'Settings → Reading or Appearance. Typography, guides and reading/paused controls retain their own settings.'},
 {'id':'system','title':'Device & connections','description':'Settings → Display, Connections, Device or Help. Includes keyboards, upload, storage, encryption and updates.'},
 {'id':'status','title':'Loading & recovery','description':'Startup, library loading, errors and interrupted operations. Status states use the firmware renderer.'}]
TITLES={'menu':'Main menu','quick-controls':'Quick controls','reader-paused':'Reader · paused','reader-playing':'Reader · playing','dictionary':'Dictionary · offline definition','dictionary-setup':'Dictionary · install and online help','reading':'Reading settings','reading-controls':'Reading controls','display':'Display settings','help':'Help & introduction','updates':'Firmware & updates','encryption':'Settings encryption','encryption-confirm':'Encryption confirmation','wifi-settings':'Wi-Fi & update source','wifi-scan':'Wi-Fi · networks','wifi-scanning':'Wi-Fi · scanning','wifi-empty':'Wi-Fi · none found','wifi-failed':'Wi-Fi · scan failed','wifi-password':'Wi-Fi · password','ota-owner':'Update repository · keyboard','ota-tag':'Release tag · keyboard'}
INTRO=['Welcome','Touch practice','Quick controls','Find your speed','Tune pacing','Essential menu','Page navigation','Essential settings','Focus timer','Physical buttons','Upload books','Dictionary']
COVERAGE={'Read':'menu','Library':'library','Chapters':'chapters','Settings':'settings','ReadingSettings':'reading-controls','InterfaceSettings':'display-controls','PacingSettings':'pacing','ReaderAppearance':'appearance-type','BookFonts':'book-font-language','NetworkSettings':'wifi-settings','WifiScan':'wifi-scan','WifiConnect':'wifi-password','NetworkEdit':'ota-tag','Device':'device','StorageEncryption':'encryption','Sync':'companion','Ota':'updates','FocusTimers':'focus','FocusEditor':'focus-editor','FocusNameEdit':'focus-name','FocusSession':'focus-session','Reader':'reader-paused','Usb':'usb-transfer','Status':'startup','Standby':'standby','ReadingHub':'reading','DisplayHub':'display','FocusPresets':'focus-presets','FindPlace':'find-place','FindChapters':'find-chapters','BookSearch':'search-results','BookSearchKeyboard':'search-keyboard','QuickControls':'quick-controls','Onboarding':'intro-en-0','ButtonMap':'button-map','AddBooks':'add-books','TouchCheck':'touch-check-edges','Dictionary':'dictionary','DictionarySearch':'dictionary-search','DictionaryHelp':'dictionary-setup'}
# These retained enum values are not reachable in the LCD release's navigation.
INACTIVE={'StorageHub':'LCD Device uses the storage-status action instead.','PowerHub':'LCD Device offers power off; sleep is in Display settings.'}
def classify(n):
 if n.startswith(('intro-','setup-')):return 'onboarding'
 if n.startswith('dictionary'):return 'dictionary'
 if n.startswith('focus'):return 'focus'
 if n.startswith(('appearance','pacing','reading','chunk')):return 'appearance'
 if n.startswith(('reader','page','find-','search-','quick-')):return 'reading'
 if n.startswith(('library','chapter')):return 'library'
 if n.startswith(('startup','loading','status')):return 'status'
 return 'system'
def title(n):
 if n in TITLES:return TITLES[n]
 m=re.fullmatch(r'intro-(en|de|ro)-(\d+)',n)
 if m:return 'Introduction · '+INTRO[int(m[2])]+' · '+m[1].upper()
 return n.replace('-',' ').capitalize()
def route(g,n):
 if g=='onboarding':return 'First boot / Settings → Help → Replay introduction'
 if g=='dictionary':return 'Book → middle button → Dictionary' if n!='dictionary-setup' else 'Settings → Connections → Dictionary setup'
 if g=='reading':return 'Book → middle button → Page / Find place' if n.startswith(('page','find','search')) else 'Book → middle button for controls'
 if g=='appearance':return 'Menu → Settings → Reading / Appearance'
 if g=='focus':return 'Menu → Start Focus → Presets / Session'
 if g=='library':return 'Menu → Library / Chapters'
 if g=='status':return 'During startup, loading or recovery'
 return 'Menu → Settings → Display / Connections / Device / Help'
def links(g,n):
 if n=='menu':return ['reader-paused','library','chapters','settings','focus']
 if n=='settings':return ['reading','appearance-type','display','connections','device','help']
 if n=='quick-controls':return ['reader-paused','menu','page','dictionary']
 if n=='connections':return ['wifi-settings','companion','usb-transfer','dictionary','dictionary-setup']
 if n=='device':return ['updates','encryption','status-storage']
 if n=='help':return ['button-map','add-books','intro-en-0']
 if n in ('display','display-controls'):return ['touch-check-edges','settings']
 if n=='wifi-settings':return ['wifi-scan','wifi-saved','updates']
 if g=='dictionary':return ['quick-controls','dictionary-search','dictionary-setup']
 if g=='focus':return ['menu','focus','focus-presets','focus-session']
 if g=='onboarding':return ['intro-en-0','intro-en-11','menu']
 return ['menu','settings']
def main():
 p=argparse.ArgumentParser();p.add_argument('screens',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
 sha=subprocess.check_output(['git','rev-parse','--short=12','HEAD'],cwd=ROOT,text=True).strip()
 dirty=bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT).strip())
 items=[]
 for f in sorted(a.screens.glob('*.png')):
  n=f.stem
  if n in ('storage','power'):continue
  g=classify(n);items.append({'id':n,'title':title(n),'group':g,'route':route(g,n),'links':links(g,n),'image':'data:image/png;base64,'+base64.b64encode(f.read_bytes()).decode()})
 names={s['id'] for s in items};coverage=[]
 enum=(ROOT/'src/ui/screens/Screens.h').read_text().split('enum class Screen : uint8_t {',1)[1].split('};',1)[0]
 for screen in re.findall(r'^\s*(\w+)\s*,',enum,re.M):
  if screen in INACTIVE:coverage.append({'screen':screen,'captured':True,'detail':'Not reachable here: '+INACTIVE[screen]});continue
  name=COVERAGE.get(screen,screen.lower());found=name in names
  coverage.append({'screen':screen,'captured':found,'detail':name if found else 'Capture pending: '+name})
  if not found:
   g=classify(name);items.append({'id':name,'title':title(name),'group':g,'route':route(g,name),'links':links(g,name),'image':None})
 data={'build':'UI + dictionary candidate · '+sha+(' · working candidate' if dirty else ''),'board':'LCD 3.49 Rev1 · 640 × 172','groups':GROUPS,'screens':items,'coverage':coverage}
 encoded=json.dumps(data,ensure_ascii=False).replace('<','\\u003c')
 html=Path(__file__).with_name('review.html').read_text().replace('__REVIEW_DATA__',encoded)
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(html)
 (a.output.parent/'screen-coverage.json').write_text(json.dumps({'build':data['build'],'coverage':coverage},indent=2)+'\n')
 print(f'{a.output}: {len(names)} captures; {sum(not x["captured"] for x in coverage)} screen routes need captures')
if __name__=='__main__':main()
