#include "Jovial/Components/Components2D.h"
#include "Jovial/FileSystem/FileSystem.h"
#include "Jovial/JovialEngine.h"
#include "Jovial/Renderer/TextRenderer.h"
#include "Jovial/Shapes/Color.h"
#include "Jovial/Std/Array.h"
#include "Jovial/Std/Vector2i.h"
#include <cctype>

using namespace jovial;

class WordFinder {
public:
    WordFinder() {
        dictionary = fs::read_entire_file(fs::Path(JV_RES_DIR JV_SEP "dictionary.txt"));
    }

    void find_words() {
        int matches_count = 0;
        for (size_t i = 0; i < dictionary.length() && matches_count < matches.length;) {
            while (dictionary[i] == ' ' || dictionary[i] == '\n') {
                i += 1;
                if (i >= dictionary.length()) {
                    return;
                }
            }

            StringView potential_word = StringView(dictionary.items, 0, dictionary.size() - 1);
            potential_word.begin = i;

            potential_word = potential_word.chop_to('\n');
            i += potential_word.size();

            if (word[word_len - 1] != '*') {
                if (potential_word.size() != word_len) {
                    continue;
                }
            } else {
                if (potential_word.size() < word_len) {
                    continue;
                }
            }

            bool matched = true;
            for (size_t j = 0; j < word_len && matched; ++j) {
                if (word[j] != '_' && word[j] != '*') {
                    if (word[j] != potential_word[j]) {
                        matched = false;
                    }
                }
            }
            if (matched) {
                printj(potential_word);
                matches[matches_count] = potential_word;
                matches_count += 1;
            }
        }
    }

    void find(Font *font, Vector2 pos) {
        if (word_len < JV_ARRAY_LEN(word) - 1) {
            for (char c: Input::get_chars_typed()) {
                if (c == ' ' || c == '?') {
                    word[word_len] = '_';
                    word_len++;
                } else {
                    word[word_len] = c;
                    word_len++;
                }
                for (int i = 0; i < matches.length; ++i) {
                    matches[i] = {};
                }
            }
        }

        if (Input::is_typed(Actions::Backspace) && word_len > 0) {
            word_len--;
            word[word_len] = '\0';
            for (int i = 0; i < matches.length; ++i) {
                matches[i] = {};
            }
        }
        if (Input::is_typed(Actions::Enter) && matches[0].items == nullptr) {
            find_words();
        }

        if (word_len == 0) {
            const char *text = "Type to start finding";
            float width = font->measure(text).x;

            Vector2 position = pos - Vector2(width / 2, font->size / 2);
            font->draw(position, text);
        } else {
            float width = font->measure(word).x;

            Vector2 position = pos - Vector2(width / 2, font->size / 2);
            font->draw(position, word);
        }

        for (int i = 0; i < matches.length; ++i) {
            float width = measure_text(matches[i], font).x;

            StringView view = matches[i];
            Vector2 position = pos - Vector2(width / 2, font->size / 2);
            position.y -= font->size * (float) (i + 1);
            draw_text(position, view, font, {});
        }
    }

    char word[30] = {};
    int word_len = 0;

    String dictionary;

    Array<StringView, 3> matches{};
};
