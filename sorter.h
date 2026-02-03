#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

// just copy paste this from the Excel output
#define EXPECTED_HEADER "TITLE	AUTHOR(s)	\"TRANSLATOR(s), EDITOR(s), etc.\"	SUBJECT	STATUS	DATE	ISBN\n"
#define EXPECTED_NUMBER_OF_FIELDS 7

//------------------------------------------------------------------------------

typedef enum { // see Book for explanations
    TITLE,
    AUTHOR,
    CONTRIBUTOR,
    SUBJECT,
    STATUS,
    DATE,
    ISBN_S
} BookField;

typedef struct {
    char * title;           // the title of the work;   "Being and Time"
    char * author;          // the author(s);           "Martin Heidegger"
    char * contributor;     // anyone else, not above;  "trans. Macquarrie and Robinson"
    char * subject;         // general grouping;        "Philosophy; Metaphysics"
    char * status;          // how much i've read;      "None"
    char * date;            // abou when i got it;      "2024 December"
    char * isbn_s;          // ISBN in string form;     "978006157594"
} Book;

// currently, collections assume a couple shaky things:
// 1) they are only of one author (not a terrible assumption, but not the most general)
// 2) that each title it contains is unique
// these might have to be rectified in future but for now they are fine (book count ~= 160)
typedef struct {
    char ** titles;             // collections are defined by a list of titles
    unsigned int num_titles;    // as above
} Collection;

typedef struct {
    Book ** books;
    unsigned int num_books;
    unsigned int books_capacity;

    Collection ** collections;
    unsigned int num_collections;
    unsigned int collections_capacity;
} Library;

//----------------------------
// forward declarations for primary functions

typedef int (*book_field_getter)(Library *, const char *);

int get_idx_by_title(Library * library, const char * title);
int get_idx_by_author(Library * library, const char * author);
int get_idx_by_contributor(Library * library, const char * contributor);
int get_idx_by_subject(Library * library, const char * subject);
int get_idx_by_status(Library * library, const char * status);
int get_idx_by_date(Library * library, const char * date);
int get_idx_by_isbn_s(Library * library, const char * isbn_s);

const book_field_getter get_by[] = {
    [TITLE] = get_idx_by_title,
    [AUTHOR] = get_idx_by_author,
    [CONTRIBUTOR] = get_idx_by_contributor,
    [SUBJECT] = get_idx_by_subject,
    [STATUS] = get_idx_by_status,
    [DATE] = get_idx_by_date,
    [ISBN_S] = get_idx_by_isbn_s,
};

//----------------------------

char * verify_header(FILE * input_file);
Book * get_book_from_line(char * line);
int alphabetic_priority_author(const void * _book_a, const void * _book_b);
int alphabetic_priority_title(const void * _book_a, const void * _book_b);
int alphabetic_priority_qsort_s(const void * _a, const void * _b);
bool string_is_member(char ** values, unsigned int num_values, char * value);
int get_idx_by_value(Book ** library, unsigned int num_books, char * value, BookField field);

//------------------------------------------------------------------------------
// primary functions - stuff that gets called directly in main()

struct parse_args_ret_t {
    char * input_filename;
    char * output_filename;
};

struct parse_args_ret_t parse_args(int argc, char ** argv) {
    static struct parse_args_ret_t output = { 0 };
    
    if(argc < 2) {
        printf("USAGE:\nsort <required: input filename> <optional: output filename>\n\n");
        exit(1);
    }

    output.input_filename = argv[1];

    if(argc >= 3) output.output_filename = argv[2];
    else output.output_filename = NULL;

    return output;
}

//----------------------------

typedef enum {
    OUTPUT_STDOUT = 0,
    OUTPUT_TXT,
    OUTPUT_HTML
} OutputFormat;

struct open_files_ret_t {
    FILE * input_file;
    FILE * output_file;
    OutputFormat output_format;
};

struct open_files_ret_t open_files(struct parse_args_ret_t args) {
    static struct open_files_ret_t output = { 0 };

