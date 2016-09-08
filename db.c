/*
  Copyright (C) 2016 Roman Y. Dayneko, <dayneko3000@gmail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

#include "db.h"

#include "stdlib.h"
#include "string.h"
#include "unistd.h"

static int last_colomn_callback(void *notused, int coln, char **rows, char **colnm)
{
    char *temp = (char*)notused;
    sprintf(temp, "%s", rows[coln - 1]);
    
    return 0;
}

static int quantity = 0;

static int first_colomn_callback(void *notused, int coln, char **rows, char **colnm)
{
    char **temp = (char**)notused;
    
    sprintf(temp[quantity], "%s", rows[0]);

    quantity ++;
    
    return 0;
}

/*
char *dir_path(const char * file)
{
    
    
    return res;
}
*/

int check_individual_prm_existence(const char *table, const char *file, const int id)
{
    char query [QUERY_MAX], *res = 0, *err = NULL;
    int result = 1;
    
    res = (char*)malloc(sizeof(char) * EXIST_MAX);
    
    sprintf(query, "SELECT EXISTS(SELECT * FROM %s WHERE (id = %d AND file_id = (SELECT id FROM file_list WHERE path = \"%s\")));", table, id, file);
    if (sqlite3_exec(db, query, last_colomn_callback, res, &err) != SQLITE_OK)
    {
        //что-то не так
        //скорее всего пропала база
        
        free(res);
        
        return 0;
    }
    
    if (res[0] != '1')
    {
        result = 0;
    }
    
    free(res);
    
    return result;
}

int is_rootdir(const char * file)
{
    if (strcmp(file, root_path) == 0)
        return 1;

    return 0;
}


int check_prm_existence(const char *file)
{
    char query[QUERY_MAX], *res = 0, *err = NULL;
    int result = 1;
    
    res = (char*)malloc(sizeof(char) * EXIST_MAX);
    sprintf(query, "SELECT EXISTS(SELECT * FROM file_list WHERE path = \"%s\");", file);
    if (sqlite3_exec(db, query, last_colomn_callback, res, &err) != SQLITE_OK)
    {
        free(res);
        
        return 0;
    }
    
    if (res[0] != '1')
    {
        result = 0;
    }
    
    free(res);
    
    return result;
} 

int select_prm(const char *table, const char *file, const int id)
{
    char *err = NULL, *res = NULL, query[QUERY_MAX];

    res = (char *)malloc(sizeof(char) * PRM_MAX);
    
    sprintf(query, "SELECT prms FROM %s WHERE path = \"%s\" AND id = %d;", table, file, id);
    if (sqlite3_exec(db, query, last_colomn_callback, res, &err) != SQLITE_OK)
    {
        free(res);
        
        return 0;
    }
    
    int result = atoi(res);//( & operation_mask) != 0;

    free(res);
    
    return result;
}

int get_field(const char *file, const char *field)
{
    int result = 0;
    char *err = NULL, *res = NULL, query[QUERY_MAX];

    res = (char *)malloc(sizeof(char) * PRM_MAX);
    
    sprintf(query, "SELECT %s FROM file_list WHERE path = \"%s\"", field, file);
    sqlite3_exec(db, query, last_colomn_callback, res, &err);
    result = atoi(res);
    
    free(res);
    
    return result;
}

int get_dir_prms(const char *file, const uid_t *uid, const gid_t *gid, const int owning)
{
    int result = 0, len = 0;
    char *temp_path = NULL;
    
    if (is_rootdir(file)) 
        return FULL_PRMS;
    
    for (int i = strlen(file) - 1; i >= 0; i --){
        if (file[i] == '/'){
            len = i == 0 ? 1 : i;
            break;
        }
    }
    temp_path = (char *) malloc(sizeof(char) * len);
    
    for (int i = 0; i < len; i ++){
        temp_path[i] = file[i];
    }
    temp_path[len] = '\0'; 
            
    if (check_individual_prm_existence("user_prm_list", temp_path, (int) uid))
        result = select_prm(temp_path, "user_prm_list", (int) uid);
    else
        if (check_individual_prm_existence("group_prm_list", temp_path, (int) gid))
            result = select_prm(temp_path, "group_prm_list", (int) gid);
        else
            if (owning == 1)
                result = get_field(temp_path, "owner_prms");
            else
                if (owning == 2)
                    result = get_field(temp_path, "group_prms");
                else
                    result = get_field(temp_path, "other_prms");
    
    //result &= get_dir_prms(temp_path, uid, gid, owning);        
            
    free(temp_path);

    return result;
}

