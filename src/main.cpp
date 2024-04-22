#include "Jovial/Components/Components2D.h"
#include "Jovial/Components/Util/LazyAssets.h"
#include "Jovial/Components/Util/Screenshot.h"
#include "Jovial/Core/Node.h"
#include "Jovial/FileSystem/FileSystem.h"
#include "Jovial/JovialEngine.h"
#include "Jovial/Renderer/2DRenderer.h"
#include "Jovial/Renderer/TextRenderer.h"
#include "Jovial/Shapes/Color.h"
#include "Jovial/Shapes/Rect.h"
#include "Jovial/Shapes/ShapeDrawer.h"
#include "Jovial/Std/Vector2i.h"
#include <cctype>

using namespace jovial;

#define WINDOW_NAME "Sharewords"
#define WINDOW_SIZE Vector2(1280, 720)
#define WINDOW_RES Vector2(0, 0)
#define PADDING (Window::get_current_width() / 40.0f)

struct Answer {
    char hint[64] = {0};

    Vector2i coords;
    int number = 0;
};

struct Crossword {
    Vector2i size;
    char *letters;
    Vec<Answer> across;
    Vec<Answer> down;
    char title[30];

    explicit Crossword(Vector2i size, const char *title) : size(size), title() {
        letters = (char *) malloc(sizeof(char) * size.x * size.y);
        for (int i = 0; i < size.x * size.y; ++i) {
            letters[i] = '\0';
        }
        strcpy(this->title, title);
    }

    [[nodiscard]] bool contains(Vector2i coord) const {
        return coord.x >= 0 && coord.y >= 0 && coord.x < size.x && coord.y < size.y;
    }

    [[nodiscard]] char at(Vector2i coord) const {
        if (coord.x > size.x || coord.y > size.y) {
            JV_CORE_ERROR("coord ", coord, " is larger than crossword size of ", size);
            return '\0';
        }

        return letters[coord.y * size.x + coord.x];
    }

    void erase(Vector2i coord) {
        if (coord.x > size.x || coord.y > size.y) {
            JV_CORE_ERROR("coord ", coord, " is larger than crossword size of ", size);
        } else {
            for (int i = 0; i < across.size(); ++i) {
                if (across[i].coords == coord) {
                    across.swap_pop(i);
                }
            }
            for (int i = 0; i < down.size(); ++i) {
                if (down[i].coords == coord) {
                    down.swap_pop(i);
                }
            }
            set(coord, '\0');
        }
    }

    void set(Vector2i coord, char c) const {
        if (coord.x > size.x || coord.y > size.y) {
            JV_CORE_ERROR("coord ", coord, " is larger than crossword size of ", size);
        } else {
            letters[coord.y * size.x + coord.x] = (char) toupper(c);
        }
    }

    [[nodiscard]] float square_size() const {
        auto winsize = Window::get_current_size() - Vector2(PADDING * 2);
        winsize.y -= PADDING;
        float x = winsize.x / (float) size.x;
        float y = winsize.y / (float) size.y;
        return math::min(x, y);
    }

    void save_to(const fs::Path &path) const {
        String output;
        output += title;
        output += "\n";
        output += to_string(size.x) + " " + to_string(size.y) + "\n";

        for (int i = 0; i < size.x * size.y; ++i) {
            if (letters[i] == '\0') {
                output += '~';
            } else {
                output += letters[i];
            }
        }
        output += "across:\n";
        for (auto &answer: across) {
            output += to_string(answer.number) + ":" +
                      to_string(answer.coords.x) + "," + to_string(answer.coords.y) + ":" +
                      answer.hint + "\n";
        }
        output += "down:\n";
        for (auto &answer: down) {
            output += to_string(answer.number) + ":" +
                      to_string(answer.coords.x) + "," + to_string(answer.coords.y) + ":" +
                      answer.hint + "\n";
        }
        fs::write_entire_file(output, path);
    }

