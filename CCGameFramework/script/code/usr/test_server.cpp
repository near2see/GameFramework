#include "/include/io"
#include "/include/fs"
#include "/include/proc"
#include "/include/json"
#include "/include/memory"
struct string {
    char* text;
    int capacity;
    int length;
};
string new_string() {
    string s;
    s.text = malloc(16);
    s.capacity = 16;
    s.length = 0;
    return s;
}
void append_char(string* s, char c) {
    if (s->length >= s->capacity - 1) {
        s->capacity <<= 1;
        char* new_text = malloc(s->capacity);
        strcpy(new_text, s->text);
        free(s->text);
        s->text = new_text;
    }
    (s->text)[s->length++] = c;
    (s->text)[s->length] = 0;
}
void write_string(int handle, char* s) {
    put_string(s);
    while (*s) { write(handle, *s++); }
}
int read_file(int handle) {
    int c;
    for (;;) {
        string s = new_string();
        put_string(">> IN:  ");
        while (c = read(handle), c < 0x1000) {
            append_char(&s, c);
        }
        if (c == 0x2001) break;
        put_string(s.text);
        put_string("\n");
        put_string("<< OUT: ");
        json_object* obj = json_parse_obj(s.text);
        if (obj) {
            char* url = json_obj_get_string(obj, "url")->data.str;
            if (strlen(url) == 1) url = "/index.html";
            char* local = malloc(strlen(url) + 10);
            strcpy(local, "/www");
            strcat(local, url);
            put_string(local);
            put_string(" ");
            int f = open(local);
            free(local);
            if (f < 0) {
                write_string(handle, "File not found.");
                truncate(handle);
            }
            else {
                load(f);
                if (copy(f, handle) != 0) {
                    write_string(handle, "Copy failed.");
                    truncate(handle);
                }
                else {
                    put_string("[Local file]");
                }
                close(f);
            }
            free(obj);
        }
        else {
            write_string(handle, "Parsing json failed.");
            truncate(handle);
        }
        put_string("\n");
        free(s.text);
    }
    switch (c) {
    case 0x2000:
        // put_string("[INFO] Read to the end.");
        break;
    case 0x2001:
        set_fg(240, 0, 0);
        put_string("[ERROR] Read error.");
        restore_fg();
        break;
    }
    close(handle);
    newline();
}
int main(int argc, char **argv) {
    put_string("========== [#21 TEST SERVER] ==========\n");
    int handle = open("/server/8888");
    switch (handle) {
    default:
        // put_string("[INFO] Success.");
        read_file(handle);
        break;
    case -1:
        set_fg(240, 0, 0);
        put_string("[ERROR] File not exists.");
        restore_fg();
        break;
    case -2:
        set_fg(240, 0, 0);
        put_string("[ERROR] Path is not file.");
        restore_fg();
        break;
    case -3:
        set_fg(240, 0, 0);
        put_string("[ERROR] File is locked.");
        restore_fg();
        break;
    }
    put_string("========== [#21 TEST SERVER] ==========\n");
    return 0;
}