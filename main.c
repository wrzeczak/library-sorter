#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

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

Book ** library; // global array, modified frequently
unsigned int num_books = 0; // as above

// currently, collections assume a couple shaky things:
// 1) they are only of one author (not a terrible assumption, but not the most general)
// 2) that each title it contains is unique
// these might have to be rectified in future but for now they are fine (book count ~= 160)
typedef struct {
    char ** titles;             // collections are defined by a list of titles
    unsigned int num_titles;    // as above
} Collection;

Collection ** collections; // global array, modified frequently
unsigned int num_collections = 0; // as above

// the necessary forward-declarations for main()
void sort_by_author(Book ** books, unsigned int num_books);
void add_collection(unsigned int num_titles, ...);
void fix_collections(Book ** library, unsigned int num_books, Collection ** collections, unsigned int num_collections);

// just copy paste this from the Excel output
#define EXPECTED_HEADER "TITLE	AUTHOR(s)	\"TRANSLATOR(s), EDITOR(s), etc.\"	SUBJECT	STATUS	DATE	ISBN\n"
#define EXPECTED_NUMBER_OF_FIELDS 7

// functions that hide necessary stuff in main(); i know i have sinned and i'm sorry
void handle_arguments_and_read_file_and_some_other_stuff(int argc, char ** argv);
void print_output_and_cleanup_stuff();

char * input_filename;          // set by handle_args...
char * output_filename = NULL;  // as above

//------------------------------------------------------------------------------

int main(int argc, char ** argv) {
    handle_arguments_and_read_file_and_some_other_stuff(argc, argv); // i've intentionally scrubbed all this away so that when i modify this program to change how it sorts i have as little clutter as possible

    //! STUFF THAT ACTUALLY CHANGES DEPENDING ON USE-CASE!!
    //!----------------------------------------------------

    sort_by_author(library, num_books); // calls sort_by_title()
    add_collection(4, "Spring Snow", "Runaway Horses", "The Temple of Dawn", "The Decay of the Angel");
    fix_collections(library, num_books, collections, num_collections);

    // okay no need to change anything else...

    print_output_and_cleanup_stuff();

    return 0;
}

//------------------------------------------------------------------------------

// i TOTALLY PROMISE these functions exist
char * verify_header(FILE * input_file); 
Book * get_book_from_line(char * line); // calls malloc()
void handle_arguments_and_read_file_and_some_other_stuff(int argc, char ** argv) {
    if(argc < 2) {
        printf("USAGE: ./sort <input filename> <optional; output filename>");
        exit(1);
    } else {
        input_filename = argv[1];
    }

    if(argc >= 3) {
        output_filename = argv[2];
    }

    if(output_filename != NULL) printf("Reading from \"%s\".\n", input_filename); // no printing if outputting to stdout
    if(output_filename != NULL) printf("Outputting to \"%s.\"\n", output_filename);

    FILE * input_file = fopen(input_filename, "r");
    char * header_line = verify_header(input_file); // does not rewind()
    if(header_line != NULL) {
        printf("ERROR: Header mismatch in %s!\n Expected \""EXPECTED_HEADER"\" Found \"%s\"!\n", input_filename, header_line);
        exit(2);
    }

    char line[512];
    memset(line, 0, 512);

    while(fgets(line, 512, input_file) != NULL) {
        num_books++;
        library = realloc(library, num_books * sizeof(Book *));
        library[num_books - 1] = get_book_from_line(line);
    }

    fclose(input_file);

    if(output_filename != NULL) printf("Read %d books from file \"%s\".\n", num_books, input_filename);
}

void print_output_and_cleanup_stuff() {
    unsigned int longest_title_length = 0;
    for(unsigned int i = 0; i < num_books; i++) {
        char * title = library[i]->title;
        unsigned int len = (unsigned int) strlen(title);
        if(len > longest_title_length)
            longest_title_length = len;
    }

    if(output_filename == NULL) {
        for(unsigned int i = 0; i < num_books; i++) {
            printf("%3d: %-*s %s\n", i, longest_title_length, library[i]->title, library[i]->author);
        }
    } else {
        FILE * output_file = fopen(output_filename, "w");
        for(unsigned int i = 0; i < num_books; i++) {
            fprintf(output_file, "%3d: %-*s %s\n", i, longest_title_length, library[i]->title, library[i]->author);
        }
    }

    // destroy library
    for(unsigned int i = 0; i < num_books; i++) {
        Book * b = library[i];
        free(b->author);
        free(b->contributor);
        free(b->date);
        free(b->isbn_s);
        free(b->status);
        free(b->subject);
        free(b->title);
        free(b);
    }

    free(library);

    // destroy collections
    for(unsigned int i = 0; i < num_collections; i++) {
        Collection * c = collections[i];
        for(unsigned int j = 0; j < c->num_titles; j++) {
            free(c->titles[j]);
        }
        free(c->titles);
        free(c);
    }

    free(collections);
}

