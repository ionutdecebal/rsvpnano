RSVP Nano SD Card Converter

Put these files at the root of the SD card, next to the books folder. You can also put the
sd_card_converter folder itself on the SD card root; the script will look one folder up if needed.

- convert_books.py
- Convert books.command
- convert_books_linux.sh
- Convert books.bat

Then run the launcher for your computer:

- Windows: double-click Convert books.bat
- macOS: double-click Convert books.command
- Linux: run convert_books_linux.sh or python3 convert_books.py

The converter scans the SD card's books folder and creates .rsvp files beside supported source
books. It skips .rsvp files that already exist unless you run:

python3 convert_books.py --force

By default the converter no longer applies a word cap. If you ever want a smaller file on purpose,
you can set one explicitly, for example:

python3 convert_books.py --max-words 50000

Supported input formats:

- .epub
- .txt
- .md / .markdown
- .html / .htm / .xhtml

The device reads .rsvp files first. If both book.txt and book.rsvp exist, book.rsvp is the one
shown by the firmware.