int get_prms(const char *file, const uid_t *uid, const gid_t *gid)
{
    int result = 0, owner = 0, group_owner = 0;
    

    if (check_prm_existence(file) == 0)
    {
        return 0;
    }
    
    owner = get_field(file, "uid");
    group_owner = get_field(file, "gid");
    
    if (check_individual_prm_existence("user_prm_list", file, (int) uid))
        result = select_prm(file, "user_prm_list", (int) uid);
    else
        if (check_individual_prm_existence("group_prm_list", file, (int) gid))
            result = select_prm(file, "group_prm_list", (int) gid);
        else
            if (owner == (int) uid)
                result = get_field(file, "owner_prms");
            else
                if (group_owner == (int) gid)
                    result = get_field(file, "group_prms");
                else
                    result = get_field(file, "other_prms");
            
    
    result &= get_dir_prms(file, uid, gid, owner == (int) uid ? 1 : group_owner == (int) gid ? 2 : 0);
    
    return result;
}

int add_file(const char *file, const uid_t *uid, const gid_t *gid, const mode_t mode)
{
    char *err = 0;
    char insert_query[QUERY_MAX];

    sprintf(insert_query, "INSERT OR IGNORE INTO file_list (path, uid, gid, mode, owner_prms, group_prms, other_prms) VALUES(\"%s\", %d, %d, %d, %d, %d, %d);", 
            file, (int)uid, (int) gid, (int) mode, FULL_PRMS | (S_ISDIR(mode) ? OPENDIR : 0), FULL_PRMS | (S_ISDIR(mode) ? OPENDIR : 0), READ_ONLY_PRMS | (S_ISDIR(mode) ? OPENDIR : 0));
    if(sqlite3_exec(db, insert_query, NULL, NULL, &err) != SQLITE_OK)
    {
        //ignored
    }

    return 0;
}

int remove_file(const char *file)
{
    char *err = 0;
    char delete_query[QUERY_MAX];
    sprintf(delete_query, "DELETE FROM user_prm_list WHERE (user_prm_list.id = (SELECT id FROM file_list WHERE path = \"%s\"));", file);
    
    if(sqlite3_exec(db, delete_query, NULL, NULL, &err) == SQLITE_OK)
    {
        sprintf(delete_query, "DELETE FROM group_prm_list WHERE (group_prm_list.id = (SELECT id FROM file_list WHERE path = \"%s\"));", file);
        sqlite3_exec(db, delete_query, NULL, NULL, &err);
        
        sprintf(delete_query, "DELETE FROM file_list WHERE path = \"%s\";", file);
        sqlite3_exec(db, delete_query, NULL, NULL, &err);
    }
    else
    {
        //ignored
    }
    
    return 0;
}

int update_owners(const char *path, const uid_t *uid, const gid_t *gid)
{
    char update_query [QUERY_MAX], *err = NULL;
    
    sprintf(update_query, "UPDATE file_list SET uid = %d, gid = %d WHERE path = \"%s\";", (int) uid, (int) gid, path);
    if (sqlite3_exec(db, update_query, NULL, NULL, &err) != SQLITE_OK)
    {
        //ignored
    }
    
    return 0;
}

