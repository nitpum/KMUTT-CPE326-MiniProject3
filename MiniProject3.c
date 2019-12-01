#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

#define ROOT_PATH "./observed"

/* fetch all directory and recursively add to watch's directory */
void add_directory(int *fd, char *current_path, char level_one[][255], int *lo_index, int parent)
{
    struct dirent *de;
    struct stat buffer;
    char path[255];

    /* add current path to watch's directory
     * watch for modify files, create delete move (rename) files and directories
     */
    inotify_add_watch(*fd, current_path, IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM);

    /* get all files and directories */
    DIR *dr = opendir(current_path);

    if (dr == NULL)
    {
        printf("Could not open directory: %s\n", current_path);
        return;
    }

    while ((de = readdir(dr)) != NULL)
    {
        /* set full path */
        sprintf(path, "%s/%s", current_path, de->d_name);
        /* check is path is directory, also no . (current) and .. (parent) */
        lstat(path, &buffer);
        if (S_ISDIR(buffer.st_mode) && strcmp(de->d_name, ".") && strcmp(de->d_name, ".."))
        {
            /* if path is directory, then fetch directory in this path and add to watch's directory */
            *lo_index += 1;
            if (!strcmp(current_path, ROOT_PATH))
            {
                /* case: directory of root */
                sprintf(level_one[*lo_index], "%s", de->d_name);
                add_directory(fd, path, level_one, lo_index, *lo_index);
            }
            else
            {
                sprintf(level_one[*lo_index], "%s", level_one[parent]);
                add_directory(fd, path, level_one, lo_index, parent);
            }
        }
    }

    closedir(dr);
}

int main()
{
    /* initialize stat counter (array size of 3 with initial value of 0) */
    int *file = (int *)calloc(3, sizeof(int));
    int *dir = (int *)calloc(3, sizeof(int));
    printf("READY!\n");

    while (1)
    {
        int length, i = 0, out = 0;
        int fd;
        char buffer[BUF_LEN];
        char level_one[255][255];
        int lo_index = 0;

        /* initialize inotify */
        fd = inotify_init();

        if (fd < 0)
        {
            perror("inotify_inits");
        }

        if (out == 1)
        {
            continue;
            (void)close(fd);
        }

        /* watch directory */
        add_directory(&fd, ROOT_PATH, level_one, &lo_index, -1);
        length = read(fd, buffer, BUF_LEN);

        if (length < 0)
        {
            perror("reads");
        }

        while (i < length)
        {
            /* event occur */
            struct inotify_event *event = (struct inotify_event *)&buffer[i];

            /* check type of event */
            if (event->wd > 1)
            {
                /* check directory modification by file inside directory */
                if (!strcmp(event->name, level_one[event->wd - 1]) && event->mask & IN_IGNORED)
                {
                    printf("The directory %s was deleted.\n", event->name);
                    dir[2]++;
                }
                else
                {
                    printf("The directory %s was modified.\n", level_one[event->wd - 1]);
                    dir[1]++;
                }
            }
            else if (event->mask & IN_CREATE)
            {
                if (event->mask & IN_ISDIR)
                {
                    printf("The directory %s was created.\n", event->name);
                    dir[0]++;
                }
                else
                {
                    printf("The file %s was created.\n", event->name);
                    file[0]++;
                }
            }
            else if (event->mask & IN_DELETE)
            {
                if (event->mask & IN_ISDIR)
                {
                    printf("The directory %s was deleted.\n", event->name);
                    dir[2]++;
                }
                else
                {
                    printf("The file %s was deleted.\n", event->name);
                    file[2]++;
                }
            }
            else if (event->mask & IN_MODIFY)
            {
                printf("The file %s was readed.\n", event->name);
                file[1]++;
            }
            else if (event->mask & IN_MOVED_FROM)
            {
                if (event->mask & IN_ISDIR)
                {
                    printf("The directory %s was modified.\n", event->name);
                    dir[1]++;
                }
                else
                {
                    printf("The file %s was readed.\n", event->name);
                    file[1]++;
                }
            }

            i += EVENT_SIZE + event->len;
        }

        /* show stat */
        printf("File: Create - %d \t Read - %d \t Delete - %d\n", file[0], file[1], file[2]);
        printf("Directory: Create - %d \t Update - %d \t Delete - %d\n\n", dir[0], dir[1], dir[2]);
        (void)close(fd);
    }

    exit(0);
}