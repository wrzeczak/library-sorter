#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <raylib.h>
#include <raymath.h>

#include "sorter.h"

//--- CONSTANTS ---------------------------------------------------------------


#define FONT_SIZE 20
#define ROW_HEIGHT 28
#define ROW_BORDER 2
#define TEXT_X_OFFSET 8
#define TEXT_Y_OFFSET 4

#define TITLE_COLOR RAYWHITE
#define VALUE_COLOR BLACK
#define BORDER_COLOR GRAY

#define NUM_ROWS_AT_ONCE 35
#define WIDTH 1600
#define HEIGHT ((ROW_HEIGHT - ROW_BORDER) * (NUM_ROWS_AT_ONCE + 1)) + ROW_BORDER

#define LIBRARY_FIELD_MAPPING(name, type, title, author, contributor, subject, status, date, isbn) \
const type name[EXPECTED_NUMBER_OF_FIELDS] = { \
    [TITLE] = title, \
    [AUTHOR] = author, \
    [CONTRIBUTOR] = contributor, \
    [SUBJECT] = subject, \
    [STATUS] = status, \
    [DATE] = date, \
    [ISBN_S] = isbn \
};

LIBRARY_FIELD_MAPPING(COL_WIDTH_PERCENTS, float, 0.54f, 0.20f, 0.0f, 0.18f, 0.079f, 0.0f, 0.0f);

const unsigned int COL_WIDTHS[EXPECTED_NUMBER_OF_FIELDS] = { 
    [TITLE] = COL_WIDTH_PERCENTS[TITLE] * WIDTH,
    [AUTHOR] = COL_WIDTH_PERCENTS[AUTHOR] * WIDTH,
    [CONTRIBUTOR] = COL_WIDTH_PERCENTS[CONTRIBUTOR] * WIDTH,
    [SUBJECT] = COL_WIDTH_PERCENTS[SUBJECT] * WIDTH,
    [STATUS] = COL_WIDTH_PERCENTS[STATUS] * WIDTH,
    [DATE] = COL_WIDTH_PERCENTS[DATE] * WIDTH,
    [ISBN_S] = COL_WIDTH_PERCENTS[ISBN_S] * WIDTH
};


typedef Color (*ColumnColorizer)(char *, bool);

Color colorize_titles(char * field, bool even);
Color colorize_authors(char * field, bool even);
Color colorize_contributors(char * field, bool even);
Color colorize_subjects(char * field, bool even);
Color colorize_statuses(char * field, bool even);
Color colorize_dates(char * field, bool even);
Color colorize_isbns(char * field, bool even);

LIBRARY_FIELD_MAPPING(COL_COLORS, ColumnColorizer, colorize_titles, colorize_authors, colorize_contributors, colorize_subjects, colorize_statuses, colorize_dates, colorize_isbns);
LIBRARY_FIELD_MAPPING(COL_TITLES, char *, "Title", "Author(s)", "Contributor(s)", "Subject", "Status", "Date Acquired", "ISBN");

void draw_column_headers(const unsigned int column_widths[], const char * column_titles[]);
void draw_column_values(const unsigned int column_widths[], Library * library, unsigned int starting_at);
void draw_help_message();

//--- MAIN --------------------------------------------------------------------

int main(int argc, char ** argv) {
    //--- WINDOW INIT --------------------------------------------------------------

    InitWindow(WIDTH, HEIGHT, "");
    SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));

    //--- PROGRAM INIT -------------------------------------------------------------

    struct parse_args_ret_t args = parse_args(argc, argv);
    struct open_files_ret_t files = open_files(args);
    if(files.output_file != NULL) fclose(files.output_file); // we don't need this
    
    Library library = parse_library(files.input_file);

    //----------------------------

    sort_by_author(&library);
    add_collection(&library, 4, "Spring Snow", "Runaway Horses", "The Temple of Dawn", "The Decay of the Angel");
    apply_collections(&library);

    unsigned int starting_at = 0;

    bool show_help = false;

    //--- DRAWING ------------------------------------------------------------------

    while(!WindowShouldClose()) {
        //---- UPDATE ------------------------------------------------------------------

        if(IsKeyPressedRepeat(KEY_DOWN) || IsKeyPressedRepeat(KEY_SPACE)
            || IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_SPACE)) {
            if(starting_at < (library.num_books - NUM_ROWS_AT_ONCE)) starting_at++;
        }

        if(IsKeyPressedRepeat(KEY_UP) || IsKeyPressedRepeat(KEY_BACKSPACE)
            || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_BACKSPACE)) {
            if(starting_at > 0) starting_at--;
        }

        if(IsKeyPressed(KEY_SLASH) && IsKeyDown(KEY_LEFT_SHIFT)) {
            show_help = !show_help;
        }

        //---- DRAW --------------------------------------------------------------------

        BeginDrawing();

            ClearBackground(BLACK);

            draw_column_headers(COL_WIDTHS, COL_TITLES);
            draw_column_values(COL_WIDTHS, &library, starting_at);

            if(show_help) draw_help_message();
            
        EndDrawing();

        //------------------------------------------------------------------------------
    }

    //---- DE-INIT -----------------------------------------------------------------

    CloseWindow();
    destroy_library(library);

    return 0;
}

