#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ADD HEADER
void add_header(const char *filename, const char *header) {
    FILE *fp = fopen(filename, "r");

    char tempname[300];
    sprintf(tempname, "%s.tmp", filename);
    FILE *temp = fopen(tempname, "w");

    if (!fp || !temp) return;

    char line[256];

    // Write header first
    fprintf(temp, "%s\n", header);

    // Copy original content
    while (fgets(line, sizeof(line), fp)) {
        fputs(line, temp);
    }

    fclose(fp);
    fclose(temp);

    remove(filename);
    rename(tempname, filename);
}

// REPLACE WORD
void replace_word(const char *filename, const char *old, const char *new) {
    FILE *fp = fopen(filename, "r");

    char tempname[300];
    sprintf(tempname, "%s.tmp", filename);
    FILE *temp = fopen(tempname, "w");

    if (!fp || !temp) return;

    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        char buffer[512] = "";
        char *pos = line;

        // Replace ALL occurrences in line
        while ((pos = strstr(pos, old)) != NULL) {
            int index = pos - line;

            strncat(buffer, line, index);
            strcat(buffer, new);

            pos += strlen(old);
            strcpy(line, pos);
            pos = line;
        }

        strcat(buffer, line);
        fputs(buffer, temp);
    }

    fclose(fp);
    fclose(temp);

    remove(filename);
    rename(tempname, filename);
}

// DELETE LINE
void delete_line(const char *filename, const char *word) {
    FILE *fp = fopen(filename, "r");

    char tempname[300];
    sprintf(tempname, "%s.tmp", filename);
    FILE *temp = fopen(tempname, "w");

    if (!fp || !temp) return;

    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, word) == NULL) {
            fputs(line, temp);
        }
    }

    fclose(fp);
    fclose(temp);

    remove(filename);
    rename(tempname, filename);
}

// APPEND LINE
void append_line(const char *filename, const char *text) {
    FILE *fp = fopen(filename, "a");

    if (!fp) return;

    fprintf(fp, "%s\n", text);

    fclose(fp);
}