    output.input_file = fopen(args.input_filename, "r");
    if(args.output_filename == NULL) {
        // by default, OUTPUT_STDOUT
        output.output_file = NULL;
        output.output_format = OUTPUT_STDOUT;
    } else {
        output.output_file = fopen(args.output_filename, "w");
        size_t output_filename_len = strlen(args.output_filename);
        if(strncmp(".html", args.output_filename + (output_filename_len - 5), strlen(".html")) == 0) output.output_format = OUTPUT_HTML;
        else output.output_format = OUTPUT_TXT;
    }

    return output;
}

Library parse_library(FILE * input_file) {
    static Library output = { 0 };
    output.books_capacity = 2;
    output.num_books = 0;
    
    char * header_line = verify_header(input_file); // does not rewind()
    
    if(header_line != NULL) {
        printf("ERROR: Header mismatch!\n Expected \""EXPECTED_HEADER"\" Found \"%s\"!\n", header_line);
        exit(2);
    }
    
    char line[512];
    memset(line, 0, 512);
    output.books = (Book **) calloc(output.books_capacity, sizeof(Book *));
    
    while(fgets(line, 512, input_file) != NULL) {
        if(output.num_books >= output.books_capacity) {
            output.books_capacity *= 2;
            output.books = realloc(output.books, output.books_capacity * sizeof(Book *));
        }
        output.books[output.num_books] = get_book_from_line(line);
        output.num_books++;
    }
    
    fclose(input_file);
    
    return output;
}

void sort_by_author(Library * library) {
    // sort alphabetically by author
    qsort(library->books, library->num_books, sizeof(Book *), &alphabetic_priority_author);
    
    // get every author with multiple entries
    char * authors_multiple[library->num_books];
    unsigned int am_idx = 0;
    
    // search for authors with multiple entries; only works if they are sorted by author name
    char * last_author = NULL;
    for(unsigned int i = 1; i < library->num_books; i++) {
        last_author = library->books[i - 1]->author;
        if(strncmp(last_author, library->books[i]->author, strlen(last_author)) == 0) {
            if(!string_is_member(authors_multiple, am_idx, last_author)) {
                authors_multiple[am_idx] = last_author;
                am_idx++;
            }
        }
    }
    
    // maybe i could do this in one loop, but i'll do it in two for the sake of code simplicity
    unsigned int author_spans[am_idx];
    unsigned int author_begins[am_idx];
    
    // get how many books are under each author's name
    for(unsigned int i = 0; i < am_idx; i++) {
        char * author = authors_multiple[i];
        unsigned int start_idx = get_by[AUTHOR](library, author); // only works if sorted by author AND if get_idx_by_value() returns the first instance of value
        
        unsigned int author_count = 1;
        char * next_author = library->books[start_idx + author_count]->author;
        while(((start_idx + author_count) < library->num_books)
        && (strncmp(next_author, author, strlen(author)) == 0)) {
            author_count++;
            next_author = library->books[start_idx + author_count]->author;
        }
        
        // now, get all the books of that author
        Book * author_books[author_count];
        
        memcpy(author_books, library->books + (unsigned char) start_idx, author_count * sizeof(Book *));
        qsort(author_books, author_count, sizeof(Book *), &alphabetic_priority_title);
        
        // sorted, now copy
        memcpy(library->books + (unsigned char) start_idx, author_books, author_count * sizeof(Book *));
    }
}

