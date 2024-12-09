#ifndef _TYPES_H_
#define _TYPES_H_

typedef int    boolean;
typedef uint16_t     flag16; 


#define MAX_NAME_LEN    128   

#define NEWFS_ERROR_NONE          0


typedef enum newfs_file_type {
    NEWFS_REG_FILE,//文件
    NEWFS_DIR,   //文件夹
    NEWFS_SYM_LINK
} NEWFS_FILE_TYPE;

struct newfs_dentry {
    char     name[MAX_NAME_LEN];
    uint32_t ino;

    struct newfs_dentry* parent;        /* 父亲Inode的dentry */
    struct newfs_dentry* brother;       /* 兄弟 */
    struct newfs_inode*  inode;         /* 指向inode */
    NEWFS_FILE_TYPE      ftype;
};

//Type def
#define FALSE	0


//Marco
#define TRUE                    1
#define FALSE                   0
#define UINT32_BITS             32
#define UINT8_BITS              8

#define SUPER_BLOCK_NUM         1
#define INODE_MAP_BLOCK_NUM     1
#define DATA_MAP_BLOCK_NUM      1
#define INODE_BLOCK_NUM         512
#define DATA_BLOCK_NUM          3072

#define NEWFS_SUPER_OFS           0     //超级快偏移量
#define NEWFS_ERROR_NONE          0     //操作执行成功时的返回值（无错误）
#define NEWFS_ERROR_IO            EIO   /* Error Input/Output */
#define NEWFS_ERROR_NOSPACE       ENOSPC //无空闲空间错误
#define NEWFS_ERROR_NOBIT         ENOSPC 
#define NEWFS_ERROR_ISDIR         EISDIR
#define NEWFS_ERROR_SEEK          ESPIPE 
#define NEWFS_ERROR_EXISTS        EEXIST //创建时存在
#define NEWFS_ERROR_UNSUPPORTED   ENXIO
#define NEWFS_ERROR_NOTFOUND      ENOENT

//#define NEWFS_MAGIC_NUM           0x12345678
#define NEWFS_SUPER_OFS           0    //超级块偏移数
#define NEWFS_ROOT_INO            0    //根目录inode

#define NEWFS_INODE_PER_FILE      1
#define NEWFS_DATA_PER_FILE       6
#define NEWFS_DEFAULT_PERM        0777

//Macro Function
#define NEWFS_DBG(fmt, ...) do { printf("SFS_DBG: " fmt, ##__VA_ARGS__); } while(0)
#define NEWFS_IO_SZ()                     (super.sz_io)
#define NEWFS_DISK_SZ()                   (super.sz_disk)
#define NEWFS_DRIVER()                    (super.fd)
#define NEWFS_BLKS_SZ_CAL(blk)                ((blk) * NEWFS_BLKS_SZ)

#define NEWFS_ROUND_DOWN(value, round)    ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))
#define NEWFS_ROUND_UP(value, round)      ((value) % (round) == 0 ? (value) : ((value) / (round) + 1) * (round))

#define NEWFS_BLKS_SZ                     (super.sz_blk)
#define NEWFS_ASSIGN_FNAME(pnewfs_dentry, _fname) memcpy(pnewfs_dentry->name, _fname, strlen(_fname))

//计算文件系统中 inode 的偏移量
#define NEWFS_INO_OFS(ino)                (super.inode_offset +  NEWFS_BLKS_SZ_CAL(ino))
//计算文件系统中数据块的偏移量
#define NEWFS_DATA_OFS(off)              (super.data_offset +  NEWFS_BLKS_SZ_CAL(off))


#define NEWFS_IS_DIR(pinode)              (pinode->dentry->ftype == NEWFS_DIR)
#define NEWFS_IS_REG(pinode)              (pinode->dentry->ftype == NEWFS_REG_FILE)



//内联函数创建新的目录项
static inline struct newfs_dentry* new_dentry(char * name, NEWFS_FILE_TYPE ftype) {
    struct newfs_dentry * dentry = (struct newfs_dentry *)malloc(sizeof(struct newfs_dentry));
    memset(dentry, 0, sizeof(struct newfs_dentry));
    NEWFS_ASSIGN_FNAME(dentry, name);
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL; 
    dentry->parent  = NULL;
    dentry->brother = NULL;  

    return dentry;                                          
}


struct custom_options {
	const char*        device;
};

struct newfs_super {
    uint32_t magic;
    int      fd;

    int sz_blk;
    int sz_io;              //磁盘IO大小
    int sz_disk;            //磁盘容量大小
    int sz_usage;

    /* 逻辑块信息 */
    int blks_nums;          // 逻辑块数

    /* 磁盘布局分区信息 */
    
    int max_ino;            // 最大支持inode数
    int data_num;
    uint8_t* map_inode;
    int map_inode_blks;     // 索引节点位图于磁盘中的块数
    int map_inode_offset;   // 索引节点位图于磁盘中的偏移
    
    uint8_t* map_data;
    int map_data_blks;     // 数据节点位图于磁盘中的块数
    int map_data_offset;   // 数据节点位图于磁盘中的偏移

    int data_offset;        //数据的偏移
    int inode_offset;

    boolean is_mounted;     //是否挂载

    struct newfs_dentry* root_dentry;
};



struct newfs_inode {
    uint32_t ino;
    char               target_path[MAX_NAME_LEN];
    int                size;                  /* 文件已占用空间 */
    int                dir_cnt;
    struct newfs_dentry* dentry;              /* 指向该inode的dentry */
    struct newfs_dentry* dentrys;             /* 所有目录项 */
    uint8_t*           block_pointer[6];
    int                block_isUsed[6];
    int                block_index[6];       //数据块在磁盘中的块号
    
};



/******************************************************************************
* SECTION: FS Specific Structure - Disk structure
*******************************************************************************/
struct newfs_super_d {
    uint32_t magic;
    int      fd;
    uint32_t sz_usage;
    /* TODO: Define yourself */

     /* 磁盘布局分区信息 */
    int sb_offset;          // 超级块于磁盘中的偏移，通常默认为0
    int sb_blks;            // 超级块于磁盘中的块数，通常默认为1

    uint32_t           max_ino;
    uint32_t           map_inode_blks;
    uint32_t           map_inode_offset;

    uint32_t           map_data_blks;
    uint32_t           map_data_offset;
    
    uint32_t           inode_offset;
    uint32_t           data_offset;
};

struct newfs_inode_d
{
    uint32_t           ino;                           /* 在inode位图中的下标 */
    uint32_t           size;                          /* 文件已占用空间 */
    uint32_t           dir_cnt;
    NEWFS_FILE_TYPE    ftype;   
    int block_index[6]; // 记录偏移（可固定分配）
    int block_isUsed[6];
    char               target_path[MAX_NAME_LEN];
};  

struct newfs_dentry_d
{
    char               fname[MAX_NAME_LEN];
    NEWFS_FILE_TYPE      ftype;
    uint32_t           ino;                           /* 指向的ino号 */
}; 

#endif /* _TYPES_H_ */