    explicit Crossword(const fs::Path &path) : letters(nullptr), title() {
        String input = path.read_entire_file();
        StringView view(input.items, 0, input.count);

        bool error;

        StringView title_view = view.chop_to('\n');
        for (int i = 0; i < title_view.size(); ++i) {
            title[i] = title_view[i];
        }
        view.begin += title_view.size() + 1;

        StringView width = view.chop_to(' ');
        view.begin += width.size();
        view.trim_lead();
        size.x = atoi(width, &error);
        if (error) {
            JV_CORE_FATAL("could not load ", path.str)
        }

        StringView height = view.chop_to('\n');
        view.begin += height.size();
        view.trim_lead();
        size.y = atoi(height, &error);
        if (error) {
            JV_CORE_FATAL("could not load ", path.str)
        }

        printj("Loaded crossword size: ", size.x, ", ", size.y);

        letters = (char *) malloc(sizeof(char) * size.x * size.y);
        for (int i = 0; i < size.x * size.y; ++i) {
            char c = view.first();
            if (c == '~') {
                letters[i] = '\0';
            } else {
                letters[i] = c;
            }
            view.begin += 1;
        }

        view.begin += view.chop_to('\n').size() + 1;// skip 'across:'
        if (view.size() == 0) return;

        while (view.first() != 'd') {// down:
            StringView num = view.chop_to(':');
            view.begin += num.size() + 1;

            StringView x = view.chop_to(',');
            view.begin += x.size() + 1;

            StringView y = view.chop_to(':');
            view.begin += y.size() + 1;

            StringView hint = view.chop_to('\n');
            view.begin += hint.size() + 1;

            Answer answer;

            answer.number = atoi(num, &error);
            if (error) { JV_CORE_FATAL("could not load ", path.str) }

            answer.coords.x = atoi(x, &error);
            if (error) { JV_CORE_FATAL("could not load ", path.str) }

            answer.coords.y = atoi(y, &error);
            if (error) { JV_CORE_FATAL("could not load ", path.str) }

            for (int i = 0; i < hint.size(); ++i) {
                answer.hint[i] = hint[i];
            }

            across.push_back(answer);
        }

        view.begin += view.chop_to('\n').size() + 1;// skip 'down:'
        if (view.size() == 0) return;

        while (view.size() > 0) {// down:
            StringView num = view.chop_to(':');
            view.begin += num.size() + 1;

            StringView x = view.chop_to(',');
            view.begin += x.size() + 1;

            StringView y = view.chop_to(':');
            view.begin += y.size() + 1;

            StringView hint = view.chop_to('\n');
            view.begin += hint.size() + 1;

            Answer answer;

            answer.number = atoi(num, &error);
            if (error) { JV_CORE_FATAL("could not load ", path.str) }

            answer.coords.x = atoi(x, &error);
            if (error) { JV_CORE_FATAL("could not load ", path.str) }

            answer.coords.y = atoi(y, &error);
            if (error) { JV_CORE_FATAL("could not load ", path.str) }

            for (int i = 0; i < hint.size(); ++i) {
                answer.hint[i] = hint[i];
            }

            down.push_back(answer);
        }
    }

    ~Crossword() {
        free(letters);
    }
};

void draw_number(Vector2 position, int number, Font *font) {
    String str = to_string(number);
    for (int i = 0; i < str.count; ++i) {
        int index = str[i] - font->first_char;
        Glyph &glyph = font->glyphs[index];

        auto padding = (float) font->padding;
        Rect2 uv = {font->rects[index].x - padding, font->rects[index].y - padding,
                    font->rects[index].w + padding, font->rects[index].h + padding};

        uv.x /= (float) font->texture.width;
        uv.y /= (float) font->texture.height;
        uv.w /= (float) font->texture.width;
        uv.h /= (float) font->texture.height;

        rendering::TextureDrawProperties props;
        props.centered = true;
        props.scale = Vector2(1.0f / 2);
        props.uv = uv;
        props.size = {font->size, font->size};
        props.color = Colors::Black;

        rendering::draw_texture(font->texture, position + Vector2(font->size / 5, font->size / 1.3f), props);
    }
}

struct CrosswordDrawer {
    CrosswordDrawer() {
        font_data_len = 0;
        font_data = fs::read_file_data(JV_FONTS_DIR JV_SEP "jet_brains.ttf", &font_data_len);
    }

    void update_square_size(const Crossword &crossword) {
        float new_square_size = crossword.square_size();
        if (square_size != new_square_size) {
            square_size = new_square_size;
            destroy_font(&font);
            font = Font(font_data, font_data_len, square_size);
            hints_font = Font(font_data, font_data_len, square_size / 1.5f);
        }
    }