void add_collection(Library * library, unsigned int num_titles, ...) {
    if(num_titles < 2) {
        printf("Bad collection at %d. Kill yourself.\n", __LINE__); // will this __LINE__ be in the 500s or at the callsite? doesn't matter. kill yourself.
        exit(-80085);
    }

    va_list args;               // if macros are my favorite feature then varargs are my second favorite
    va_start(args, num_titles); // although python does these in an infinitely safer and way better way

    Collection * coll = malloc(sizeof(Collection));
    coll->titles = malloc(sizeof(char *));
    coll->num_titles = 0;

    for(unsigned int i = 0; i < num_titles; i++) {
        char * title = va_arg(args, char *);

        coll->num_titles++;
        coll->titles = realloc(coll->titles, coll->num_titles * sizeof(char *));
        coll->titles[coll->num_titles - 1] = malloc(strlen(title) + 1);
        memset(coll->titles[coll->num_titles - 1], 0, strlen(title) + 1);
        memcpy(coll->titles[coll->num_titles - 1], title, strlen(title) + 1);
    }

    for(unsigned int i = 0; i < coll->num_titles; i++) {
        if(get_by[TITLE](library, coll->titles[i]) == -1) {
            printf("It's so fucking over. \"%s\" is not in the library. Fuck. Eggplant ratatouille.\n", coll->titles[i]);
            exit(67);
        }
    }

    library->num_collections++;
    library->collections = realloc(library->collections, library->num_collections * sizeof(Collection *));
    library->collections[library->num_collections - 1] = coll;
}

void apply_collections(Library * library) {
    unsigned int num_collections = library->num_collections;
    unsigned int num_books = library->num_books;
    Book ** books = library->books;
    Collection ** collections = library->collections;

    for(unsigned int i = 0; i < num_collections; i++) {
        Collection c = *(collections[i]);
        char * first_title = c.titles[0];

        unsigned int first_title_idx = get_by[TITLE](library, first_title);

        // now, re-sort all the author's titles
        char * author = books[first_title_idx]->author;
        unsigned int author_start_idx = get_by[AUTHOR](library, author);

        // get author span
        unsigned int span = 1;
        char * next_author = books[author_start_idx + span]->author;
        while(strncmp(next_author, author, strlen(author)) == 0) {
            span++;
            next_author = books[author_start_idx + span]->author;
        }

        unsigned int num_noncollected_titles = (span - c.num_titles) + 1; // +1 for the first member of the title, which we will treat as noncollected
        char * noncollected_titles[num_noncollected_titles];
        unsigned int num_titles_before = first_title_idx - author_start_idx;
        unsigned int num_titles_after = num_noncollected_titles - num_titles_before - 1;

        unsigned int collected_titles_found = 0;
        for(unsigned int j = 0; j < span; j++) {
            char * title = books[author_start_idx + j]->title;
            // string_is_member() causes problems here??
            bool is_in_collection = false;

            for(unsigned int k = 1; k < c.num_titles; k++) { // k = 1 to skip first member of collection
                if(strncmp(c.titles[k], title, strlen(title)) == 0) is_in_collection = true;
            }

            if(!is_in_collection) {
                // append to noncol
                noncollected_titles[j - collected_titles_found] = title;
            } else {
                collected_titles_found++; // do not append and note it has been skipped
            }
        }

        // sort the noncollected titles
        qsort(noncollected_titles, num_noncollected_titles, sizeof(char *), &alphabetic_priority_qsort_s);

        for(unsigned int j = 0; j < num_noncollected_titles; j++) {
            if(strncmp(noncollected_titles[j], c.titles[0], strlen(c.titles[0])) == 0) {
                num_titles_before = j;
                num_titles_after = span - num_titles_before - c.num_titles;
            }
        }

        // now, stitch everything up
        char * collected_titles[span];

        for(unsigned int j = 0; j < num_titles_before; j++) {
            collected_titles[j] = noncollected_titles[j];
        }

        for(unsigned int j = 0; j < c.num_titles; j++) {
            collected_titles[j + num_titles_before] = c.titles[j];
        }

        for(unsigned int j = 0; j < num_titles_after; j++) { // not doing the offsets implicitly becausae they are too complex :(
            collected_titles[j + num_titles_before + c.num_titles] = noncollected_titles[j + num_titles_before + 1];
        }
        
        // how about that variable name!
        Book * collected_titles_in_book_form = malloc(span * sizeof(Book));

        // deep copy the data from books in the order listed in the (properly sorted) collected_titles
        for(unsigned int j = 0; j < span; j++) {
            char * title = collected_titles[j];
            collected_titles_in_book_form[j] = *(books[get_by[TITLE](library, title)]); // DEEP COPY!
        }

        // overwrite books with the properly sorted books
        for(unsigned int j = 0; j < span; j++) {
            *(library->books[author_start_idx + j]) = collected_titles_in_book_form[j];
        }

        free(collected_titles_in_book_form);
    }
}

