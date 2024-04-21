#include "Jovial/Components/Components2D.h"
#include "Jovial/Components/Util/LazyAssets.h"
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

namespace Fonts {
}// namespace Fonts

struct Crossword {
    Vector2i size;
    char *letters;

    explicit Crossword(Vector2i size) : size(size) {
        letters = (char *) malloc(sizeof(char) * size.x * size.y);
        for (int i = 0; i < size.x * size.y; ++i) {
            letters[i] = '\0';
        }
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

    void set(Vector2i coord, char c) const {
        if (coord.x > size.x || coord.y > size.y) {
            JV_CORE_ERROR("coord ", coord, " is larger than crossword size of ", size);
        } else {
            letters[coord.y * size.x + coord.x] = (char) toupper(c);
        }
    }

    [[nodiscard]] float square_size() const {
        auto winsize = Window::get_current_size() - Vector2(PADDING * 2);
        float x = winsize.x / (float) size.x;
        float y = winsize.y / (float) size.y;
        return math::min(x, y);
    }

    ~Crossword() {
        free(letters);
    }
};

void draw_letter(Vector2 position, char ch, Font *font) {
    int index = ch - font->first_char;
    Glyph &glyph = font->glyphs[index];

    float x = position.x + (float) glyph.offsetX + (float) font->padding;
    float y = position.y - (float) glyph.offsetY + (float) font->padding - (font->rects[index].h - font->rects[index].y);

    auto padding = (float) font->padding;
    Rect2 uv = {font->rects[index].x - padding, font->rects[index].y - padding,
                font->rects[index].w + padding, font->rects[index].h + padding};

    uv.x /= (float) font->texture.width;
    uv.y /= (float) font->texture.height;
    uv.w /= (float) font->texture.width;
    uv.h /= (float) font->texture.height;

    rendering::TextureDrawProperties props;
    props.centered = true;
    props.uv = uv;
    props.size = {font->size, font->size};
    props.color = Colors::Black;

    rendering::draw_texture(font->texture, position + Vector2(font->size / 2, font->size / 2), props);
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
        }
    }

    void draw_lines(const Crossword &crossword) const {
        for (int x = 0; x < crossword.size.x + 1; ++x) {
            rendering::draw_line({Vector2((float) x * square_size + PADDING, PADDING),
                                  Vector2((float) x * square_size + PADDING, (float) Window::get_current_height() - PADDING)},
                                 2.0f,
                                 {.color = Colors::Black});
        }
        for (int y = 0; y < crossword.size.y + 1; ++y) {
            rendering::draw_line({Vector2(PADDING, (float) y * square_size + PADDING),
                                  Vector2((float) Window::get_current_height() - PADDING, (float) y * square_size + PADDING)},
                                 2.0f,
                                 {.color = Colors::Black});
        }
    }

    void draw(const Crossword &crossword) {
        update_square_size(crossword);
        draw_lines(crossword);

        Rect2 base_square({0.0f, 0.0f}, {square_size, square_size});

        for (int x = 0; x < crossword.size.x; ++x) {
            for (int y = 0; y < crossword.size.y; ++y) {
                Vector2 pos = Vector2((float) x * square_size, (float) y * square_size) + Vector2(PADDING);

                char letter = crossword.at({x, y});

                if (letter == '\0') {
                    rendering::ShapeDrawProperties props{};
                    props.color = Colors::Black;
                    Rect2 square = base_square.move(pos);
                    rendering::draw_rect2(square, props);
                } else {
                    draw_letter(pos, letter, &font);
                }
            }
        }
    }

    float square_size = 0;
    Font font;
    unsigned char *font_data;
    int font_data_len;
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

        if (mode != NONE) {
            for (char c: Input::get_chars_typed()) {
                crossword.set(current_square, c);
                move(mode, crossword);
            }
            if (Input::is_typed(Actions::Backspace)) {
                move(-direction_vector(mode), crossword);
                crossword.set(current_square, '\0');
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
                    default:
                        break;
                }
            }
        }
    }

    void update_current_square(const Crossword &crossword, const CrosswordDrawer &drawer) {
        Vector2i new_square = (Input::get_mouse_position() - Vector2(PADDING)) / drawer.square_size;
        if (crossword.contains(new_square)) {
            current_square = new_square;
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
    World() : crossword({20, 20}) {}

    void update() override {
        drawer.draw(crossword);
        navigator.navigate(crossword, drawer);
    }

    Crossword crossword;
    CrosswordNavigator navigator;
    CrosswordDrawer drawer;
};

int main() {
    Jovial game;

    game.push_plugin(new Window({WINDOW_NAME, WINDOW_SIZE, WINDOW_RES, nullptr, Colors::JovialWhite}));
    game.push_plugins(plugins::default_plugins_2d);
    game.push_plugin(new NodePlugin(new World));

    game.run();

    return 0;
}