// make sure the input file matches what we expect
// returns the header (first line) if it FAILS
// RETURNS NULL ON SUCCESS
// i know this is not an idiomatic design decision but unfortunately it does make sense
char * verify_header(FILE * input_file) {
    static char line[256]; // static because this is only called once
    memset(line, 0, 256);
    fgets(line, 256, input_file);
    // rewind(input_file);

    if(strncmp(line, EXPECTED_HEADER, strlen(line)) != 0) {
        return line;
    } else return NULL;
}

// debug print stuff
void print_book(Book book) {
    printf("\"%s\", by %s, %s. In %s, Read: %s, Acquired %s (ISBN: %s)\n", book.title, book.author, book.contributor, book.subject, book.status, book.date, book.isbn_s);
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

// -1 if (a before b), 0 if (a == b), 1 if (a after b)
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

// wrapper for the above so i can use it in qsort()
int alphabetic_priority_qsort_s(const void * _a, const void * _b) {
    const char * a = *(char **) _a;
    const char * b = *(char **) _b;
    return alphabetic_priority_s(a, b);
}

// sorts two books by their titles; qsort-compatible, expects Book **
int alphabetic_priority_title(const void * _book_a, const void * _book_b) {
    const Book * book_a = *((Book **) _book_a);
    const Book * book_b = *((Book **) _book_b);
    return alphabetic_priority_s(book_a->title, book_b->title);
}

// as above, but for authors
int alphabetic_priority_author(const void * _book_a, const void * _book_b) {
    const Book * book_a = *((Book **) _book_a);
    const Book * book_b = *((Book **) _book_b);
    return alphabetic_priority_s(book_a->author, book_b->author);
}

// modifies books in-place with qsort()
void sort_by_title(Book ** books, unsigned int num_books) {
    qsort(books, num_books, sizeof(Book *), &alphabetic_priority_title);
}

// python: "if value in values"
bool string_is_member(char ** values, unsigned int num_values, char * value) {
    for(unsigned int i = 0; i < num_values; i++) {
        if(strncmp(values[i], value, strlen(value)) == 0) return true;
    }
    return false;
}

// turns all capitals into lowercase
// "Being And Time" -> "being and time"
// this is NOT sanitize_title()
char * make_lowercase_string(char * string) {
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

// get the index of a book by one of its values
// get the index of the book titled "Being and Time":
// get_idx_by_value(library, num_books, "Being and Time", TITLE);
int get_idx_by_value(Book ** library, unsigned int num_books, char * value, BookField field) {
    for(unsigned int i = 0; i < num_books; i++) {
        Book b = *(library[i]);
        char * comp;

        // there seems to be not a good way to get around this...
        switch(field) {
            case TITLE: {
                comp = b.title;
                break;
            }
            case AUTHOR: {
                comp = b.author;
                break;
            }
            case CONTRIBUTOR: {
                comp = b.contributor;
                break;
            }
            case SUBJECT: {
                comp = b.subject;
                break;
            }
            case STATUS: {
                comp = b.status;
                break;
            }
            case DATE: {
                comp = b.date;
                break;
            }
            case ISBN_S: {
                comp = b.isbn_s;
                break;
            }
            default: {
                printf("You fucked this shit up.\n");
                exit(69);
            }
        }

        char * lowercase_value = malloc(strlen(value) + 1);
        memset(lowercase_value, 0, strlen(value) + 1);
        memcpy(lowercase_value, make_lowercase_string(value), strlen(value) + 1);

        comp = make_lowercase_string(comp);
        
        if(strncmp(comp, lowercase_value, strlen(lowercase_value)) == 0) {
            free(lowercase_value);
            return i;
        }

        free(lowercase_value);
    }
    return -1;
}

// sorts by author
// ALSO sorts within authors by title
// Book A, Author A
// Book B, Author A
// Book C, Author B
void sort_by_author(Book ** books, const unsigned int num_books) {
    // sort alphabetically by author
    qsort(books, num_books, sizeof(Book *), &alphabetic_priority_author);

    char * authors_multiple[num_books];
    unsigned int am_idx = 0;

    // search for authors with multiple entries; only works if they are sorted by author name
    char * last_author = NULL;
    for(unsigned int i = 1; i < num_books; i++) {
        last_author = books[i - 1]->author;
        if(strncmp(last_author, books[i]->author, strlen(last_author)) == 0) {
            if(!string_is_member(authors_multiple, am_idx, last_author)) {
                authors_multiple[am_idx] = last_author;
                am_idx++;
            }
        }
    }

    unsigned int author_spans[am_idx];
    unsigned int author_begins[am_idx];

    // get how many books are under each author's name
    for(unsigned int i = 0; i < am_idx; i++) {
        char * author = authors_multiple[i];
        unsigned int start_idx = get_idx_by_value(books, num_books, author, AUTHOR); // only works if sorted by author AND if get_idx_by_value() returns the first instance of value

        unsigned int j = 1;
        char * next_author = books[start_idx + j]->author;
        while(strncmp(next_author, author, strlen(author)) == 0) {
            j++;
            next_author = books[start_idx + j]->author;
        }

        author_begins[i] = start_idx;
        author_spans[i] = j;
    }

    // printf("Found %d authors with multiple publications.\n", am_idx);

    // for(unsigned int i = 0; i < am_idx; i++) {
    //     printf("%d: Author \"%s\" has %d publications:\n", i, authors_multiple[i], author_spans[i]);
    //     for(unsigned int j = 0; j < author_spans[i]; j++) {
    //         printf("\t\"%s\".\n", books[author_begins[i] + j]->title);
    //     }
    // }

    // extract the titles and sort them separately
    // then, modify the values in-place
    for(unsigned int i = 0; i < am_idx; i++) {
        unsigned int start_idx = author_begins[i];
        unsigned int num_titles = author_spans[i];
        char * title_buf[author_spans[i]];

        for(unsigned int j = start_idx; j < (start_idx + num_titles); j++) {
            title_buf[j - start_idx] = books[j]->title;
        }

        qsort(title_buf, num_titles, sizeof(char *), &alphabetic_priority_qsort_s);

        for(unsigned int j = 0; j < num_titles; j++) {
            char * title = title_buf[j];
            unsigned int current_title_idx = get_idx_by_value(books, num_books, title, TITLE);
            unsigned int new_title_idx = start_idx + j;

            Book * tmp = books[current_title_idx];
            books[current_title_idx] = books[new_title_idx];
            books[new_title_idx] = tmp;
        }
    }

    // should be done?

    // printf("Found %d authors with multiple publications.\n", am_idx);

    // for(unsigned int i = 0; i < am_idx; i++) {
    //     printf("%d: Author \"%s\" has %d publications:\n", i, authors_multiple[i], author_spans[i]);
    //     for(unsigned int j = 0; j < author_spans[i]; j++) {
    //         printf("\t\"%s\".\n", books[author_begins[i] + j]->title);
    //     }
    // }
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

// impure
// append a collection (...) to the global collections variable
void add_collection(unsigned int num_titles, ...) {
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

    num_collections++;
    collections = realloc(collections, num_collections * sizeof(Collection));
    collections[num_collections - 1] = coll;
}

// NOTE: this assumes all the titles are unique; i may need to rework this to include author/isbn information as well
// if this occurs, i should probably just redo it with isbn since that uniquely identifies books
// this also assumes collections are single-author, which is their intended purpose (for now...)
void fix_collections(Book ** library, unsigned int num_books, Collection ** collections, unsigned int num_collections) {
    for(unsigned int i = 0; i < num_collections; i++) {
        Collection c = *(collections[i]);
        char * first_title = c.titles[0];

        int potential_first_title_idx = get_idx_by_value(library, num_books, first_title, TITLE);
        if(potential_first_title_idx < 0) {
            printf("Somethign is wrong with the collection; couldn't find %d %s!\n", TITLE, first_title);
            exit(67);
        }
        unsigned int first_title_idx = (unsigned int) potential_first_title_idx;

        // printf("First title in collection %d is \"%s\" (%d).\n", i, library[first_title_idx]->title, first_title_idx);

        // now, re-sort all the author's titles
        char * author = library[first_title_idx]->author;
        unsigned int author_start_idx = get_idx_by_value(library, num_books, author, AUTHOR); // no error checking

        // get author span
        unsigned int span = 1;
        char * next_author = library[author_start_idx + span]->author;
        while(strncmp(next_author, author, strlen(author)) == 0) {
            span++;
            next_author = library[author_start_idx + span]->author;
        }

        // long ass debug printfs because this took a while to figure out
        // not deleting them in case something big breaks
        // printf("Author \"%s\" has %d works, beginning at (%d).\n", author, span, author_start_idx);
        // for(unsigned int j = 0; j < span; j++) {
        //     printf("\t%d: \"%s\" (%d)\n", j, library[author_start_idx + j]->title, author_start_idx + j);
        // }

        // printf("This collectiom (#%d) contains %d titles:\n", i, c.num_titles);
        // for(unsigned int j = 0; j < c.num_titles; j++) {
        //     printf("\t%d: \"%s\" (%d)\n", j, c.titles[j], get_idx_by_value(library, num_books, c.titles[j], TITLE));
        // }

        unsigned int num_noncollected_titles = (span - c.num_titles) + 1; // +1 for the first member of the title, which we will treat as noncollected
        char * noncollected_titles[num_noncollected_titles];
        unsigned int num_titles_before = first_title_idx - author_start_idx;
        unsigned int num_titles_after = num_noncollected_titles - num_titles_before - 1;

        // printf("%d = %d + %d + %d\n", span, num_titles_before, c.num_titles, num_titles_after);

        unsigned int collected_titles_found = 0;
        for(unsigned int j = 0; j < span; j++) {
            char * title = library[author_start_idx + j]->title;
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

        // put the first member of the collection in
        // noncollected_titles[num_titles_before] = c.titles[0];

        // sort the noncollected titles
        qsort(noncollected_titles, num_noncollected_titles, sizeof(char *), &alphabetic_priority_qsort_s);

        for(unsigned int j = 0; j < num_noncollected_titles; j++) {
            if(strncmp(noncollected_titles[j], c.titles[0], strlen(c.titles[0])) == 0) {
                num_titles_before = j;
                num_titles_after = span - num_titles_before - c.num_titles;
            }
        }

        // printf("The author's non-collected titles:\n");
        // for(unsigned int j = 0; j < num_noncollected_titles; j++) {
        //     printf("\t%d: \"%s\" (%d)\n", j, noncollected_titles[j], get_idx_by_value(library, num_books, noncollected_titles[j], TITLE));
        // }

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
        
        // printf("The author's collected titles:\n");
        // for(unsigned int j = 0; j < span; j++) {
        //     printf("\t%d: \"%s\" (%d)\n", j, collected_titles[j], get_idx_by_value(library, num_books, collected_titles[j], TITLE));
        // }

        // now, all the author's titles are sorted; modify library in-place to reflect this
        // for(unsigned int j = 0; j < span; j++) { SWAPPING BREAKS EVERYTHING OH MY GOD
        //     char * title = collected_titles[j];
        //     unsigned int current_title_idx = get_idx_by_value(library, num_books, title, TITLE);
        //     unsigned int new_title_idx = author_start_idx + j;

        //     Book * tmp = library[current_title_idx];
        //     library[current_title_idx] = library[new_title_idx];
        //     library[new_title_idx] = tmp;
        // }

        // how about that variable name!
        Book * collected_titles_in_book_form = malloc(span * sizeof(Book));

        // deep copy the data from library in the order listed in the (properly sorted) collected_titles
        for(unsigned int j = 0; j < span; j++) {
            char * title = collected_titles[j];
            collected_titles_in_book_form[j] = *(library[get_idx_by_value(library, num_books, title, TITLE)]); // DEEP COPY!
        }

        // overwrite library with the properly sorted books
        for(unsigned int j = 0; j < span; j++) {
            *(library[author_start_idx + j]) = collected_titles_in_book_form[j];
        }

        free(collected_titles_in_book_form);
    }
}