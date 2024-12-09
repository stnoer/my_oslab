# include "../include/newfs.h" 

extern struct custom_options newfs_options;			 /* 全局选项 */
extern struct newfs_super super; 


/**
 * @brief 驱动读
 * 
 * @param offset 
 * @param out_content 
 * @param size 
 * @return int 
 */
int my_read(int offset, uint8_t *out_content, int size) {
    int      offset_aligned = NEWFS_ROUND_DOWN(offset, NEWFS_BLKS_SZ);
    int      bias           = offset - offset_aligned;
    int      size_aligned   = NEWFS_ROUND_UP((size + bias), NEWFS_BLKS_SZ);
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    // lseek(NEWFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(NEWFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // read(NEWFS_DRIVER(), cur, NEWFS_IO_SZ());
        ddriver_read(NEWFS_DRIVER(), (char*)cur, NEWFS_IO_SZ());
        cur          += NEWFS_IO_SZ();
        size_aligned -= NEWFS_IO_SZ();   
    }
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return NEWFS_ERROR_NONE;
}

/**
 * @brief 驱动写
 * 
 * @param offset 
 * @param in_content 
 * @param size 
 * @return int 
 */
int my_write(int offset, uint8_t *in_content, int size) {
    int      offset_aligned = NEWFS_ROUND_DOWN(offset, NEWFS_BLKS_SZ);
    int      bias           = offset - offset_aligned;
    int      size_aligned   = NEWFS_ROUND_UP((size + bias), NEWFS_BLKS_SZ);
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned+1);
    if (in_content == NULL) {
    fprintf(stderr, "Error: in_content is NULL\n");
    free(temp_content);
    return -1;
    }
    uint8_t* cur            = temp_content;
    my_read(offset_aligned, temp_content, size_aligned);
    memcpy(temp_content + bias, in_content, size);
    
    // lseek(NEWFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(NEWFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // write(NEWFS_DRIVER(), cur, NEWFS_IO_SZ());
        if(!ddriver_write(NEWFS_DRIVER(), (char*)cur, NEWFS_IO_SZ()))
            printf("ddriver write error!!!");
        cur          += NEWFS_IO_SZ();
        size_aligned -= NEWFS_IO_SZ();   
    }

    free(temp_content);
    return NEWFS_ERROR_NONE;
}

/**
 * @brief 为dentry分配一个inode，占用位图
 * 
 * @param dentry 该dentry指向分配的inode
 * @return newfs_inode
 */
struct newfs_inode* newfs_alloc_inode(struct newfs_dentry * dentry) {
    struct newfs_inode* inode;
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;

    boolean is_find_free_entry = FALSE;
    boolean is_find_enough_free_data_bit = FALSE;
    int find_the_free_data_bit_num = 0 ;

    /* 检查位图是否有空位 */
    for (byte_cursor = 0; byte_cursor < NEWFS_BLKS_SZ_CAL(super.map_inode_blks); byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                super.map_inode[byte_cursor] |= (0x1 << bit_cursor);/* 当前ino_cursor位置空闲 */
                is_find_free_entry = TRUE;           
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }
    
    //无空位返回无空闲空间错误
    if (!is_find_free_entry || ino_cursor == super.max_ino)
        return (struct newfs_inode*)(-NEWFS_ERROR_NOSPACE);

    //初始化inode
    inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));
    inode->ino  = ino_cursor; 
    inode->size = 0;

    //查看数据位图中是否有六个bit
    for (byte_cursor = 0 ; byte_cursor < (NEWFS_BLKS_SZ_CAL(super.map_data_blks));byte_cursor++){
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {  
            if((super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                find_the_free_data_bit_num ++;
                if(find_the_free_data_bit_num == NEWFS_DATA_PER_FILE && byte_cursor * UINT8_BITS + bit_cursor < DATA_BLOCK_NUM){   //找满六个空块
                    is_find_enough_free_data_bit = TRUE;
                    break;
                }
            }
        }
        if (is_find_enough_free_data_bit) {
            break;
        }
    }

        //是否有充足的空闲空间
    if (!is_find_enough_free_data_bit)
        return (struct newfs_inode*)(-NEWFS_ERROR_NOBIT);

    /* dentry指向inode */
    dentry->inode = inode;
    dentry->ino   = inode->ino;
    /* inode指回dentry */
    inode->dentry = dentry;
    
    inode->dir_cnt = 0;
    inode->dentrys = NULL;
    
    // 如果是文件还需要申请数据块
    if (NEWFS_IS_REG(inode)) {
        for(int i = 0 ; i < NEWFS_DATA_PER_FILE ; i++ ){
            inode->block_pointer[i] = (uint8_t*)malloc(NEWFS_BLKS_SZ);
        }
    }

    return inode;
}