void draw_column_headers(const unsigned int column_widths[], const char * column_titles[]) {
    DrawRectangle(0, 0, WIDTH, ROW_HEIGHT, BLACK);
    DrawRectangleLinesEx((Rectangle) { 0, 0, WIDTH, HEIGHT }, ROW_BORDER, BORDER_COLOR);

    int offset = 0;
    for(int i = 0; i < EXPECTED_NUMBER_OF_FIELDS; i++) {
        int width = column_widths[i];
        if(width != 0) {
            const char * title = column_titles[i];
            DrawText(title, offset + 0 + TEXT_X_OFFSET, 0 + TEXT_Y_OFFSET, FONT_SIZE, TITLE_COLOR);
            DrawRectangleLinesEx((Rectangle) { offset, 0, width + ROW_BORDER, ROW_HEIGHT }, ROW_BORDER, BORDER_COLOR);
        }
        offset += width;
    }
}

void draw_column_values(const unsigned int column_widths[], Library * library, unsigned int starting_at) {
    int y_offset = 0;
    int x_offset = 0;
    int last_row_rendered = (starting_at + NUM_ROWS_AT_ONCE);
    
    for(int i = starting_at; i < last_row_rendered; i++) {
        y_offset += ROW_HEIGHT - ROW_BORDER;
        Book * book = library->books[i];
        
        for(int j = 0; j < EXPECTED_NUMBER_OF_FIELDS; j++) {
            int width = column_widths[j];
            if(width != 0) {
                char * value = get_field[j](book);
                Color background_color = COL_COLORS[j](value, (i % 2) == 0);
                DrawRectangleRec((Rectangle) { x_offset, y_offset, width + ROW_BORDER, ROW_HEIGHT }, background_color);
                DrawText(value, x_offset + TEXT_X_OFFSET, y_offset + TEXT_Y_OFFSET, FONT_SIZE, VALUE_COLOR);
                DrawRectangleLinesEx((Rectangle) { x_offset, y_offset, width + ROW_BORDER, ROW_HEIGHT }, ROW_BORDER, BORDER_COLOR);
            }
            x_offset += width;
        }

        x_offset = 0;
    }
}

void draw_help_message() {
    int m_width = 1100;
    int m_height = 290;
    
    int x_offset = (WIDTH - m_width) / 2;
    int y_offset = (HEIGHT - m_height) / 2;

    DrawRectangle(x_offset, y_offset, m_width, m_height, Fade(GetColor(0xddddddff), 0.8f));
    DrawRectangleLines(x_offset, y_offset, m_width, m_height, BLACK);

    DrawText("HELP!", x_offset + 10, y_offset + 10, 60, RED);
    DrawText("Press SPACE or DOWN to scroll down.", x_offset + 10, y_offset + 10 + 80, 40, BLACK);
    DrawText("Press BACKSPACE or UP to scroll up.", x_offset + 10, y_offset + 10 + 80 + 50, 40, BLACK);
    DrawText("Press ? (SHIFT + /) to show/close this message.", x_offset + 10, y_offset + 10 + 80 + 50 + 50, 40, BLACK);
    DrawText("Press ESC to exit this program.", x_offset + 10, y_offset + 10 +80 + 50 + 50 + 50, 40, BLACK);
}

#define DEFAULT_BACKGROUND_COLOR (Color) { 0xaa, 0xaa, 0xaa, 0xff }
#define DARKENING_AMOUNT 0x0d

static inline Color darken_color(Color color) {
    return (Color) { color.r - DARKENING_AMOUNT, color.g - DARKENING_AMOUNT, color.b - DARKENING_AMOUNT, color.a - DARKENING_AMOUNT };
}

Color colorize_titles(char * field, bool even) {
    return (even) ? DEFAULT_BACKGROUND_COLOR : darken_color(DEFAULT_BACKGROUND_COLOR);
}

Color colorize_authors(char * field, bool even) {
    return (even) ? DEFAULT_BACKGROUND_COLOR : darken_color(DEFAULT_BACKGROUND_COLOR);
}

Color colorize_contributors(char * field, bool even) {
    return (even) ? DEFAULT_BACKGROUND_COLOR : darken_color(DEFAULT_BACKGROUND_COLOR);
}

Color colorize_subjects(char * field, bool even) {
    Color c = DEFAULT_BACKGROUND_COLOR;

    // checks front
    
    #define str_case(string_value, color_hex) else if(strncmp(string_value, field, strlen(string_value)) == 0) { c = GetColor(color_hex); }
    
    if(strncmp("History", field, strlen("History")) == 0) { c = GetColor(0xdcf3d3ff); }
    str_case("Biography", 0xb9e7a7ff)
    str_case("Sociology", 0xb9e7a7ff)
    str_case("Psychology", 0xb9e7a7ff)

    str_case("Score", 0xdbdbdbff)
    str_case("Technical", 0xdbdbdbff)
    
    str_case("Literature", 0xfbe4d7ff)
    str_case("Criticism", 0xf7cab0ff)
    
    str_case("Religion", 0xf3d1f0ff)
    str_case("Textbook; Religion", 0xf3d1f0ff)
    str_case("Religious Primary", 0xe5a3dfff)
    
    str_case("Philosophy", 0xcdeefbff)

    #undef str_case

    return c;
}

Color colorize_statuses(char * field, bool even) {
    Color c = DEFAULT_BACKGROUND_COLOR;

    #define str_case(string_value, color_hex) else if(strncmp(string_value, field, strlen(string_value)) == 0) { c = GetColor(color_hex); }

    if(strncmp("None", field, strlen("None")) == 0) { c = GetColor(0xb1b1b1ff); }
    str_case("Complete", 0xffffcfff)
    str_case("Partial", 0xdbdbdbff)

    return c;
}

Color colorize_dates(char * field, bool even) {
    return (even) ? DEFAULT_BACKGROUND_COLOR : darken_color(DEFAULT_BACKGROUND_COLOR);
}

Color colorize_isbns(char * field, bool even) {
    return (even) ? DEFAULT_BACKGROUND_COLOR : darken_color(DEFAULT_BACKGROUND_COLOR);
}