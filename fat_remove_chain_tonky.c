void fat_remove_chain(cluster_t clst, cluster_t pclst) {
    //! Indexed and Extensible Files
    /* TODO: Your code goes here. */
    // printf("\n---fat_remove_chain---\n");
    // printf("target clst :: %x\n", clst);
    // printf("target pclst :: %x\n", clst);
    // todo 0 조건은 없는게 맞음...
    while (fat_fs->fat[clst] != EOChain && fat_fs->fat[clst] != 0) {
        // printf("deleted clst :: %x\n", clst);
        cluster_t temp = fat_fs->fat[clst];
        fat_fs->fat[clst] = 0;
        clst = temp;
    }
    // printf("deleted clst :: %x\n", clst);
    fat_fs->fat[clst] = 0;
    if (pclst)
        fat_fs->fat[clst] = EOChain;
}