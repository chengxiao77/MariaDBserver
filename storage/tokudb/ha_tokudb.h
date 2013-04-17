#ifdef USE_PRAGMA_INTERFACE
#pragma interface               /* gcc class implementation */
#endif

#include <db.h>

typedef struct st_tokudb_share {
    char *table_name;
    uint table_name_length, use_count;
    pthread_mutex_t mutex;
    THR_LOCK lock;

    ulonglong auto_ident;
    ulonglong last_auto_increment;
    ha_rows rows, org_rows;
    ulong *rec_per_key;
    DB *status_block;
    //
    // DB that is indexed on the primary key
    //
    DB *file;
    //
    // array of all DB's that make up table, includes DB that
    // is indexed on the primary key
    //
    DB *key_file[MAX_KEY];
    u_int32_t *key_type;
    uint status, version;
    uint ref_length;
    bool fixed_length_primary_key, fixed_length_row;

} TOKUDB_SHARE;

typedef struct st_prim_key_part_info {
    uint offset;
    uint part_index;
} PRIM_KEY_PART_INFO;

class ha_tokudb : public handler {
private:
    THR_LOCK_DATA lock;         ///< MySQL lock
    TOKUDB_SHARE *share;        ///< Shared lock info

    //
    // last key returned by ha_tokudb's cursor
    //
    DBT last_key;
    //
    // current row pointed to by ha_tokudb's cursor
    // TODO: make sure current_row gets set properly
    //
    DBT current_row;
    //
    // pointer used for multi_alloc of key_buff, key_buff2, primary_key_buff
    //
    void *alloc_ptr;
    //
    // buffer used to temporarily store a "packed row" 
    // data pointer of a DBT will end up pointing to this
    // see pack_row for usage
    //
    uchar *rec_buff;
    //
    // number of bytes allocated in rec_buff
    //
    ulong alloced_rec_buff_length;
    //
    // buffer used to temporarily store a "packed key" 
    // data pointer of a DBT will end up pointing to this
    //
    uchar *key_buff; 
    //
    // buffer used to temporarily store a "packed key" 
    // data pointer of a DBT will end up pointing to this
    // This is used in functions that require the packing
    // of more than one key
    //
    uchar *key_buff2; 
    //
    // buffer used to temporarily store a "packed key" 
    // data pointer of a DBT will end up pointing to this
    // currently this is only used for a primary key in
    // the function update_row, hence the name. It 
    // does not carry any state throughout the class.
    //
    uchar *primary_key_buff;

    //
    // transaction used by ha_tokudb's cursor
    //
    DB_TXN *transaction;

    //
    // instance of cursor being used for init_xxx and rnd_xxx functions
    //
    DBC *cursor;
    //
    // flags that are returned in table_flags()
    //
    ulong int_table_flags;
    //
    // count on the number of rows that gets changed, such as when write_row occurs
    //
    ulong changed_rows;
    //
    // index into key_file that holds DB* that is indexed on
    // the primary_key. this->key_file[primary_index] == this->file
    //
    uint primary_key;
    uint last_dup_key;
    //
    // if set to 0, then the primary key is not hidden
    // if non-zero (not necessarily 1), primary key is hidden
    //
    uint hidden_primary_key;
    uint version;
    bool key_read, using_ignore;

    PRIM_KEY_PART_INFO* primary_key_offsets;
    bool fix_rec_buff_for_blob(ulong length);
#define TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH 5 // QQQ why 5?
    uchar current_ident[TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH];

    ulong max_row_length(const uchar * buf);
    int pack_row(DBT * row, const uchar * record);
    void unpack_row(uchar * record, DBT * row, DBT* key);
    void unpack_key(uchar * record, DBT * key, uint index);
    DBT* create_dbt_key_from_key(DBT * key, KEY* key_info, uchar * buff, const uchar * record, int key_length = MAX_KEY_LENGTH);
    DBT *create_dbt_key_from_table(DBT * key, uint keynr, uchar * buff, const uchar * record, int key_length = MAX_KEY_LENGTH);
    DBT *pack_key(DBT * key, uint keynr, uchar * buff, const uchar * key_ptr, uint key_length);
    int remove_key(DB_TXN * trans, uint keynr, const uchar * record, DBT * prim_key);
    int remove_keys(DB_TXN * trans, const uchar * record, DBT * prim_key, key_map * keys);
    int restore_keys(DB_TXN * trans, key_map * changed_keys, uint primary_key, const uchar * old_row, DBT * old_key, const uchar * new_row, DBT * new_key);
    int key_cmp(uint keynr, const uchar * old_row, const uchar * new_row);
    int update_primary_key(DB_TXN * trans, bool primary_key_changed, const uchar * old_row, DBT * old_key, const uchar * new_row, DBT * prim_key, bool local_using_ignore);
    int read_row(int error, uchar * buf, uint keynr, DBT * row, DBT * key, bool);
    DBT *get_pos(DBT * to, uchar * pos);
 