    void draw_lines(const Crossword &crossword) const {
        for (int x = 0; x < crossword.size.x + 1; ++x) {
            rendering::draw_line({Vector2((float) x * square_size + PADDING, PADDING),
                                  Vector2((float) x * square_size + PADDING, (float) Window::get_current_height() - PADDING * 2)},
                                 2.0f,
                                 {.color = Colors::Black});
        }
        for (int y = 0; y < crossword.size.y + 1; ++y) {
            rendering::draw_line({Vector2(PADDING, (float) y * square_size + PADDING),
                                  Vector2((float) Window::get_current_height() - PADDING * 2, (float) y * square_size + PADDING)},
                                 2.0f,
                                 {.color = Colors::Black});
        }
    }

    void draw(const Crossword &crossword) {
        update_square_size(crossword);
        draw_lines(crossword);

        Rect2 base_square({0.0f, 0.0f}, {square_size, square_size});

        font.draw({PADDING, (float) Window::get_current_height() - PADDING * 1.25f},
                  crossword.title);

        for (int x = 0; x < crossword.size.x; ++x) {
            for (int y = 0; y < crossword.size.y; ++y) {
                Vector2 pos = Vector2((float) x * square_size, (float) y * square_size) + Vector2(PADDING);

                char letter = crossword.at({x, y});

                if (letter == '\0') {
                    rendering::ShapeDrawProperties props{};
                    props.color = Colors::Black;
                    Rect2 square = base_square.move(pos);
                    rendering::draw_rect2(square, props);
                } else if (!hidden) {
                    draw_char(pos, letter, &font);
                }
            }
        }
        draw_answer_numbers(crossword);
    }

    void draw_answer_numbers(const Crossword &crossword) {
        for (auto &answer: crossword.across) {
            Vector2 pos = Vector2((float) answer.coords.x * square_size, (float) answer.coords.y * square_size) +
                          Vector2(PADDING);
            draw_number(pos, answer.number, &font);
        }
        for (auto &answer: crossword.down) {
            Vector2 pos = Vector2((float) answer.coords.x * square_size, (float) answer.coords.y * square_size) +
                          Vector2(PADDING);
            draw_number(pos, answer.number, &font);
        }
    }

    bool hidden = false;

    float square_size = 0;
    Font font;
    Font hints_font;
    unsigned char *font_data;
    int font_data_len;
};

struct CrosswordHinter {
    CrosswordHinter() = default;

    [[nodiscard]] bool editing() const {
        return edit_offset != -1;
    }

    void hint(Crossword &crossword, const CrosswordDrawer &drawer) {
        Vector2 hint_pos(drawer.square_size * (float) crossword.size.x + PADDING * 2,
                         (float) Window::get_current_height() - PADDING * 3);
        Vector2 offset(PADDING, 0.0f);

        drawer.hints_font.draw(hint_pos, "Down:");
        hint_pos.y -= drawer.hints_font.size;
        for (int i = 0; i < crossword.down.size(); ++i) {
            drawer.hints_font.draw(hint_pos, to_string(crossword.down[i].number) + ".");
            drawer.hints_font.draw(hint_pos + offset, crossword.down[i].hint, {.fix_start_pos = true});

            edit_hint(false, i, hint_pos, crossword, drawer);

            hint_pos.y -= drawer.hints_font.size;
        }

        hint_pos.y -= PADDING;

        drawer.hints_font.draw(hint_pos, "Across:");
        hint_pos.y -= drawer.hints_font.size;
        for (int i = 0; i < crossword.across.size(); ++i) {
            drawer.hints_font.draw(hint_pos, to_string(crossword.across[i].number) + ".");
            drawer.hints_font.draw(hint_pos + offset, crossword.across[i].hint, {.fix_start_pos = true});

            edit_hint(true, i, hint_pos, crossword, drawer);

            hint_pos.y -= drawer.hints_font.size;
        }
    }

