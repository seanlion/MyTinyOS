
/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
                                  struct supplemental_page_table *src UNUSED)
{
    struct hash_iterator i;
    struct page *page = NULL;
    struct page *new_page;
    struct frame *new_frame;
    struct container *new_con;
    struct container *con;
    hash_first(&i, &src->pages);
    while (hash_next(&i))
    {
        // printf("Copy 시작\n");
        page = (hash_entry(hash_cur(&i), struct page, hash_elem));
        new_page = malloc(sizeof(struct page));
        // memcpy(new_page, page, sizeof(page));
        // uninit_new(new_page, page->va, page->uninit.init, page->uninit.type, page->uninit.aux, page->uninit.page_initializer);
        new_page->va = page->va;
        new_page->operations = page->operations;
        new_page->file_len = page->file_len;
        new_page->writable = page->writable;
        new_page->file_info = page->file_info;
        if (page->frame)
        {
            new_frame = vm_get_frame();
            memcpy(new_frame->kva, page->frame->kva, PGSIZE);
            pml4_set_page(thread_current()->pml4, new_page->va, new_frame->kva, new_page->writable);
            new_page->frame = new_frame;
            list_push_back(&frame_table, &new_frame->frame_elem);
        }
        switch (VM_TYPE(page->operations->type))
        {
        case VM_UNINIT:
            new_page->uninit.init = page->uninit.init;
            new_page->uninit.type = page->uninit.type;
            new_page->uninit.page_initializer = page->uninit.page_initializer;
            new_page->uninit.aux = malloc(sizeof(struct container));
            new_con = (struct container *)new_page->uninit.aux;
            con = (struct container *)page->uninit.aux;
            if (con)
            {
                new_con->file = con->file;
                new_con->page_read_bytes = con->page_read_bytes;
                new_con->writable = con->writable;
                new_con->offset = con->offset;
                new_con->file_len = con->file;
            }
            break;
        case VM_ANON:
            new_page->anon.aux = malloc(sizeof(struct container));
            new_con = (struct container *)new_page->anon.aux;
            con = (struct container *)page->anon.aux;
            if (con)
            {
                new_con->file = con->file;
                new_con->page_read_bytes = con->page_read_bytes;
                new_con->writable = con->writable;
                new_con->offset = con->offset;
                new_con->file_len = con->file;
            }
            new_page->anon.sec_no = page->anon.sec_no;
            new_page->anon.page_read_bytes = page->anon.page_read_bytes;
            new_page->anon.is_swap_out = page->anon.is_swap_out;
            break;
        case VM_FILE:
            new_page->file.page_read_bytes = page->file.page_read_bytes;
            new_page->file.offset = page->file.offset;
        default:
            break;
        }

        spt_insert_page(dst, new_page);
        // printf("Copy 종료\n");
    }
    return true;
}