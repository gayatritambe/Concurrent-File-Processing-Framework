#ifndef FILE_OPS_H
#define FILE_OPS_H

void add_header(const char *filename, const char *header);
void append_line(const char *filename, const char *line);
void replace_word(const char *filename, const char *old, const char *new);
void delete_line(const char *filename, const char *keyword);

#endif