void do_output(Library library, FILE * output_file, OutputFormat output_format) {
    const char * txt_format_str = "%3d: %-*s %s\n";
    const char * html_preamble = "<style>\n\tbody {\n\t\tcolor: white;\n\t\tbackground-color: #222;\n\t}\n</style>\n\n<table style=\"width: 100%%;\">\n\t<tr>\n\t\t<th>NUMBER</th>\n\t\t<th>TITLE</th>\n\t\t<th>AUTHOR</th>\n\t</tr>\n";
    const char * html_format_str = "\t<tr>\n\t\t<td>%d</td>\n\t\t<td>%s</td>\n\t\t<td>%s</td>\n\t</tr>\n";
    
    size_t longest_title_length = 0;
    for(unsigned int i = 0; i < library.num_books; i++) {
        size_t len = strlen(library.books[i]->title);
        if(len > longest_title_length) {
            longest_title_length = len;
        }
    }

    switch(output_format) {
        case OUTPUT_STDOUT: {
            for(unsigned int i = 0; i < library.num_books; i++) {
                printf(txt_format_str, i + 1, longest_title_length, library.books[i]->title, library.books[i]->author);
            }
            break;
        }
        case OUTPUT_TXT: {
            for(unsigned int i = 0; i < library.num_books; i++) {
                fprintf(output_file, txt_format_str, i + 1, longest_title_length, library.books[i]->title, library.books[i]->author);
            }
            break;
        }
        case OUTPUT_HTML: {
            fprintf(output_file, html_preamble);
            for(unsigned int i = 0; i < library.num_books; i++) {
                fprintf(output_file, html_format_str, i + 1, library.books[i]->title, library.books[i]->author);
            }
            break;
        }
    }
}

//------------------------------------------------------------------------------
// secondary functions - not directly called by main()

//----------------------------
// forward declarations thereof

char * sanitize_data(char * value);
int alphabetic_priority_s(const char * _a, const char * _b);
char * sanitize_title(const char * title);
int alphabetic_priority_c(char a, char b);
char * make_lowercase_string(const char * string);

// make sure the input file matches what we expect
// returns the header (first line) if it FAILS
// RETURNS NULL ON SUCCESS
// i know this is not an idiomatic design decision but unfortunately it does make sense
char * verify_header(FILE * input_file) {
    static char line[256]; // static because this is only called once
    memset(line, 0, 256);
    fgets(line, 256, input_file);

    if(strncmp(line, EXPECTED_HEADER, strlen(line)) != 0) {
        return line;
    } else return NULL;
}

// parse each line of the input file
// this will not work on the first (header) line
Book * get_book_from_line(char * line) {
    Book * output = malloc(sizeof(Book));

    char * token = sanitize_data(strtok(line, "\t"));
    output->title = malloc(strlen(token) + 1);
    memcpy(output->title, token, strlen(token) + 1);

    // MACROS!!! MACROS!!!! YIPPPEEE!!!!!
    // i love doing these little time saver macros so much
    // i know some people hate them but macros are genuinely my favorite C feature
    // they're so useful (and so enticingly pernicious... danger... intrigue...)
    #define GET_FIELD(field) do { \
        token = sanitize_data(strtok(NULL, "\t")); \
        output->field = malloc(strlen(token) + 1); \
        memcpy(output->field, token, strlen(token) + 1); \
    } while(0); 

    GET_FIELD(author);
    GET_FIELD(contributor);
    GET_FIELD(subject);
    GET_FIELD(status);
    GET_FIELD(date);
    GET_FIELD(isbn_s);

    #undef GET_FIELD

    return output;
}

