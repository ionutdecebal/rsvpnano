#pragma once

#include <stdint.h>

enum class UiLanguage : uint8_t {
  English = 0,
  Spanish,
  French,
  German,
  Romanian,
  Polish,
  Count,
};

enum class UiText : uint8_t {
  Resume,
  Chapters,
  Library,
  Settings,
  UsbTransfer,
  PowerOff,
  Back,
  Display,
  TypographyTune,
  WordPacing,
  Theme,
  Brightness,
  Language,
  ReadingMode,
  LongWords,
  Complexity,
  Punctuation,
  ResetPacing,
  Night,
  Dark,
  Light,
  On,
  Off,
  FontSize,
  Typeface,
  PhantomWords,
  RedHighlight,
  Tracking,
  Anchor,
  GuideWidth,
  GuideGap,
  Reset,
  Typography,
  TapToExit,
  TapToReset,
  TapChangeSample,
  TapExitSample,
  TapToggleSample,
  TapCycleSample,
  CurrentBook,
  Start,
  StartOfBook,
  RestartBook,
  AreYouSure,
  NoKeepPlace,
  YesRestart,
  NoSamples,
  Large,
  Medium,
  Small,
  Standard,
  RsvpMode,
  ScrollMode,
};

namespace Localization {

inline UiLanguage sanitizeLanguage(uint8_t value) {
  if (value >= static_cast<uint8_t>(UiLanguage::Count)) {
    return UiLanguage::English;
  }
  return static_cast<UiLanguage>(value);
}

inline UiLanguage nextLanguage(UiLanguage current) {
  uint8_t value = static_cast<uint8_t>(current);
  value = static_cast<uint8_t>((value + 1) % static_cast<uint8_t>(UiLanguage::Count));
  return static_cast<UiLanguage>(value);
}

inline const char *languageName(UiLanguage language) {
  switch (language) {
    case UiLanguage::Spanish:
      return "Espanol";
    case UiLanguage::French:
      return "Francais";
    case UiLanguage::German:
      return "Deutsch";
    case UiLanguage::Romanian:
      return "Romana";
    case UiLanguage::Polish:
      return "Polski";
    case UiLanguage::English:
    default:
      return "English";
  }
}

inline const char *text(UiLanguage language, UiText key) {
  switch (language) {
    case UiLanguage::Spanish:
      switch (key) {
        case UiText::Resume:
          return "Reanudar";
        case UiText::Chapters:
          return "Capitulos";
        case UiText::Library:
          return "Biblioteca";
        case UiText::Settings:
          return "Ajustes";
        case UiText::UsbTransfer:
          return "USB";
        case UiText::PowerOff:
          return "Apagar";
        case UiText::Back:
          return "Atras";
        case UiText::Display:
          return "Pantalla";
        case UiText::TypographyTune:
          return "Tipografia";
        case UiText::WordPacing:
          return "Ritmo lectura";
        case UiText::Theme:
          return "Tema";
        case UiText::Brightness:
          return "Brillo";
        case UiText::Language:
          return "Idioma";
        case UiText::ReadingMode:
          return "Modo lectura";
        case UiText::LongWords:
          return "Palabras largas";
        case UiText::Complexity:
          return "Complejidad";
        case UiText::Punctuation:
          return "Puntuacion";
        case UiText::ResetPacing:
          return "Restablecer ritmo";
        case UiText::Night:
          return "Noche";
        case UiText::Dark:
          return "Oscuro";
        case UiText::Light:
          return "Claro";
        case UiText::On:
          return "Si";
        case UiText::Off:
          return "No";
        case UiText::FontSize:
          return "Tamano";
        case UiText::Typeface:
          return "Fuente";
        case UiText::PhantomWords:
          return "Palabras fantasma";
        case UiText::RedHighlight:
          return "Rojo";
        case UiText::Tracking:
          return "Espaciado";
        case UiText::Anchor:
          return "Ancla";
        case UiText::GuideWidth:
          return "Ancho guia";
        case UiText::GuideGap:
          return "Hueco guia";
        case UiText::Reset:
          return "Restablecer";
        case UiText::Typography:
          return "Tipografia";
        case UiText::TapToExit:
          return "toca salir";
        case UiText::TapToReset:
          return "toca reiniciar";
        case UiText::TapChangeSample:
          return "Toca cambiar L/R muestra";
        case UiText::TapExitSample:
          return "Toca salir L/R muestra";
        case UiText::TapToggleSample:
          return "Toca alternar L/R muestra";
        case UiText::TapCycleSample:
          return "Toca ciclo L/R muestra";
        case UiText::CurrentBook:
          return "Libro actual";
        case UiText::Start:
          return "Inicio";
        case UiText::StartOfBook:
          return "Inicio del libro";
        case UiText::RestartBook:
          return "Reiniciar libro";
        case UiText::AreYouSure:
          return "Seguro?";
        case UiText::NoKeepPlace:
          return "No, conservar";
        case UiText::YesRestart:
          return "Si, reiniciar";
        case UiText::NoSamples:
          return "Sin muestras";
        case UiText::Large:
          return "Grande";
        case UiText::Medium:
          return "Mediano";
        case UiText::Small:
          return "Pequeno";
        case UiText::Standard:
          return "Estandar";
        case UiText::RsvpMode:
          return "RSVP";
        case UiText::ScrollMode:
          return "Scroll pagina";
      }
      break;
    case UiLanguage::French:
      switch (key) {
        case UiText::Resume:
          return "Reprendre";
        case UiText::Chapters:
          return "Chapitres";
        case UiText::Library:
          return "Bibliotheque";
        case UiText::Settings:
          return "Reglages";
        case UiText::UsbTransfer:
          return "USB";
        case UiText::PowerOff:
          return "Eteindre";
        case UiText::Back:
          return "Retour";
        case UiText::Display:
          return "Affichage";
        case UiText::TypographyTune:
          return "Typographie";
        case UiText::WordPacing:
          return "Rythme mots";
        case UiText::Theme:
          return "Theme";
        case UiText::Brightness:
          return "Luminosite";
        case UiText::Language:
          return "Langue";
        case UiText::ReadingMode:
          return "Mode lecture";
        case UiText::LongWords:
          return "Mots longs";
        case UiText::Complexity:
          return "Complexite";
        case UiText::Punctuation:
          return "Ponctuation";
        case UiText::ResetPacing:
          return "Reinit. rythme";
        case UiText::Night:
          return "Nuit";
        case UiText::Dark:
          return "Sombre";
        case UiText::Light:
          return "Clair";
        case UiText::On:
          return "Oui";
        case UiText::Off:
          return "Non";
        case UiText::FontSize:
          return "Taille";
        case UiText::Typeface:
          return "Police";
        case UiText::PhantomWords:
          return "Mots fantomes";
        case UiText::RedHighlight:
          return "Accent rouge";
        case UiText::Tracking:
          return "Espacement";
        case UiText::Anchor:
          return "Ancre";
        case UiText::GuideWidth:
          return "Largeur guide";
        case UiText::GuideGap:
          return "Ecart guide";
        case UiText::Reset:
          return "Reinit.";
        case UiText::Typography:
          return "Typographie";
        case UiText::TapToExit:
          return "toucher sortie";
        case UiText::TapToReset:
          return "toucher reinit.";
        case UiText::TapChangeSample:
          return "Touchez change  G/D echant.";
        case UiText::TapExitSample:
          return "Touchez sortie  G/D echant.";
        case UiText::TapToggleSample:
          return "Touchez option  G/D echant.";
        case UiText::TapCycleSample:
          return "Touchez cycle  G/D echant.";
        case UiText::CurrentBook:
          return "Livre actuel";
        case UiText::Start:
          return "Debut";
        case UiText::StartOfBook:
          return "Debut du livre";
        case UiText::RestartBook:
          return "Relancer livre";
        case UiText::AreYouSure:
          return "Confirmer ?";
        case UiText::NoKeepPlace:
          return "Non, garder";
        case UiText::YesRestart:
          return "Oui, relancer";
        case UiText::NoSamples:
          return "Aucun exemple";
        case UiText::Large:
          return "Grand";
        case UiText::Medium:
          return "Moyen";
        case UiText::Small:
          return "Petit";
        case UiText::Standard:
          return "Standard";
        case UiText::RsvpMode:
          return "RSVP";
        case UiText::ScrollMode:
          return "Defilement page";
      }
      break;
    case UiLanguage::German:
      switch (key) {
        case UiText::Resume:
          return "Weiter";
        case UiText::Chapters:
          return "Kapitel";
        case UiText::Library:
          return "Bibliothek";
        case UiText::Settings:
          return "Optionen";
        case UiText::UsbTransfer:
          return "USB";
        case UiText::PowerOff:
          return "Ausschalten";
        case UiText::Back:
          return "Zuruck";
        case UiText::Display:
          return "Anzeige";
        case UiText::TypographyTune:
          return "Typografie";
        case UiText::WordPacing:
          return "Lesetempo";
        case UiText::Theme:
          return "Thema";
        case UiText::Brightness:
          return "Helligkeit";
        case UiText::Language:
          return "Sprache";
        case UiText::ReadingMode:
          return "Lesemodus";
        case UiText::LongWords:
          return "Lange Worter";
        case UiText::Complexity:
          return "Komplexitat";
        case UiText::Punctuation:
          return "Zeichen";
        case UiText::ResetPacing:
          return "Tempo zuruck";
        case UiText::Night:
          return "Nacht";
        case UiText::Dark:
          return "Dunkel";
        case UiText::Light:
          return "Hell";
        case UiText::On:
          return "Ein";
        case UiText::Off:
          return "Aus";
        case UiText::FontSize:
          return "Schriftgrad";
        case UiText::Typeface:
          return "Schriftart";
        case UiText::PhantomWords:
          return "Phantomworter";
        case UiText::RedHighlight:
          return "Rotfokus";
        case UiText::Tracking:
          return "Laufweite";
        case UiText::Anchor:
          return "Anker";
        case UiText::GuideWidth:
          return "Guidebreite";
        case UiText::GuideGap:
          return "Guidespalt";
        case UiText::Reset:
          return "Zurucksetzen";
        case UiText::Typography:
          return "Typografie";
        case UiText::TapToExit:
          return "tippen zum Ende";
        case UiText::TapToReset:
          return "tippen zum Reset";
        case UiText::TapChangeSample:
          return "Tippen aendern  L/R Probe";
        case UiText::TapExitSample:
          return "Tippen zuruck  L/R Probe";
        case UiText::TapToggleSample:
          return "Tippen schalten  L/R Probe";
        case UiText::TapCycleSample:
          return "Tippen wechseln  L/R Probe";
        case UiText::CurrentBook:
          return "Aktuelles Buch";
        case UiText::Start:
          return "Start";
        case UiText::StartOfBook:
          return "Buchanfang";
        case UiText::RestartBook:
          return "Buch neu";
        case UiText::AreYouSure:
          return "Sicher?";
        case UiText::NoKeepPlace:
          return "Nein, merken";
        case UiText::YesRestart:
          return "Ja, neu";
        case UiText::NoSamples:
          return "Keine Proben";
        case UiText::Large:
          return "Gross";
        case UiText::Medium:
          return "Mittel";
        case UiText::Small:
          return "Klein";
        case UiText::Standard:
          return "Standard";
        case UiText::RsvpMode:
          return "RSVP";
        case UiText::ScrollMode:
          return "Seiten-Scroll";
      }
      break;
    case UiLanguage::Romanian:
      switch (key) {
        case UiText::Resume:
          return "Continua";
        case UiText::Chapters:
          return "Capitole";
        case UiText::Library:
          return "Biblioteca";
        case UiText::Settings:
          return "Setari";
        case UiText::UsbTransfer:
          return "USB";
        case UiText::PowerOff:
          return "Oprire";
        case UiText::Back:
          return "Inapoi";
        case UiText::Display:
          return "Afisaj";
        case UiText::TypographyTune:
          return "Tipografie";
        case UiText::WordPacing:
          return "Ritm cuvinte";
        case UiText::Theme:
          return "Tema";
        case UiText::Brightness:
          return "Luminoz.";
        case UiText::Language:
          return "Limba";
        case UiText::ReadingMode:
          return "Mod citire";
        case UiText::LongWords:
          return "Cuvinte lungi";
        case UiText::Complexity:
          return "Complexitate";
        case UiText::Punctuation:
          return "Punctuatie";
        case UiText::ResetPacing:
          return "Reset ritm";
        case UiText::Night:
          return "Noapte";
        case UiText::Dark:
          return "Inchis";
        case UiText::Light:
          return "Deschis";
        case UiText::On:
          return "Pornit";
        case UiText::Off:
          return "Oprit";
        case UiText::FontSize:
          return "Marime";
        case UiText::Typeface:
          return "Font";
        case UiText::PhantomWords:
          return "Cuvinte fantoma";
        case UiText::RedHighlight:
          return "Accent rosu";
        case UiText::Tracking:
          return "Spatiere";
        case UiText::Anchor:
          return "Ancora";
        case UiText::GuideWidth:
          return "Latime ghid";
        case UiText::GuideGap:
          return "Spatiu ghid";
        case UiText::Reset:
          return "Resetare";
        case UiText::Typography:
          return "Tipografie";
        case UiText::TapToExit:
          return "atinge iesire";
        case UiText::TapToReset:
          return "atinge reset";
        case UiText::TapChangeSample:
          return "Atinge schimba  S/D proba";
        case UiText::TapExitSample:
          return "Atinge iesi  S/D proba";
        case UiText::TapToggleSample:
          return "Atinge comuta  S/D proba";
        case UiText::TapCycleSample:
          return "Atinge ciclu  S/D proba";
        case UiText::CurrentBook:
          return "Cartea curenta";
        case UiText::Start:
          return "Inceput";
        case UiText::StartOfBook:
          return "Inceputul cartii";
        case UiText::RestartBook:
          return "Reporneste cartea";
        case UiText::AreYouSure:
          return "Sigur?";
        case UiText::NoKeepPlace:
          return "Nu, pastreaza";
        case UiText::YesRestart:
          return "Da, reporneste";
        case UiText::NoSamples:
          return "Fara probe";
        case UiText::Large:
          return "Mare";
        case UiText::Medium:
          return "Mediu";
        case UiText::Small:
          return "Mic";
        case UiText::Standard:
          return "Standard";
        case UiText::RsvpMode:
          return "RSVP";
        case UiText::ScrollMode:
          return "Derulare pagina";
      }
      break;
    case UiLanguage::Polish:
      switch (key) {
        case UiText::Resume:
          return "Wznow";
        case UiText::Chapters:
          return "Rozdzialy";
        case UiText::Library:
          return "Biblioteka";
        case UiText::Settings:
          return "Ustawienia";
        case UiText::UsbTransfer:
          return "USB";
        case UiText::PowerOff:
          return "Wylacz";
        case UiText::Back:
          return "Wroc";
        case UiText::Display:
          return "Ekran";
        case UiText::TypographyTune:
          return "Typografia";
        case UiText::WordPacing:
          return "Tempo slow";
        case UiText::Theme:
          return "Motyw";
        case UiText::Brightness:
          return "Jasnosc";
        case UiText::Language:
          return "Jezyk";
        case UiText::ReadingMode:
          return "Tryb czyt.";
        case UiText::LongWords:
          return "Dlugie slowa";
        case UiText::Complexity:
          return "Zlozonosc";
        case UiText::Punctuation:
          return "Interpunk.";
        case UiText::ResetPacing:
          return "Reset tempa";
        case UiText::Night:
          return "Noc";
        case UiText::Dark:
          return "Ciemny";
        case UiText::Light:
          return "Jasny";
        case UiText::On:
          return "Tak";
        case UiText::Off:
          return "Nie";
        case UiText::FontSize:
          return "Rozmiar";
        case UiText::Typeface:
          return "Kroj";
        case UiText::PhantomWords:
          return "Slowa widma";
        case UiText::RedHighlight:
          return "Czerwony";
        case UiText::Tracking:
          return "Odstepy";
        case UiText::Anchor:
          return "Kotwica";
        case UiText::GuideWidth:
          return "Szer. guide";
        case UiText::GuideGap:
          return "Przerwa guide";
        case UiText::Reset:
          return "Reset";
        case UiText::Typography:
          return "Typografia";
        case UiText::TapToExit:
          return "dotknij wyjscie";
        case UiText::TapToReset:
          return "dotknij reset";
        case UiText::TapChangeSample:
          return "Dotknij zmien  L/R probka";
        case UiText::TapExitSample:
          return "Dotknij wyjdz  L/R probka";
        case UiText::TapToggleSample:
          return "Dotknij przel.  L/R probka";
        case UiText::TapCycleSample:
          return "Dotknij cykl  L/R probka";
        case UiText::CurrentBook:
          return "Biezaca ksiazka";
        case UiText::Start:
          return "Start";
        case UiText::StartOfBook:
          return "Poczatek ksiazki";
        case UiText::RestartBook:
          return "Restart ksiazki";
        case UiText::AreYouSure:
          return "Na pewno?";
        case UiText::NoKeepPlace:
          return "Nie, zostaw";
        case UiText::YesRestart:
          return "Tak, restart";
        case UiText::NoSamples:
          return "Brak probek";
        case UiText::Large:
          return "Duzy";
        case UiText::Medium:
          return "Sredni";
        case UiText::Small:
          return "Maly";
        case UiText::Standard:
          return "Standard";
        case UiText::RsvpMode:
          return "RSVP";
        case UiText::ScrollMode:
          return "Scroll strony";
      }
      break;
    case UiLanguage::English:
    default:
      switch (key) {
        case UiText::Resume:
          return "Resume";
        case UiText::Chapters:
          return "Chapters";
        case UiText::Library:
          return "Library";
        case UiText::Settings:
          return "Settings";
        case UiText::UsbTransfer:
          return "USB transfer";
        case UiText::PowerOff:
          return "Power off";
        case UiText::Back:
          return "Back";
        case UiText::Display:
          return "Display";
        case UiText::TypographyTune:
          return "Typography tune";
        case UiText::WordPacing:
          return "Word pacing";
        case UiText::Theme:
          return "Theme";
        case UiText::Brightness:
          return "Brightness";
        case UiText::Language:
          return "Language";
        case UiText::ReadingMode:
          return "Reading mode";
        case UiText::LongWords:
          return "Long words";
        case UiText::Complexity:
          return "Complexity";
        case UiText::Punctuation:
          return "Punctuation";
        case UiText::ResetPacing:
          return "Reset pacing";
        case UiText::Night:
          return "Night";
        case UiText::Dark:
          return "Dark";
        case UiText::Light:
          return "Light";
        case UiText::On:
          return "On";
        case UiText::Off:
          return "Off";
        case UiText::FontSize:
          return "Font size";
        case UiText::Typeface:
          return "Typeface";
        case UiText::PhantomWords:
          return "Phantom words";
        case UiText::RedHighlight:
          return "Red highlight";
        case UiText::Tracking:
          return "Tracking";
        case UiText::Anchor:
          return "Anchor";
        case UiText::GuideWidth:
          return "Guide width";
        case UiText::GuideGap:
          return "Guide gap";
        case UiText::Reset:
          return "Reset";
        case UiText::Typography:
          return "Typography";
        case UiText::TapToExit:
          return "tap to exit";
        case UiText::TapToReset:
          return "tap to reset";
        case UiText::TapChangeSample:
          return "Tap change  L/R sample";
        case UiText::TapExitSample:
          return "Tap exit  L/R sample";
        case UiText::TapToggleSample:
          return "Tap toggle  L/R sample";
        case UiText::TapCycleSample:
          return "Tap cycle  L/R sample";
        case UiText::CurrentBook:
          return "Current book";
        case UiText::Start:
          return "Start";
        case UiText::StartOfBook:
          return "Start of book";
        case UiText::RestartBook:
          return "Restart book";
        case UiText::AreYouSure:
          return "Are you sure?";
        case UiText::NoKeepPlace:
          return "No, keep place";
        case UiText::YesRestart:
          return "Yes, restart";
        case UiText::NoSamples:
          return "No samples";
        case UiText::Large:
          return "Large";
        case UiText::Medium:
          return "Medium";
        case UiText::Small:
          return "Small";
        case UiText::Standard:
          return "Standard";
        case UiText::RsvpMode:
          return "RSVP";
        case UiText::ScrollMode:
          return "Page scroll";
      }
      break;
  }

  return "";
}

}  // namespace Localization