/**
 * @brief 
 * 
 * @param inode 
 * @param dir [0...]
 * @return struct newfs_dentry* 
 */
struct newfs_dentry* newfs_get_dentry(struct newfs_inode * inode, int dir) {
    struct newfs_dentry* dentry_cursor = inode->dentrys;
    int    cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt) {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}

/**
 * @brief 将denry插入到inode中，采用头插法
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int newfs_alloc_dentry(struct newfs_inode* inode, struct newfs_dentry* dentry) {
    if (inode->dentrys == NULL) {
        inode->dentrys = dentry;
        

    }
    else {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    int is_find_free_entry;
    int ino_cursor = 0;
    for (int byte_cursor = 0; byte_cursor < NEWFS_BLKS_SZ_CAL(super.map_data_blks); byte_cursor++){
        for (int bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                /* 当前ino_cursor位置空闲 */
                int i=0;
                while (inode->block_isUsed[i] == TRUE && i < NEWFS_DATA_PER_FILE)
                    i++;
                if(inode->block_isUsed[i] == FALSE){
                    super.map_data[byte_cursor] |= (0x1 << bit_cursor);                        
                    inode->block_index[i] = byte_cursor * UINT8_BITS + bit_cursor;  //记录数据块偏移的位数
                    inode->block_isUsed[i] = TRUE;
                    is_find_free_entry = TRUE;      //find the free inode        
                    break;
                }
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }


    if (!is_find_free_entry || ino_cursor == super.data_num)
        return (-NEWFS_ERROR_NOSPACE);


    inode->dir_cnt++;
    return inode->dir_cnt;
}
/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 * 
 * @param inode 
 * @return int 
 */
int newfs_sync_inode(struct newfs_inode * inode) {
    struct newfs_inode_d  inode_d;
    struct newfs_dentry*  dentry_cursor;
    struct newfs_dentry_d dentry_d;

    //维护in_dish inode
    int ino             = inode->ino;
    inode_d.ino         = ino;
    inode_d.size        = inode->size;
    //memcpy(inode_d.target_path, inode->target_path, MAX_NAME_LEN);
    inode_d.ftype       = inode->dentry->ftype;
    inode_d.dir_cnt     = inode->dir_cnt;
    

    for (int i = 0 ; i < NEWFS_DATA_PER_FILE ; i++ ){
        inode_d.block_isUsed[i] = inode->block_isUsed[i];
        inode_d.block_index[i] = inode->block_index[i];
    }

//v
    /* 先写inode本身 */
    if (my_write(NEWFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                     sizeof(struct newfs_inode_d)) != NEWFS_ERROR_NONE) {
        do { printf("SFS_DBG:%s io error\n",  __func__); } while(0);
        return -NEWFS_ERROR_IO;
    }

    /* 再写inode下方的数据 */
    if (NEWFS_IS_DIR(inode)) { /* 如果当前inode是目录，那么数据是目录项，且目录项的inode也要写回 */                          
        int counter = 0;   
        dentry_cursor = inode->dentrys;
        while (dentry_cursor != NULL && counter <  NEWFS_DATA_PER_FILE)
        {
            if(inode->block_isUsed[counter]){
                int offset = NEWFS_DATA_OFS(inode->block_index[counter]);
                //counter++;
                while(dentry_cursor!=NULL){     //将目录写回磁盘
                    memcpy(dentry_d.fname, dentry_cursor->name, MAX_NAME_LEN);
                    dentry_d.ftype = dentry_cursor->ftype;
                    dentry_d.ino = dentry_cursor->ino;
                    printf("%s:offset:%d\n",dentry_d.fname,offset);
                    if (my_write(offset, (uint8_t *)&dentry_d, sizeof(struct newfs_dentry_d)) != NEWFS_ERROR_NONE) {
                        NEWFS_DBG("[%s] io error\n", __func__);
                        return -NEWFS_ERROR_IO;                     
                    }

                    if(dentry_cursor->inode != NULL){  //将当前目录下的索引节点继续写回磁盘中
                        newfs_sync_inode(dentry_cursor->inode);
                    }

                    dentry_cursor = dentry_cursor->brother;
                    offset += sizeof(struct newfs_dentry_d);
                }
                counter ++ ;
            }
        }
    }
    else if (NEWFS_IS_REG(inode)) { /* 如果当前inode是文件，那么数据是文件内容，直接写即可 */
        for(int i = 0 ; i < NEWFS_DATA_PER_FILE && inode->block_isUsed[i]; i++ ){
            if (my_write(NEWFS_DATA_OFS(inode->block_index[i]), inode->block_pointer[i], 
                                NEWFS_BLKS_SZ) != NEWFS_ERROR_NONE) {
                do { printf("SFS_DBG:%s io error\n",  __func__); } while(0);
                return -NEWFS_ERROR_IO;
            }
        }
    }
    return NEWFS_ERROR_NONE;
}