int alphabetic_priority_author(const void * _book_a, const void * _book_b) {
    const Book * book_a = *((Book **) _book_a);
    const Book * book_b = *((Book **) _book_b);
    return alphabetic_priority_s(book_a->author, book_b->author);
}

int alphabetic_priority_title(const void * _book_a, const void * _book_b) {
    const Book * book_a = *((Book **) _book_a);
    const Book * book_b = *((Book **) _book_b);
    return alphabetic_priority_s(book_a->title, book_b->title);
}

int alphabetic_priority_s(const char * _a, const char * _b) {
    char * temp = sanitize_title(_a);    // have to malloc this and make a copy
    char * a = malloc(strlen(temp) + 1); // because we are dealing with static buffers
    memcpy(a, temp, strlen(temp) + 1);   // which are nice if you only call once
    char * b = sanitize_title(_b);       // but not so much if you call twice

    unsigned int smallest_strlen = (unsigned int) ((strlen(a) < strlen(b)) ? strlen(a) : strlen(b));

    for(unsigned int i = 0; i < smallest_strlen; i++) {
        int cmp = alphabetic_priority_c(a[i], b[i]);

        if(cmp != 0) {
            free(a);
            return cmp;
        }
    }

    free(a);
    return 0;
}

int alphabetic_priority_c(char a, char b) {
    if(a < b) return -1;
    if(a > b) return 1;
    return 0; // a == b
}

// remove all spaces, remove "the," "on," "an," "a," turn all letters lowercase
// this makes titles just slightly fuzzy which might be useful in future
// "Being And Time" should equal "Being and Time" => "beingandtime"
char * sanitize_title(const char * title) {
    static char output_buf[256];
    memset(output_buf, 0, 256);
    unsigned int output_buf_idx = 0;

    size_t beginning_offset = 0;
    // i LOVE macros for eliminating redundant code
    // this could NEVER go wrong <3
    #define OFFSET(str) if(strncmp(title, str, strlen(str)) == 0) beginning_offset = strlen(str)

    OFFSET("The ");
    OFFSET("An ");
    OFFSET("On ");
    OFFSET("A ");

    #undef OFFSET

    for(size_t i = beginning_offset; i < strlen(title); i++) {
        char c = title[i];
        if(c != ' ') { // exclude spaces
            if((c >= 'A') && (c <= 'Z')) c += 'a' - 'A'; // decapitalize capitals

            if((c >= 'a') && (c <= 'z')) { // only include alphabetical characters
                // append to buf
                output_buf[output_buf_idx] = c;
                output_buf_idx++;
            }
        }
    }

    return output_buf;
}

// get rid of "" and newlines
char * sanitize_data(char * value) {
    static char output_buffer[256];
    memset(output_buffer, 0, 256);
    unsigned int output_buffer_idx = 0;

    for(size_t i = 0; i < strlen(value); i++) {
        char v = value[i];
        if((v != '\"') && (v != '\n')) {
            output_buffer[output_buffer_idx] = v;
            output_buffer_idx++;
        }
    }

    return output_buffer;
}

// python: "if value in values"
bool string_is_member(char ** values, unsigned int num_values, char * value) {
    for(unsigned int i = 0; i < num_values; i++) {
        if(strncmp(values[i], value, strlen(value)) == 0) return true;
    }
    return false;
}

int get_idx_by_title(Library * library, const char * title) {
    char * comp = malloc(strlen(title) + 1);
    memset(comp, 0, strlen(title) + 1);
    memcpy(comp, make_lowercase_string(title), strlen(title));
    size_t strlen_comp = strlen(comp);

    for(unsigned int i = 0; i < library->num_books; i++) {
        Book b = *(library->books[i]);
        if(strncmp(comp, make_lowercase_string(b.title), strlen_comp) == 0) {
            free(comp);
            return i;
        }
    }

    free(comp);
    return -1;
}

