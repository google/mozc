FORMS = radical_search.ui hand_writing.ui \
        character_palette.ui stroke_count.ui
HEADERS = character_pad_main.h radical_search_widget.h \
          result_list.h hand_writing_widget.h hand_writing_canvas.h \
	  character_palette_widget.h \
          stroke_count_widget.h \
          character_palette_table_widget.h unicode_util.h selection_handler.h
SOURCES = character_pad_main..cc radical_search_widget.cc \
          hand_writing_widget.cc hand_writing_canvas.cc \
          stroke_count_widget.cc \
          main.cc result_list.cc \
	  character_palette_widget.cc character_palette_table_widget.cc \
          unicode_util.cc selection_handler.cc
TRANSLATIONS = character_pad_en.ts character_pad_ja.ts
RESOURCES = character_pad.qrc
# win32:QMAKE_CXXFLAGS += /J
win32:LIBS += libzinnia.lib user32.lib
unix:LIBS += -lzinnia
# mac:LIBS += -lzinnia