    int open_secondary_table(DB** ptr, KEY* key_info, const char* name, int mode, u_int32_t* key_type);
 
public:
    ha_tokudb(handlerton * hton, TABLE_SHARE * table_arg);
    ~ha_tokudb() {
    } 
    const char *table_type() const {
        return "TOKUDB";
    } 
    const char *index_type(uint inx) {
        return "BTREE";
    }
    const char **bas_ext() const;

    //
    // Returns a bit mask of capabilities of storage engine. Capabilities 
    // defined in sql/handler.h
    //
    ulonglong table_flags(void) const {
        return int_table_flags;
    } 
    ulong index_flags(uint inx, uint part, bool all_parts) const;

    //
    // Returns limit on the number of keys imposed by tokudb.
    //
    uint max_supported_keys() const {
        return MAX_KEY - 1;
    } 

    uint extra_rec_buf_length() const {
        return TOKUDB_HIDDEN_PRIMARY_KEY_LENGTH;
    } 
    ha_rows estimate_rows_upper_bound();

    //
    // Returns the limit on the key length imposed by tokudb.
    //
    uint max_supported_key_length() const {
        return UINT_MAX32;
    } 

    //
    // Returns limit on key part length imposed by tokudb.
    //
    uint max_supported_key_part_length() const {
        return UINT_MAX32;
    } 
    const key_map *keys_to_use_for_scanning() {
        return &key_map_full;
    }

    double scan_time();

    int open(const char *name, int mode, uint test_if_locked);
    int close(void);
    int create(const char *name, TABLE * form, HA_CREATE_INFO * create_info);
    int delete_table(const char *name);
    int rename_table(const char *from, const char *to);
#if 0
    int analyze(THD * thd, HA_CHECK_OPT * check_opt);
    int optimize(THD * thd, HA_CHECK_OPT * check_opt);
    int check(THD * thd, HA_CHECK_OPT * check_opt);
#endif
    int write_row(uchar * buf);
    int update_row(const uchar * old_data, uchar * new_data);
    int delete_row(const uchar * buf);

    int index_init(uint index, bool sorted);
    int index_end();
    int index_read(uchar * buf, const uchar * key, uint key_len, enum ha_rkey_function find_flag);
    int index_read_idx(uchar * buf, uint index, const uchar * key, uint key_len, enum ha_rkey_function find_flag);
#if 0
    int index_read_last(uchar * buf, const uchar * key, uint key_len);
#endif
    int index_next(uchar * buf);
    int index_next_same(uchar * buf, const uchar * key, uint keylen);
    int index_prev(uchar * buf);
    int index_first(uchar * buf);
    int index_last(uchar * buf);

    int rnd_init(bool scan);
    int rnd_end();
    int rnd_next(uchar * buf);
    int rnd_pos(uchar * buf, uchar * pos);

    int read_range_first(const key_range *start_key,
                                 const key_range *end_key,
                                 bool eq_range, bool sorted);
    int read_range_next();


    void position(const uchar * record);
    int info(uint);
    int extra(enum ha_extra_function operation);
    int reset(void);
    int external_lock(THD * thd, int lock_type);
    int start_stmt(THD * thd, thr_lock_type lock_type);

    ha_rows records_in_range(uint inx, key_range * min_key, key_range * max_key);

    THR_LOCK_DATA **store_lock(THD * thd, THR_LOCK_DATA ** to, enum thr_lock_type lock_type);

    void get_status();
    inline void get_auto_primary_key(uchar * to) {
        pthread_mutex_lock(&share->mutex);
        share->auto_ident++;
        int5store(to, share->auto_ident);
        pthread_mutex_unlock(&share->mutex);
    }
    virtual void get_auto_increment(ulonglong offset, ulonglong increment, ulonglong nb_desired_values, ulonglong * first_value, ulonglong * nb_reserved_values);
    void print_error(int error, myf errflag);
    uint8 table_cache_type() {
        return HA_CACHE_TBL_TRANSACT;
    }
    bool primary_key_is_clustered() {
        return true;
    }
    int cmp_ref(const uchar * ref1, const uchar * ref2);
    bool check_if_incompatible_data(HA_CREATE_INFO * info, uint table_changes);

    int add_index(TABLE *table_arg, KEY *key_info, uint num_of_keys);
    int prepare_drop_index(TABLE *table_arg, uint *key_num, uint num_of_keys);
    int final_drop_index(TABLE *table_arg);

private:
    int __close(int mutex_is_locked);
    int read_last();
    ulong field_offset(Field *);
};