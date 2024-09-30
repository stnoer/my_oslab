#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// char *fmtname(char *path) {
//   static char buf[DIRSIZ + 1];
//   char *p;

//   // Find first character after last slash.
//   for (p = path + strlen(path); p >= path && *p != '/'; p--);
//   p++;

//   // Return blank-padded name.
//   if (strlen(p) >= DIRSIZ) return p;
//   memmove(buf, p, strlen(p));
//   //  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
//   char *s = buf + strlen(p);
//   *s = '\0';
//   return buf;
// }

void printOut(const char *path, const char *name) { printf("%s\n", path); }

void find(const char *path, const char *name) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find：cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch (st.type) {
    case T_DIR:
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf("find: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      int n;
      while ((n = read(fd, &de, sizeof(de))) == sizeof(de)) {
        // printf("%d,%d,%d\n",de.inum,strlen(de.name),n);
        if (strlen(de.name) == 0) break;
        if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) continue;

        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0) {
          printf("find: cannot stat %s\n", buf);
          continue;
        }

        if (strcmp(de.name, name) == 0) {
          printOut(buf, name);
        }
        int new_fd = open(buf, 0);  // 以只读方式打开新目录

        find(buf, name);  // 递归查找
        close(new_fd);    // 关闭目录
      }
      break;
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if (argc == 3) {
    find(argv[1], argv[2]);
  } else
    fprintf(2, "wrong format! Enter the correct format like find <path> <name>");
}