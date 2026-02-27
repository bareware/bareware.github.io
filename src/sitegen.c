#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* ptr;
    char* end;
} string_t;

void trim(string_t* str) {
    while (str->ptr < str->end) {
        if (str->ptr[0] == ' ' || str->ptr[0] == '\t') {
            str->ptr++;
        } else if (str->end[-1] == ' ' || str->end[-1] == '\t') {
            str->end--;
        } else {
            break;
        }
    }
}

string_t advance_block(string_t* str, char term, bool trim_ws) {
    char* ptr = str->ptr;
    while (str->ptr < str->end) {
        char c = *str->ptr++;
        if (c == term) {
            break;
        }
    }
    string_t result = (string_t){ ptr, str->ptr - 1 };
    if (trim_ws) {
        trim(&result);
    }
    return result;
}

void render_block(FILE* out, string_t str) {
    uint32_t len = (uint32_t)(str.end - str.ptr);
    for (uint32_t i=0; i<len; i++) {
        char c = str.ptr[i];
        switch (c) {
            case '<': fprintf(out, "&lt;"); break;
            case '>': fprintf(out, "&gt;"); break;
            case '&': fprintf(out, "&amp;"); break;
            default: putc(c, out); break;
        }
    }
}

string_t read_file(const char* filename) {
    // open file for reading
    FILE* f = fopen(filename, "r");
    if (f == NULL) {
        fprintf(stderr, "File '%s' not found\n", filename);
        return (string_t){ NULL, NULL };
    }

    // find file size
    fseek(f, 0, SEEK_END);
    uint32_t len = (uint32_t)ftell(f);
    fseek(f, 0, SEEK_SET);

    // read the whole file
    string_t str;
    str.ptr = malloc(len);
    str.end = str.ptr + len;

    size_t read = fread(str.ptr, 1, len, f);
    if (read != len) {
        fprintf(stderr, "Error reading the file.\n");
        return (string_t){ NULL, NULL };
    }

    // close the file
    fclose(f);

    return str;
}

int compile(const char* in_file, const char* out_file, string_t css) {
    // input file
    string_t src = read_file(in_file);
    if (src.ptr == NULL) {
        return 1;
    }
    char* src_alloc = src.ptr;

    // output file
    FILE* out = fopen(out_file, "w");

    // parse date and title
    string_t date = advance_block(&src, '|', true);
    string_t title = advance_block(&src, '\n', true);

    // render the header
    fprintf(out, "<!DOCTYPE html><head>");
    fprintf(out, "<meta charset=\"utf-8\">");
    fprintf(out, "<title>");
    render_block(out, title);
    fprintf(out, "</title>");
    fprintf(out, "<style>");
    render_block(out, css);
    fprintf(out, "</style>");
    fprintf(out, "</head><body>");
    // title and date
    fprintf(out, "<h1>");
    render_block(out, title);
    fprintf(out, "</h1>");
    fprintf(out, "<p class=\"subt\">");
    render_block(out, date);
    fprintf(out, "</p>");

    // body
    while (src.ptr < src.end) {
        char c = *src.ptr++;
        switch (c) {
            case '\n':
                break;
            case ' ': {
                string_t line = advance_block(&src, '\n', true);
                fprintf(out, "<p>");
                render_block(out, line);
                fprintf(out, "</p>");
                break;
            }
            case '#': {
                string_t line = advance_block(&src, '\n', true);
                fprintf(out, "<h2>");
                render_block(out, line);
                fprintf(out, "</h2>");
                break;
            }
            case '!': {
                string_t link = advance_block(&src, '|', true);
                string_t label = advance_block(&src, '\n', true);
                fprintf(out, "<p><a href=\"");
                render_block(out, link);
                fprintf(out, "\">");
                render_block(out, label);
                fprintf(out, "</a></p>");
                break;
            }
            case '-': {
                fprintf(out, "<ul>");
                while (c == '-') {
                    string_t line = advance_block(&src, '\n', true);
                    fprintf(out, "<li>");
                    render_block(out, line);
                    fprintf(out, "</li>");
                    if (src.ptr < src.end && *src.ptr == '-') {
                        c = *src.ptr++;
                    } else {
                        c = ' ';
                    }
                }
                fprintf(out, "</ul>");
                break;
            }
            case '`': {
                fprintf(out, "<pre>");
                while (c == '`') {
                    string_t line = advance_block(&src, '\n', false);
                    render_block(out, line);
                    fprintf(out, "\n");
                    if (src.ptr < src.end && *src.ptr == '`') {
                        c = *src.ptr++;
                    } else {
                        c = ' ';
                    }
                }
                fprintf(out, "</pre>");
                break;
            }
            default:
                fprintf(stderr, "Unexpected token '%c' in file '%s'", c, in_file);
                return 1;
        }
    }
    fprintf(out, "<p class=\"back\"><a href=\"index.html\">&larr; Back to index</a></p>");
    fprintf(out, "</body></html>");

    free(src_alloc);

    return 0;
}

int filter_txt(const struct dirent* ent) {
    const char* ext = strrchr(ent->d_name, '.');
    if (ext != NULL && strcmp(ext, ".txt") == 0) {
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 4) { return 1; }

    const char* in_path = argv[1];
    const char* out_path = argv[2];
    const char* css_file = argv[3];

    string_t css = read_file(css_file);
    if (css.ptr == NULL) {
        return 1;
    }

    struct dirent** filelist;
    int num_files = scandir(in_path, &filelist, filter_txt, alphasort);
    if (num_files == -1) {
        fprintf(stderr, "Input path '%s' not found", in_path);
        return 1;
    }

    char in_file[1024];
    char out_file[1024];

    for (uint32_t i=0; i<num_files; i++) {
        printf("Processing %s ...\n", filelist[i]->d_name);
        sprintf(in_file, "%s/%s", in_path, filelist[i]->d_name);
        sprintf(out_file, "%s/%.*shtml", out_path, (int)strlen(filelist[i]->d_name) - 3, filelist[i]->d_name);
        compile(in_file, out_file, css);
    }

    printf("Creating index.html ...\n");
    {
        sprintf(out_file, "%s/index.html", out_path);
        FILE* out = fopen(out_file, "w");
        fprintf(out, "<!DOCTYPE html><head>");
        fprintf(out, "<meta charset=\"utf-8\">");
        fprintf(out, "<title>bareware.dev</title>");
        fprintf(out, "<style>");
        render_block(out, css);
        fprintf(out, "</style>");
        fprintf(out, "</head><body>");
        fprintf(out, "<h1>bareware.dev</h1>");
        fprintf(out, "<p class=\"subt\">Engineering without abstraction layers between you and the machine!</p>");
        for (int32_t i=num_files-1; i>=0; i--) {
            sprintf(in_file, "%s/%s", in_path, filelist[i]->d_name);
            sprintf(out_file, "%.*shtml", (int)strlen(filelist[i]->d_name) - 3, filelist[i]->d_name);

            string_t src = read_file(in_file);
            char* src_alloc = src.ptr;
            string_t date = advance_block(&src, '|', true);
            string_t title = advance_block(&src, '\n', true);

            fprintf(out, "<p><a href=\"%s\">", out_file);
            render_block(out, date);
            fprintf(out, " - ");
            render_block(out, title);
            fprintf(out, "</a></p>");

            free(src_alloc);
        }
        fprintf(out, "</body></html>");
        fclose(out);
    }

    // free filelist
    for (uint32_t i=0; i<num_files; i++) {
        free(filelist[i]);
    }
    free(filelist);

    free(css.ptr);
}