/**
 * @brief
 * 
 * @param dentry 从磁盘中读取一个 inode 的信息
 * 根据目录项 dentry 和 inode 编号加载指定的 inode 及其相关数据或目录结构
 * @param ino inode唯一编号
 * @return struct newfs_inode* 
 */
struct newfs_inode* newfs_read_inode(struct newfs_dentry * dentry, int ino) {
    struct newfs_inode* inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));
    struct newfs_inode_d inode_d;
    struct newfs_dentry* sub_dentry;
    struct newfs_dentry_d dentry_d;
    int    dir_cnt = 0;
    /* 从磁盘读索引结点 */
    if (my_read(NEWFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                        sizeof(struct newfs_inode_d)) != NEWFS_ERROR_NONE) {
        do { printf("SFS_DBG:%s io error\n",  __func__); } while(0);
        return NULL;                    
    }
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    //memcpy(inode->target_path, inode_d.target_path, MAX_NAME_LEN);
    inode->dentry = dentry;
    inode->dentrys = NULL;
    inode->dir_cnt = inode_d.dir_cnt;

    for(int i = 0 ; i < NEWFS_DATA_PER_FILE ; i++){
        inode->block_isUsed[i] = inode_d.block_isUsed[i];
        inode->block_index[i] = inode_d.block_index[i];
    }

    /* 内存中的inode的数据或子目录项部分也需要读出 */

    if (NEWFS_IS_DIR(inode)) {
        dir_cnt = inode_d.dir_cnt;
        int counter = 0 ;
        int offset;
        while(dir_cnt != 0 )
        {
            if(inode->block_isUsed[counter]){
                offset = NEWFS_DATA_OFS(inode->block_index[counter]);
                while( dir_cnt > 0 ){
                    if (my_read(offset, (uint8_t *)&dentry_d, sizeof(struct newfs_dentry_d)) != NEWFS_ERROR_NONE) {
                        NEWFS_DBG("[%s] io error\n", __func__);
                        return NULL;                    
                    }
                    sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
                    sub_dentry->parent = inode->dentry;
                    sub_dentry->ino    = dentry_d.ino; 
                    newfs_alloc_dentry(inode, sub_dentry);

                    offset += sizeof(struct newfs_dentry_d);
                    dir_cnt -- ;
                }
                counter++;
            }
        }
    }
    else if (NEWFS_IS_REG(inode)) {
        for(int i = 0 ; i < NEWFS_DATA_PER_FILE && inode->block_isUsed[i]; i++){
            inode->block_pointer[i] = (uint8_t *)malloc(NEWFS_BLKS_SZ);
            if (my_read(NEWFS_DATA_OFS(inode->block_index[i]), inode->block_pointer[i], 
                            NEWFS_BLKS_SZ) != NEWFS_ERROR_NONE) {
            NEWFS_DBG("[%s] io error\n", __func__);
            return NULL;                    
            }
        }
    }
    return inode;
}

/**
 * @brief 获取文件名
 * 
 * @param path 
 * @return char* 
 */
char* newfs_get_fname(const char* path) {
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}

/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path 
 * @return int 
 */