    void edit_hint(bool horizontal, int i, Vector2 hint_pos, const Crossword &crossword, const CrosswordDrawer &drawer) {
        Vector2 mpos = Input::get_mouse_position();
        Vector2 offset(PADDING, 0.0f);

        if (Input::is_just_pressed(Actions::LeftMouseButton) &&
            mpos.x > hint_pos.x && mpos.y > hint_pos.y && mpos.y < hint_pos.y + drawer.hints_font.size) {
            editing_horizontal = horizontal;
            edit_offset = i;
            if (horizontal)
                char_index = (int) strlen(crossword.across[i].hint);
            else
                char_index = (int) strlen(crossword.down[i].hint);
        }

        if (editing_horizontal == horizontal && i == edit_offset) {
            Vector2 cursor_pos = hint_pos + offset;
            cursor_pos.x += (float) (drawer.hints_font.glyphs[0].advanceX * char_index);

            rendering::draw_line({cursor_pos, cursor_pos + Vector2(0.0f, drawer.hints_font.size * 0.75f)},
                                 2.0f, {.color = Colors::Black});

            auto *list = &crossword.down;
            if (horizontal) {
                list = &crossword.across;
            }

            for (char c: Input::get_chars_typed()) {
                if (char_index < JV_ARRAY_LEN((*list)[i].hint)) {
                    (*list)[i].hint[char_index] = c;
                    char_index += 1;
                }
            }
            if (Input::is_typed(Actions::Backspace)) {
                if (char_index > 0) {
                    char_index -= 1;
                    (*list)[i].hint[char_index] = '\0';
                }
            }
            if (Input::is_typed(Actions::Enter) || Input::is_typed(Actions::Escape)) {
                edit_offset = -1;
            }
        }
    }

    int char_index = 0;
    bool editing_horizontal = false;
    int edit_offset = -1;
};

struct CrosswordNavigator {
    static Vector2i direction_vector(int direction) {
        switch (direction) {
            case RIGHT:
                return {1, 0};
            case LEFT:
                return {-1, 0};
            case UP:
                return {0, 1};
            case DOWN:
                return {0, -1};
            default:
                return {};
        }
    }

    [[nodiscard]] static Actions direction_to_input(int direction) {
        switch (direction) {
            case RIGHT:
                return Actions::Right;
            case DOWN:
                return Actions::Down;
            case LEFT:
                return Actions::Left;
            case UP:
                return Actions::Up;
            default:
                JV_CORE_FATAL("Invalid direction");
        }
    }

    void arrow_move(int direction, const Crossword &crossword) {
        if (Input::is_typed(direction_to_input(direction))) {
            *(int *) &mode = direction;
            if (!Input::is_pressed(Actions::LeftShift)) {
                move(direction, crossword);
            }
        }
    }

    void move(int direction, const Crossword &crossword) {
        Vector2i new_square = current_square + direction_vector(direction);
        if (crossword.contains(new_square)) {
            current_square = new_square;
        }
    }

    void move(Vector2i direction, const Crossword &crossword) {
        Vector2i new_square = current_square + direction;
        if (crossword.contains(new_square)) {
            current_square = new_square;
        }
    }

