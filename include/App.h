#pragma once

#include "BoardSupport.h"
#include "BookParser.h"
#include "ButtonHandler.h"
#include "DisplayRenderer.h"
#include "ReadingEngine.h"
#include "StorageManager.h"
#include "TouchHandler.h"

class App {
 public:
  bool begin();
  void loop();

 private:
  void loadLibrary();
  void openBookById(const String& id);
  void changeBook(int delta);
  void jumpChapter(int chapterIdx);
  void saveProgressNow();
  void enterSleep();

  void handlePausedGestures(const GestureResult& g);
  void handleMenuGestures(const GestureResult& g);
  void handleChapterGestures(const GestureResult& g);

  BoardSupport board_;
  StorageManager storage_;
  BookParser parser_;
  ReadingEngine engine_;
  DisplayRenderer view_;
  ButtonHandler button_;
  TouchHandler touch_;

  AppState state_ = AppState::LOADING;
  std::vector<BookFile> books_;
  BookData currentBook_;
  size_t wordIndex_ = 0;
  int selectedBookIdx_ = 0;
  int menuIndex_ = 0;
  bool chapterMenu_ = false;
  int chapterIndex_ = 0;

  int wpm_ = 280;
  uint32_t lastProgressSaveMs_ = 0;
  uint32_t pauseStartedMs_ = 0;
};