int newfs_calc_lvl(const char * path) {
    char* str = path;
    int   lvl = 0;
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    while (*str != NULL) {
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
}
/**
 * @brief 查找文件或目录
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry 
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 *  
 * 
 * 如果能查找到，返回该目录项
 * 如果查找不到，返回的是上一个有效的路径
 * 
 * path: /a/b/c
 *      1) find /'s inode     lvl = 1
 *      2) find a's dentry 
 *      3) find a's inode     lvl = 2
 *      4) find b's dentry    如果此时找不到了，is_find=FALSE且返回的是a的inode对应的dentry
 * 
 * @param path 
 * @return struct newfs_dentry* 
 */
struct newfs_dentry* newfs_lookup(const char * path, boolean* is_find, boolean* is_root) {
    struct newfs_dentry* dentry_cursor = super.root_dentry;
    struct newfs_dentry* dentry_ret = NULL;
    struct newfs_inode*  inode; 
    int   total_lvl = newfs_calc_lvl(path);
    int   lvl = 0;
    boolean is_hit;
    char* fname = NULL;
    char* path_cpy = (char*)malloc(sizeof(path)+1);
    *is_root = FALSE;
    strcpy(path_cpy, path);

    if (total_lvl == 0) {                           /* 根目录 */
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = super.root_dentry;
    }
    fname = strtok(path_cpy, "/");       
    while (fname)
    {   
        lvl++;
        if (dentry_cursor->inode == NULL) {           /* Cache机制 */
            newfs_read_inode(dentry_cursor, dentry_cursor->ino);
        }
        inode = dentry_cursor->inode;
        if (NEWFS_IS_REG(inode) && lvl < total_lvl) {
            do { printf("NEWFS_DBG:%s not a dird\n",  __func__); } while(0);
            dentry_ret = inode->dentry;
            break;
        }
        if (NEWFS_IS_DIR(inode)) {
            dentry_cursor = inode->dentrys;
            is_hit        = FALSE;
            while (dentry_cursor)   /* 遍历子目录项 */
            {
                if (memcmp(dentry_cursor->name, fname, strlen(fname)) == 0) {
                    is_hit = TRUE;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;
            }
            if (!is_hit) {
                *is_find = FALSE;
                 do { printf("NEWFS_DBG:%s not found\n",  __func__); } while(0);
                dentry_ret = inode->dentry;
                break;
            }

            if (is_hit && lvl == total_lvl) {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/"); 
    }
    if (dentry_ret->inode == NULL) {
        dentry_ret->inode = newfs_read_inode(dentry_ret, dentry_ret->ino);
    }
    //printf("NEWFS_DBG: returning dentry = %p\n", dentry_ret);
    return (struct newfs_dentry*)((uintptr_t)dentry_ret);
}


/**
 * @brief 挂载newfs, Layout 如下
 * 
 * Layout
 * | Super | Inode Map| Data Map | Data |
 * 
 * 2 * IO_SZ = BLK_SZ
 * 
 * 每个Inode占用一个Blk
 * @param options 
 * @return int 
 */
int my_mount(struct custom_options options){

    // 定义内存中的数据结构
    int                 ret = NEWFS_ERROR_NONE;
    int                 driver_fd;
    struct newfs_super_d  newfs_super_d; 
    struct newfs_dentry*  root_dentry;
    struct newfs_inode*   root_inode;

    int                 inode_num;
    int                 data_num;
    int                 map_inode_blks;//索引位图占用的位数
    int                 map_data_blks;//数据位图占用的位数
    
    int                 super_blks;
    boolean             is_init = FALSE;

    super.is_mounted = FALSE;

    // driver_fd = open(options.device, O_RDWR);
    driver_fd = ddriver_open(options.device);

    if (driver_fd < 0) {
        return driver_fd;
    }
    super.fd = driver_fd;
    ddriver_ioctl(NEWFS_DRIVER(), IOC_REQ_DEVICE_SIZE,  &super.sz_disk);
    ddriver_ioctl(NEWFS_DRIVER(), IOC_REQ_DEVICE_IO_SZ, &super.sz_io);
    
    super.sz_blk = super.sz_io * 2 ;
    root_dentry = new_dentry("/", NEWFS_DIR);     /* 根目录项每次挂载时新建 */
    if (my_read(NEWFS_SUPER_OFS, (uint8_t *)(&newfs_super_d), 
                        sizeof(struct newfs_super_d)) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }   
    
    //估算磁盘布局信息                                 /* 读取super */
    if (newfs_super_d.magic != NEWFS_MAGIC) {     /* 幻数不正确，初始化 */
                                                      /* 估算各部分大小 */
        super_blks = SUPER_BLOCK_NUM;          //超级块：1
        inode_num = INODE_BLOCK_NUM;           //索引节点块数：512
        data_num = DATA_BLOCK_NUM;             //数据块个数：3072
        map_inode_blks = INODE_MAP_BLOCK_NUM;  //索引节点位图：1
        map_data_blks = DATA_MAP_BLOCK_NUM;    //数据块位图：1

         /* 布局layout */
        super.max_ino = inode_num; 
        super.data_num = data_num ;
        super.magic = NEWFS_MAGIC;
        newfs_super_d.sb_offset = 0;
        newfs_super_d.sb_blks = 1;
        
        newfs_super_d.map_inode_offset = NEWFS_SUPER_OFS + NEWFS_BLKS_SZ_CAL(super_blks);
        newfs_super_d.map_inode_blks  = map_inode_blks;
        
        newfs_super_d.map_data_offset = NEWFS_SUPER_OFS + NEWFS_BLKS_SZ_CAL(super_blks + map_inode_blks);
        newfs_super_d.map_data_blks  = map_data_blks;

        newfs_super_d.inode_offset = newfs_super_d.map_data_offset + NEWFS_BLKS_SZ_CAL(map_data_blks);          //索引节点
        newfs_super_d.data_offset = newfs_super_d.inode_offset + NEWFS_BLKS_SZ_CAL(inode_num);
        newfs_super_d.sz_usage    = 0;
       // NEWFS_DBG("inode map blocks: %d\n", map_inode_blks);
        is_init = TRUE;
    }

    //直接读取填充磁盘布局信息
    super.sz_usage   = newfs_super_d.sz_usage;      /* 建立 in-memory 结构 */
    
    //分配索引位图
    super.map_inode = (uint8_t *)malloc(NEWFS_BLKS_SZ_CAL(newfs_super_d.map_inode_blks));//预先读取索引位图
    super.map_inode_blks = newfs_super_d.map_inode_blks;
    super.map_inode_offset = newfs_super_d.map_inode_offset;

    //分配数据位图
    super.map_data = (uint8_t *)malloc(NEWFS_BLKS_SZ_CAL(newfs_super_d.map_data_blks));//预先读取索引位图
    super.map_data_blks = newfs_super_d.map_data_blks;
    super.map_data_offset = newfs_super_d.map_data_offset;
    
    super.inode_offset = newfs_super_d.inode_offset;
    super.data_offset = newfs_super_d.data_offset;

    

    //预先读取索引位图
    if (my_read(newfs_super_d.map_inode_offset, (uint8_t *)(super.map_inode), 
                        NEWFS_BLKS_SZ_CAL(newfs_super_d.map_inode_blks)) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }

    //预先读取数据位图
    if (my_read(newfs_super_d.map_data_offset, (uint8_t *)(super.map_data), 
                        NEWFS_BLKS_SZ_CAL(newfs_super_d.map_data_blks)) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }

    if (is_init) {                                    /* 分配根节点 */
        root_inode = newfs_alloc_inode(root_dentry);
        newfs_sync_inode(root_inode);
    }
    //维护根目录
    root_inode            = newfs_read_inode(root_dentry, NEWFS_ROOT_INO);  /* 读取根目录 */
    root_dentry->inode    = root_inode;
    super.root_dentry = root_dentry;
    super.is_mounted  = TRUE;

   
    return ret;
}
/**
 * @brief 挂载newfs
 * 
 * @return int 
 */
int my_umount() {
    struct newfs_super_d  newfs_super_d; 

    if (!super.is_mounted) {
        return NEWFS_ERROR_NONE;
    }


    newfs_sync_inode(super.root_dentry->inode);     /* 从根节点向下刷写节点 */
                                                    
    newfs_super_d.magic               = NEWFS_MAGIC;
    newfs_super_d.map_inode_blks      = super.map_inode_blks;
    newfs_super_d.map_inode_offset    = super.map_inode_offset;
    newfs_super_d.map_data_blks       = super.map_data_blks;
    newfs_super_d.map_data_offset     = super.map_data_offset;
    newfs_super_d.inode_offset        = super.inode_offset;
    newfs_super_d.data_offset         = super.data_offset;
    newfs_super_d.max_ino             = super.max_ino;
    newfs_super_d.sz_usage            = super.sz_usage;

    //刷写超级块
    if (my_write(NEWFS_SUPER_OFS, (uint8_t *)&newfs_super_d, 
                     sizeof(struct newfs_super_d)) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }

    //刷写索引位图
    if (my_write(newfs_super_d.map_inode_offset, (uint8_t *)(super.map_inode), 
                         NEWFS_BLKS_SZ_CAL(newfs_super_d.map_inode_blks)) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }

    //刷写数据位图
    if (my_write(newfs_super_d.map_data_offset, (uint8_t *)(super.map_data), 
                         NEWFS_BLKS_SZ_CAL(newfs_super_d.map_data_blks)) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }


    free(super.map_inode);
    free(super.map_data);
    ddriver_close(NEWFS_DRIVER());//关闭打开的磁盘

    return NEWFS_ERROR_NONE;
}