int get_idx_by_author(Library * library, const char * author) {
    char * comp = malloc(strlen(author) + 1);
    memset(comp, 0, strlen(author) + 1);
    memcpy(comp, make_lowercase_string(author), strlen(author));
    size_t strlen_comp = strlen(comp);

    for(unsigned int i = 0; i < library->num_books; i++) {
        Book b = *(library->books[i]);
        if(strncmp(comp, make_lowercase_string(b.author), strlen_comp) == 0) {
            free(comp);
            return i;
        }
    }

    free(comp);
    return -1;
}

int get_idx_by_contributor(Library * library, const char * contributor) {
    char * comp = malloc(strlen(contributor) + 1);
    memset(comp, 0, strlen(contributor) + 1);
    memcpy(comp, make_lowercase_string(contributor), strlen(contributor));
    size_t strlen_comp = strlen(comp);

    for(unsigned int i = 0; i < library->num_books; i++) {
        Book b = *(library->books[i]);
        if(strncmp(comp, make_lowercase_string(b.contributor), strlen_comp) == 0) {
            free(comp);
            return i;
        }
    }

    free(comp);
    return -1;
}

int get_idx_by_subject(Library * library, const char * subject) {
    char * comp = malloc(strlen(subject) + 1);
    memset(comp, 0, strlen(subject) + 1);
    memcpy(comp, make_lowercase_string(subject), strlen(subject));
    size_t strlen_comp = strlen(comp);

    for(unsigned int i = 0; i < library->num_books; i++) {
        Book b = *(library->books[i]);
        if(strncmp(comp, make_lowercase_string(b.subject), strlen_comp) == 0) {
            free(comp);
            return i;
        }
    }

    free(comp);
    return -1;
}

int get_idx_by_status(Library * library, const char * status) {
    char * comp = malloc(strlen(status) + 1);
    memset(comp, 0, strlen(status) + 1);
    memcpy(comp, make_lowercase_string(status), strlen(status));
    size_t strlen_comp = strlen(comp);

    for(unsigned int i = 0; i < library->num_books; i++) {
        Book b = *(library->books[i]);
        if(strncmp(comp, make_lowercase_string(b.status), strlen_comp) == 0) {
            free(comp);
            return i;
        }
    }

    free(comp);
    return -1;
}

int get_idx_by_date(Library * library, const char * date) {
    char * comp = malloc(strlen(date) + 1);
    memset(comp, 0, strlen(date) + 1);
    memcpy(comp, make_lowercase_string(date), strlen(date));
    size_t strlen_comp = strlen(comp);

    for(unsigned int i = 0; i < library->num_books; i++) {
        Book b = *(library->books[i]);
        if(strncmp(comp, make_lowercase_string(b.date), strlen_comp) == 0) {
            free(comp);
            return i;
        }
    }

    free(comp);
    return -1;
}

int get_idx_by_isbn_s(Library * library, const char * isbn_s) {
    char * comp = malloc(strlen(isbn_s) + 1);
    memset(comp, 0, strlen(isbn_s) + 1);
    memcpy(comp, make_lowercase_string(isbn_s), strlen(isbn_s));
    size_t strlen_comp = strlen(comp);

    for(unsigned int i = 0; i < library->num_books; i++) {
        Book b = *(library->books[i]);
        if(strncmp(comp, make_lowercase_string(b.isbn_s), strlen_comp) == 0) {
            free(comp);
            return i;
        }
    }

    free(comp);
    return -1;
}

// wrapper for the above so i can use it in qsort()
int alphabetic_priority_qsort_s(const void * _a, const void * _b) {
    const char * a = *(char **) _a;
    const char * b = *(char **) _b;
    return alphabetic_priority_s(a, b);
}

// turns all capitals into lowercase
// "Being And Time" -> "being and time"
// this is NOT sanitize_title()
char * make_lowercase_string(const char * string) {
    static char output_buf[256];
    memset(output_buf, 0, 256);
    unsigned int output_buf_idx = 0;

    for(size_t i = 0; i < strlen(string); i++) {
        char c = string[i];
        if((c >= 'A') && (c <= 'Z')) c += 'a' - 'A';

        // append to buf
        output_buf[output_buf_idx] = c;
        output_buf_idx++;
    }

    return output_buf;
}
