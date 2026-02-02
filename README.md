# Library Sorter
---

I maintain an Excel spreadsheet with all the books in my collection ordered so that I can sort them on my shelf, track when I get books, etc. My problem is that Excel's alphabetical sorting is not smart enough (or, probably more likely, I am not smart enough to figure it out) to do a couple special things:

1) Ignore certain words when sorting alphabetically; I want *The Archealogy of Knowledge* under **A**; I want *On Authority and Revelation* also under **A**. Excluding "The," "An," "A," and "On" is what I want from this.
2) Group certain books as a collection. I only have one such case right now (Yukio Mishima's *The Sea of Fertility* tetralogy), but it's important that these books stay together, superseding alphabetization within an author. The group needs to start at the correct alphabetical position of the first book in the tetralogy, continue from there, then resume proper alphabetical ordering.
3) This is a minor nitpick, but in order to properly sort books (by author, then by title), I have to first sort by title, then by author; I keep forgetting to do this. This does not need this done by me.

This program can do everything I need. I'm putting it on Github for my future use, and maybe for others'.

---

### Usage

```terminal
> gcc -o sort main.c
```

Then, run it on some data:
```terminal
> ./sort input.txt output.txt
```

The `input.txt` format is determined by Excel; I export from Excel to tab-delimited .txt file (.csv would have been my preferred choice, but this was easier to parse given that I have a lot of datapoints that contain commas). Modifying this would require modifying `EXPECTED_HEADER`, `EXPECTED_NUMBER_OF_FIELDS`, and probably `get_book_from_line()`, and maybe the `Book` struct and `BookField` enums themselves. The order of the input data shouldn't matter for correctness purposes; the `input.txt` provided here is sorted by ISBN number, and it still gives correct results.

The `output.txt` format is determined by `print_output_and_cleanup_stuff()`. I deliberately squirreled away everything before and after the actual processing of stuff in `main()` into these two (terrible, impure, blah blah blah) functions to make my own maintenance of the program (for the purposes of actually using it) easier. Yes, I could (should) have put all the extra functions and data types and whatnot into a header file, but I also sat down and wrote this in one night, and it does what I want.

If you want any help using it or fitting it to your needs, I might be able and willing to if you email me (`wrzeczak@wrzeczak.net`/`wrzeczak@protonmail.com`) or find me on Discord (`wrzeczak`; much less reliable).