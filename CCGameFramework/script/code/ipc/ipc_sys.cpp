#include "/include/proc"
#include "/include/shell"
#include "/include/readfile"
#include "/include/string"
#include "/include/format"
#include "/include/io"
void show(char* cmd) {
    put_string(cmd);
    free(cmd);
}
void run(char* cmd) {
    put_string(cmd); put_char('\n');
    shell(cmd);
    free(cmd);
}
int main(int argc, char** argv) {
    shell("touch /fifo/ipc_service_sys");
    shell("mklink /ipc/service_sys /fifo/ipc_service_sys hide");
    put_string("service sys started\n");
    char* data; int len;
    for (;;) {
        if (recv_signal() == 9) break;
        if (readfile("/ipc/service_sys", &data, &len) != 0) {
            sleep(100);
            continue;
        }
        show(format("request come: %s\n", data));
        if (fork() == -1) {
            redirect_to_parent();
            char* uuid = data;
            char* path = format("/ipc/req_%s", uuid);
            show(format("reading: %s\n", path));
            if (readfile(path, &data, &len) == 0) {
                char* t = data;
                char* p = strchr(data, ' ');
                if (!p) {
                    p = " ";
                }
                *p = '\0';
                show(format("shell /bin/%s\n", p + 1));
                run(format("/bin/%s > /ipc/res_%s", p + 1, uuid));
                free(data);
            }
            show(format("request done: %s\n", uuid));
            free(uuid);
            free(path);
            exit(0);
        }
        free(data);
    }
    shell("rm /fifo/ipc_service_sys");
    shell("rm /ipc/service_sys");
    put_string("service sys stopped\n");
    return 0;
}