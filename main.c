
#include "euboea.h"

int main(int argc, char ** argv) {
    char * src;
    FILE * fp;
    size_t ssz;
    if (argc < 2) error("no given filename");
    fp = fopen(argv[1], "rb");
    ssz = 0;
    if (!fp) {
        perror("fopen");
        exit(0);
    }
    struct stat sbuf;
    stat(argv[1], &sbuf);
    if (S_ISDIR(sbuf.st_mode)) {
        fclose(fp);
        printf("Error: %s is a directory.\n", argv[1]);
        exit(0);
    }
    fseek(fp, 0, SEEK_END);
    ssz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    src = calloc(sizeof(char), ssz + 2);
    fread(src, sizeof(char), ssz, fp);
    fclose(fp);
    return execute(src, ssz);
}
