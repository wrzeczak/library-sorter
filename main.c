#include "sorter.h"

//------------------------------------------------------------------------------

int main(int argv, char ** argc) {
    struct parse_args_ret_t args = parse_args(argv, argc);
    struct open_files_ret_t files = open_files(args);
    
    Library library = parse_library(files.input_file);

    //----------------------------

    sort_by_author(&library);
    add_collection(&library, 4, "Spring Snow", "Runaway Horses", "The Temple of Dawn", "The Decay of the Angel");
    apply_collections(&library);

    //----------------------------

    do_output(library, files.output_file, files.output_format);

    return 0;
}