int update_permissions(const char *path, const mode_t mode)
{
    int owner_prms = START_PRMS, group_prms = START_PRMS, other_prms = START_PRMS;
    char update_query [QUERY_MAX], *err = NULL;
    
    owner_prms = START_PRMS | CHMOD;
    if ((S_IRUSR & mode) != 0)
        owner_prms |= READ | GETATTR | READDIR | FGETATTR;
    if ((S_IWUSR & mode) != 0)
        owner_prms |= WRITE | UNLINK | RENAME | LINK | MKNOD | MKDIR | RMDIR | CREATE | SYMLINK | UTIME;
    if ((S_IXUSR & mode) != 0)
        owner_prms |= EXECUTE | OPENDIR;
    
    group_prms = START_PRMS;
    if ((S_IRGRP & mode) != 0)
        group_prms |= READ | GETATTR | READDIR | FGETATTR;
    if ((S_IWGRP & mode) != 0)
        group_prms |= WRITE | UNLINK | RENAME | LINK | MKNOD | MKDIR | RMDIR | CREATE | SYMLINK | UTIME;
    if ((S_IXGRP & mode) != 0)
        group_prms |= EXECUTE | OPENDIR;
    
    other_prms = START_PRMS;
    if ((S_IROTH & mode) != 0)
        other_prms |= READ | GETATTR | READDIR | FGETATTR;
    if ((S_IWOTH & mode) != 0)
        other_prms |= WRITE | UNLINK | RENAME | LINK | MKNOD | MKDIR | RMDIR | CREATE | SYMLINK | UTIME;
    if ((S_IXOTH & mode) != 0)
        other_prms |= EXECUTE | OPENDIR;
    sprintf(update_query, "UPDATE file_list SET owner_prms = %d, group_prms = %d, other_prms = %d, mode = %d WHERE path = \"%s\";", owner_prms, group_prms, other_prms, (int) mode, path);
    sqlite3_exec(db, update_query, NULL, NULL, &err);
}

int update_path(const char *path, const char *newpath)
{
    char update_query[QUERY_MAX], *err = NULL;
    
    sprintf(update_query, "UPDATE file_list SET path = \"%s\" WHERE path = \"%s\"", newpath, path);
    sqlite3_exec(db, update_query, NULL, NULL, &err);
    
    return 0;
}


int init_db(const char *rootdir, const char *mountdir, const uid_t *uid, const gid_t *gid)
{
    char *err = NULL, *db_name = "FTDB.db", insert_query [QUERY_MAX];
    root_path = "/";
    sqlite3_stmt *res;
    
    if(sqlite3_open(db_name, &db) != SQLITE_OK)
    {
        abort();
    }
    
    sqlite3_prepare(db,"SELECT SQLITE_VERSION()", -1, &res, 0);
    sqlite3_finalize(res);
    
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS file_list(id INTEGER PRIMARY KEY, path TEXT, uid INTEGER, gid INTEGER, mode INTEGER, owner_prms INTEGER, group_prms INTEGER, other_prms INTEGER);", NULL, NULL, &err);
    
    sqlite3_exec(db, "CREATE UNIQUE INDEX IF NOT EXISTS file_list_path_index ON file_list(path);", NULL, NULL, &err);
    
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS user_prm_list(file_id INTEGER, id INTEGER, prms INTEGER);", NULL, NULL, &err);
    
    sqlite3_exec(db, "CREATE UNIQUE INDEX IF NOT EXISTS user_file_id_id_index ON user_prm_list(file_id, id);", NULL, NULL, &err);
    
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS group_prm_list(file_id INTEGER, id INTEGER, prms INTEGER);", NULL, NULL, &err);
    
    sqlite3_exec(db, "CREATE UNIQUE INDEX IF NOT EXISTS group_file_id_id_index ON group_prm_list(file_id, id);", NULL, NULL, &err);
    
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS config(id INTEGER PRIMARY KEY, path TEXT);", NULL, NULL, &err);
    
    sqlite3_exec(db, "CREATE UNIQUE INDEX IF NOT EXISTS config_path_index ON config(path);", NULL, NULL, &err);
    
    if (!check_prm_existence("/"))
    {
        struct stat *stat_buf = (struct stat *)malloc(sizeof(struct stat));
        if (lstat(rootdir, stat_buf) != 0)
            abort();
        add_file("/", uid, gid, stat_buf->st_mode); 
        free(stat_buf);
    }
    
    sprintf(insert_query, "INSERT OR REPLACE INTO config (id, path) VALUES(1, \"%s\");", mountdir);
    sqlite3_exec(db, insert_query, NULL, NULL, &err);
    sprintf(insert_query, "INSERT OR REPLACE INTO config (id, path) VALUES(2, \"%s\");", rootdir);
    sqlite3_exec(db, insert_query, NULL, NULL, &err);
    
    return 0;
}