    void navigate(Crossword &crossword, const CrosswordDrawer &drawer) {
        switch (mode) {
            case RIGHT:
                drawer.font.draw(Vector2(PADDING, PADDING / 3), "Right");
                break;
            case DOWN:
                drawer.font.draw(Vector2(PADDING, PADDING / 3), "Down");
                break;
            case LEFT:
                drawer.font.draw(Vector2(PADDING, PADDING / 3), "Left");
                break;
            case UP:
                drawer.font.draw(Vector2(PADDING, PADDING / 3), "Up");
                break;
            default:
                break;
        }
        if (Input::is_pressed(Actions::LeftMouseButton)) {
            if (Input::is_pressed(Actions::LeftShift)) {
                mode = LEFT;
            } else {
                mode = RIGHT;
            }
            update_current_square(crossword, drawer);
        }
        if (Input::is_pressed(Actions::RightMouseButton)) {
            if (Input::is_pressed(Actions::LeftShift)) {
                mode = UP;
            } else {
                mode = DOWN;
            }
            update_current_square(crossword, drawer);
        }

        Vector2 current_square_pos = Vector2(PADDING) + (Vector2) current_square * drawer.square_size;
        rendering::draw_rect2_outline({current_square_pos, current_square_pos + Vector2(drawer.square_size)},
                                      2.0f, {.color = Colors::Red});

        if (Input::is_just_pressed(Actions::Escape)) {
            mode = NONE;
        }

        if (Input::is_just_pressed(Actions::Enter)) {
            if (Input::is_pressed(Actions::LeftControl)) {
                for (int i = 0; i < crossword.across.size(); ++i) {
                    if (crossword.across[i].coords == current_square) {
                        crossword.across.swap_pop(i);
                    }
                }
                for (int i = 0; i < crossword.down.size(); ++i) {
                    if (crossword.down[i].coords == current_square) {
                        crossword.down.swap_pop(i);
                    }
                }
            } else if (crossword.at(current_square) != '\0') {
                Answer answer;
                answer.coords = current_square;
                strcpy(answer.hint, "Hint");

                int num = -1;
                for (auto &a: crossword.across) {
                    if (a.coords == current_square)
                        num = a.number;
                }
                for (auto &a: crossword.down) {
                    if (a.coords == current_square)
                        num = a.number;
                }

                if (mode == DOWN || mode == UP) {
                    if (num == -1) num = crossword.down.count + 1;
                    answer.number = num;
                    crossword.down.push_back(answer);
                } else if (mode == LEFT || mode == RIGHT) {
                    if (num == -1) num = crossword.across.count + 1;
                    answer.number = num;
                    crossword.across.push_back(answer);
                }
            }
        }

        if (mode != NONE) {
            for (char c: Input::get_chars_typed()) {
                crossword.set(current_square, c);
                move(mode, crossword);
            }
            if (Input::is_typed(Actions::Backspace)) {
                move(-direction_vector(mode), crossword);
                crossword.erase(current_square);
            }

            arrow_move(UP, crossword);
            arrow_move(DOWN, crossword);
            arrow_move(LEFT, crossword);
            arrow_move(RIGHT, crossword);
        } else {
            for (char c: Input::get_chars_typed()) {
                switch (c) {
                    case 'd':
                    case 'l': {
                        move(RIGHT, crossword);
                    } break;
                    case 'a':
                    case 'h':
                        move(LEFT, crossword);
                        break;
                    case 'w':
                    case 'k':
                        move(UP, crossword);
                        break;
                    case 's':
                    case 'j':
                        move(DOWN, crossword);
                        break;

                    case 'D':
                    case 'L': {
                        if (Input::is_pressed(Actions::LeftShift)) {
                            mode = RIGHT;
                        }
                    } break;
                    case 'A':
                    case 'H': {
                        if (Input::is_pressed(Actions::LeftShift)) {
                            mode = LEFT;
                        }
                    } break;
                    case 'W':
                    case 'K': {
                        if (Input::is_pressed(Actions::LeftShift)) {
                            mode = UP;
                        }
                    } break;
                    case 'S':
                    case 'J': {
                        if (Input::is_pressed(Actions::LeftShift)) {
                            mode = DOWN;
                        }
                    } break;

                    case 'x':
                        crossword.erase(current_square);
                    default:
                        break;
                }
            }
        }
    }

    void update_current_square(Crossword &crossword, const CrosswordDrawer &drawer) {
        Vector2i new_square = (Input::get_mouse_position() - Vector2(PADDING)) / drawer.square_size;
        if (crossword.contains(new_square)) {
            current_square = new_square;
        }
        if (Input::is_pressed(Actions::LeftControl)) {
            crossword.erase(current_square);
        }
    }

    Vector2i current_square = {};
    enum {
        NONE,
        RIGHT,
        DOWN,
        LEFT,
        UP,
    } mode = NONE;
};

class World : public Node {
public:
    World() : crossword(fs::Path::res() + "crossword.shareword") {}

    void update() override {
        if (Input::is_just_released(Actions::F1)) {
            crossword.save_to(fs::Path::res() + "crossword.shareword");
        }
        if (Input::is_just_released(Actions::F2)) {
            drawer.hidden = !drawer.hidden;
        }
        if (Input::is_just_released(Actions::F3)) {
            take_screenshot("./shareword.png");
        }
        drawer.draw(crossword);
        if (!hinter.editing()) {
            navigator.navigate(crossword, drawer);
        } else {
            navigator.mode = CrosswordNavigator::NONE;
        }
        hinter.hint(crossword, drawer);
    }

    Crossword crossword;
    CrosswordNavigator navigator;
    CrosswordDrawer drawer;
    CrosswordHinter hinter;
};

int main() {
    Jovial game;

    game.push_plugin(new Window({WINDOW_NAME, WINDOW_SIZE, WINDOW_RES, nullptr, Colors::JovialWhite}));
    game.push_plugins(plugins::default_plugins_2d);
    game.push_plugin(new NodePlugin(new World));

    game.run();

    return 0;
}
