#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int is_ascii_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return 0; 
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        if (ch > 127 || ch < 0) { 
            fclose(file);
            return 0;
        }
    }
    fclose(file);
    return 1;
}

long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

void get_permissions(const char *filename, char *perms) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        sprintf(perms, "%04o", st.st_mode & 0777);
    } else {
        strcpy(perms, "0000");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Kullanım: ./tarsau -b [dosyalar] -o [arsiv.sau] VEYA ./tarsau -a [arsiv.sau] [dizin]\n");
        return 1;
    }

    if (strcmp(argv[1], "-b") == 0) {
        char *input_files[32];
        int file_count = 0;
        char *output_file = "a.sau"; 
        long total_size = 0;
        const long MAX_SIZE = 200 * 1024 * 1024; 

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0) {
                if (i + 1 < argc) {
                    output_file = argv[i + 1];
                    i++;
                }
            } else {
                if (file_count >= 32) return 1;
                
                if (!is_ascii_file(argv[i])) { 
                    printf("%s giriş dosyasının formatı uyumsuzdur!\n", argv[i]);
                    return 1; 
                }

                long size = get_file_size(argv[i]);
                if (size == -1) return 1;
                total_size += size;

                if (total_size > MAX_SIZE) return 1;

                input_files[file_count++] = argv[i];
            }
        }

        char metadata[8192] = "";
        for (int i = 0; i < file_count; i++) {
            char perms[10];
            get_permissions(input_files[i], perms);
            long size = get_file_size(input_files[i]);
            
            char record[256];
            sprintf(record, "|%s,%s,%ld|", input_files[i], perms, size);
            strcat(metadata, record);
        }

        long section1_size = 10 + strlen(metadata);
        FILE *out = fopen(output_file, "w");
        if (!out) return 1;

        fprintf(out, "%010ld", section1_size);
        fprintf(out, "%s", metadata);

        for (int i = 0; i < file_count; i++) {
            FILE *in = fopen(input_files[i], "r");
            if (in) {
                int ch;
                while ((ch = fgetc(in)) != EOF) {
                    fputc(ch, out);
                }
                fclose(in);
            }
        }
        fclose(out);
        printf("Dosyalar birleştirildi.\n");
    } 
    else if (strcmp(argv[1], "-a") == 0) {
        if (argc < 3 || argc > 4) {
            printf("Arşiv dosyası uygunsuz veya bozuk!\n");
            return 1;
        }

        char *archive_name = argv[2];
        char *dir_name = (argc == 4) ? argv[3] : NULL;

        char *ext = strrchr(archive_name, '.');
        if (!ext || strcmp(ext, ".sau") != 0) {
            printf("Arşiv dosyası uygunsuz veya bozuk!\n");
            return 1;
        }

        FILE *in = fopen(archive_name, "r");
        if (!in) {
            printf("Arşiv dosyası uygunsuz veya bozuk!\n");
            return 1;
        }

        char size_buf[11] = {0};
        if (fread(size_buf, 1, 10, in) != 10) {
            printf("Arşiv dosyası uygunsuz veya bozuk!\n");
            fclose(in);
            return 1;
        }
        long section1_size = atol(size_buf);
        long meta_size = section1_size - 10;

        char *metadata = malloc(meta_size + 1);
        if (!metadata) {
            fclose(in);
            return 1;
        }
        fread(metadata, 1, meta_size, in);
        metadata[meta_size] = '\0';

        if (dir_name) {
            struct stat st = {0};
            if (stat(dir_name, &st) == -1) {
                mkdir(dir_name, 0700);
            }
            chdir(dir_name);
        }

        char *token = strtok(metadata, "|");
        while (token != NULL) {
            char fname[256];
            char perms[10];
            long fsize;
            if (sscanf(token, "%[^,],%[^,],%ld", fname, perms, &fsize) == 3) {
                FILE *out = fopen(fname, "w");
                if (out) {
                    for (long k = 0; k < fsize; k++) {
                        int ch = fgetc(in);
                        if (ch != EOF) fputc(ch, out);
                    }
                    fclose(out);
                    
                    long perm_val = strtol(perms, NULL, 8);
                    chmod(fname, perm_val);
                }
            }
            token = strtok(NULL, "|");
        }

        free(metadata);
        fclose(in);

        if (dir_name) {
            printf("%s dizininde dosyalar açıldı.\n", dir_name);
        } else {
            printf("Mevcut dizinde dosyalar açıldı.\n");
        }
    }

    return 0;
}