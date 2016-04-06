/*
  Copyright (C) 2016 Roman Y. Dayneko, <dayneko3000@gmail.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

/* 
 * File:   db.c
 * Author: Roman
 *
 * Created on January 9, 2016, 3:04 PM
 */

#include "db.h"

#include "stdlib.h"
#include "string.h"
#include <unistd.h>



/*
FILE *f = fopen ("/home/roman/Desktop/UntitledFolder/dbdebug.txt","a+");
fprintf (f,"");
fclose (f);
*/

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

int check_individual_prm_existence(const char *file, const char *table, const int id)
{
    char query [QUERY_MAX], *res = 0, *err = NULL;
    int result = 1;
    
    res = (char*)malloc(sizeof(char) * EXIST_MAX);
    
    sprintf(query, "select exists(select * from %s where path = \"%s\" and id = %d);", table, file, id);
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


int check_prm_existence(const char * file)
{
    char query[QUERY_MAX], *res = 0, *err = NULL;
    int result = 1;
    
    res = (char*)malloc(sizeof(char) * EXIST_MAX);
    
    sprintf(query, "select exists(select * from user_prm_list where path = \"%s\");", file);
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

int get_mask(int prm)
{
    int result = 0;
    if ((prm & (READ | OPEN | READDIR | OPENDIR)) != 0)
        result += 4;
    if ((prm & (WRITE | UNLINK | RENAME | LINK | MKNOD | MKDIR | RMDIR | CREATE)) != 0)
        result += 2;
    if ((prm & EXECUTE) != 0)
        result += 1;
    return result;
}

int get_mode(const char * path)
{
    char query[QUERY_MAX], *err = NULL;
    int result = 0, g_prms = 0, u_prms = 0, o_prms = 0;
    char *res = (char *)malloc(sizeof(char) * PRM_MAX);
    sprintf(query, "select mode from mode_list where path = \"%s\";", path);
    if (sqlite3_exec(db, query, last_colomn_callback, res, &err) != SQLITE_OK){
        
        //что-то не так
        //скорее всего пропала база
        
        free(res);
        
        return 0;
    }
    result = atoi(res) & MODE_MASK;
    sprintf(query, "select prms from user_prm_list where path = \"%s\" and id = %d;", path, STANDART_PRMS);
    sqlite3_exec(db, query, last_colomn_callback, res, &err);
    u_prms = atoi(res);
    sprintf(query, "select prms from group_prm_list where path = \"%s\" and id = %d;", path, STANDART_PRMS);
    sqlite3_exec(db, query, last_colomn_callback, res, &err);
    g_prms = atoi(res);
    sprintf(query, "select prms from other_prm_list where path = \"%s\" and id = %d;", path, STANDART_PRMS);
    sqlite3_exec(db, query, last_colomn_callback, res, &err);
    o_prms = atoi(res);
    result += (get_mask(u_prms) << 6) + (get_mask(g_prms) << 3) + get_mask(o_prms);
    return result;
}

int select_file_prm(const char *file, const char *table, const int id)
{
    char *err = NULL, *res = NULL, query[QUERY_MAX];

    res = (char *)malloc(sizeof(char) * PRM_MAX);
    
    sprintf(query, "select prms from %s where path = \"%s\" and id = %d;", table, file, id);
    if (sqlite3_exec(db, query, last_colomn_callback, res, &err) != SQLITE_OK){
        
        //что-то не так
        //скорее всего пропала база
        
        free(res);
        
        return 0;
    }
    
    int result = atoi(res);//( & operation_mask) != 0;

    free(res);
    
    return result;
}

int select_dir_prm(const char * file, const uid_t *uid, const gid_t *gid, const int owning)
{
    int result_id = 0, len = 0;
    char *temp_path = NULL, *table = NULL;
    
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
            
    if (check_individual_prm_existence(file, "user_prm_list", (int) uid))
    {
        result_id = (int) uid;
        table = "user_prm_list";
    }
    else
        if (check_individual_prm_existence(file, "group_prm_list", (int) gid))
        {
            result_id = (int) gid;
            table = "group_prm_list";
        }
        else
        {
            if (owning == 1)
                table = "user_prm_list";
            else
                if (owning == 2)
                    table = "group_prm_list";
                else
                    table = "other_prm_list";
            result_id = STANDART_PRMS;
        }
    
    int result = select_file_prm(temp_path, table, result_id);

    //if (strlen(temp_path) != 1)
        free(temp_path);

    return result;
}

int select_prm(const char * file, const uid_t *uid, const gid_t *gid)
{
    char *res = NULL, *err = NULL, *table = NULL;
    int result = FULL_PRMS, owner = 0, group_owner = 0, result_id = 0;
    char select_query [QUERY_MAX];

    if (check_prm_existence(file) == 0)
    {
        return 0;
    }
    
    res = (char *) malloc(sizeof(char) * UID_MAX);
    sprintf(select_query, "select uid from own_list where path = \"%s\"", file);
    sqlite3_exec(db, select_query, last_colomn_callback, res, &err);
    owner = atoi(res);
    free(res);
    
    res = (char *) malloc(sizeof(char) * UID_MAX);
    sprintf(select_query, "select gid from group_own_list where path = \"%s\"", file);
    sqlite3_exec(db, select_query, last_colomn_callback, res, &err);
    group_owner = atoi(res);
    free(res);
    
    if (check_individual_prm_existence(file, "user_prm_list", (int) uid))
    {
        result_id = (int) uid;
        table = "user_prm_list";
    }
    else
        if (check_individual_prm_existence(file, "group_prm_list", (int) gid))
        {
            result_id = (int) gid;
            table = "group_prm_list";
        }
        else
        {
            if (owner == (int) uid)
                table = "user_prm_list";
            else
                if (group_owner == (int) gid)
                    table = "group_prm_list";
                else
                    table = "other_prm_list";
            result_id = STANDART_PRMS;
        }
    
    result &= select_file_prm(file, table, result_id);
    result &= select_dir_prm(file, uid, gid, owner == (int) uid ? 1 : group_owner == (int) gid ? 2 : 0);
    
    return result;
}

int put_file_prm(const char *file, const uid_t *uid, const gid_t *gid, const mode_t mode)
{
    char *err = 0;
    char insert_query[QUERY_MAX];

    sprintf(insert_query, "insert or ignore into user_prm_list (path, id, prms) values (\"%s\", %d, %d);", file, STANDART_PRMS , FULL_PRMS);
    if(sqlite3_exec(db, insert_query, NULL, NULL, &err) == SQLITE_OK)
    {
        sprintf(insert_query, "insert or ignore into group_prm_list (path, id, prms) values (\"%s\", %d, %d);", file, STANDART_PRMS, FULL_PRMS);
        sqlite3_exec(db, insert_query, NULL, NULL, &err);
        
        sprintf(insert_query, "insert or ignore into other_prm_list (path, id, prms) values (\"%s\", %d, %d);", file, STANDART_PRMS, READ_ONLY_PRMS);
        sqlite3_exec(db, insert_query, NULL, NULL, &err);
        
        sprintf(insert_query, "insert or replace into own_list (path, uid) values (\"%s\", %d);", file, (int) uid);
        sqlite3_exec(db, insert_query, NULL, NULL, &err);
        
        sprintf(insert_query, "insert or replace into group_own_list (path, gid) values (\"%s\", %d);", file, (int) gid);
        sqlite3_exec(db, insert_query, NULL, NULL, &err);
        
        sprintf(insert_query, "insert or replace into mode_list (path, mode) values (\"%s\", %d);", file, (int) mode);
        sqlite3_exec(db, insert_query, NULL, NULL, &err);
    }
    else
    {
        //что-то не так
        //скорее всего пропала база
    }

    return 0;
}

int rem_prm(const char *file)
{
    char *err = 0;
    char delete_query[QUERY_MAX];
    sprintf(delete_query, "delete from prm_list where path = \"%s\";", file);
    
    if(sqlite3_exec(db, delete_query, NULL, NULL, &err) == SQLITE_OK)
    {
        sprintf(delete_query, "delete from own_list where path = \"%s\";", file);
        sqlite3_exec(db, delete_query, NULL, NULL, &err);
        
        sprintf(delete_query, "delete from group_own_list where path = \"%s\";", file);
        sqlite3_exec(db, delete_query, NULL, NULL, &err);
    }
    else
    {
        //что-то не так
        //скорее всего пропала база
    }
    
    return 0;
}

int upd_own(const char *path, const uid_t *uid, const gid_t *gid)
{
    char update_query [QUERY_MAX], *err = NULL;
    
    sprintf(update_query, "update own_list set id = %d where path = \"%s\";", (int) uid, path);
    if (sqlite3_exec(db, update_query, NULL, NULL, &err) == SQLITE_OK)
    {
        sprintf(update_query, "update group_own_list set id = %d where path = \"%s\";", (int) gid, path);
        sqlite3_exec(db, update_query, NULL, NULL, &err);
    }
    else
    {
        //что-то не так
        //скорее всего пропала база
    }
    
    return 0;
}

int upd_prms(const char *path, const mode_t mode)
{
    int prm = FULL_PRMS;
    char update_query [QUERY_MAX], *err = NULL;
    
    prm = START_PRMS;
    if ((S_IRUSR & mode) != 0)
        prm += READ + OPEN + READDIR + OPENDIR;
    if ((S_IWUSR & mode) != 0)
        prm += WRITE + UNLINK + RENAME + LINK + MKNOD + MKDIR + RMDIR + CREATE;
    if ((S_IXUSR & mode) != 0)
        prm += EXECUTE;
    sprintf(update_query, "update user_prm_list set prms = %d where path = \"%s\" and id = %d;", prm, path, STANDART_PRMS);
    sqlite3_exec(db, update_query, NULL, NULL, &err);

    prm = START_PRMS;
    if ((S_IRGRP & mode) != 0)
        prm += READ + OPEN + READDIR + OPENDIR;
    if ((S_IWGRP & mode) != 0)
        prm += WRITE + UNLINK + RENAME + LINK + MKNOD + MKDIR + RMDIR + CREATE;
    if ((S_IXGRP & mode) != 0)
        prm += EXECUTE;
    sprintf(update_query, "update group_prm_list set prms = %d where path = \"%s\" and id = %d;", prm, path, STANDART_PRMS);
    sqlite3_exec(db, update_query, NULL, NULL, &err);

    prm = START_PRMS;
    if ((S_IROTH & mode) != 0)
        prm += READ + OPEN + READDIR + OPENDIR;
    if ((S_IWOTH & mode) != 0)
        prm += WRITE + UNLINK + RENAME + LINK + MKNOD + MKDIR + RMDIR + CREATE;
    if ((S_IXOTH & mode) != 0)
        prm += EXECUTE;
    sprintf(update_query, "update other_prm_list set prms = %d where path = \"%s\" and id = %d;", prm, path, STANDART_PRMS);
    sqlite3_exec(db, update_query, NULL, NULL, &err);
    
    sprintf(update_query, "update mode_list set mode = %d where path = \"%s\";", (int) mode, path);
    sqlite3_exec(db, update_query, NULL, NULL, &err);
}

int upd_path(const char *path, const char *newpath, const char *table)
{
    quantity = 0;
    
    char select_query[QUERY_MAX], *err = NULL;
    char **res = (char**)malloc(sizeof(char*) * DEEP_MAX);

    for (int i = 0; i < DEEP_MAX; i ++)
    {
        res[i] = (char*)malloc(sizeof(char) * PATH_MAX);
    }
    
    sprintf(select_query, "select * from %s where path like \"%s%%\";", table, path);
    if (sqlite3_exec(db, select_query, first_colomn_callback, res, &err) != SQLITE_OK)
    {
        //что-то не так
        //скорее всего пропала база
    }
    
    for (int i = 0; i < quantity; i++)
    {
        int dif_len = strlen(res[i]) - strlen(path);
        char *temp = (char*)malloc(sizeof(char)*(dif_len + 1));
        char update_query [QUERY_MAX];
        
        for (int j = strlen(path); j < strlen(res[i]); j++)
            temp[j - strlen(path)] = res[i][j];
        temp[dif_len] = '\0';
        
        if (dif_len == 0)
            sprintf(update_query, "update or ignore %s set path = \"%s\" where path = \"%s\";", table, newpath, res[i]);
        else
            sprintf(update_query, "update or ignore %s set path = \"%s%s\" where path = \"%s\";", table, newpath, temp, res[i]);
        if (sqlite3_exec(db, update_query, NULL, NULL, &err) != SQLITE_OK)
        {
            rem_prm(res[i]);
        }
        
        free(temp);
    }
    
    for (int i = 0; i < DEEP_MAX; i ++){
        free(res[i]);
    }
    free(res);
    
    return 0;
}


int init_db(const char *rootdir, const char *mountdir, const uid_t *uid, const gid_t *gid)
{
    char *err = NULL, *db_name = "prm.dblite";
    root_path = "/";
    sqlite3_stmt *res;
    
    if(sqlite3_open(db_name, &db) != SQLITE_OK)
    {
        //Ошибка открытия/создания БД
        //abort();)
    }
    sqlite3_prepare(db,"SELECT SQLITE_VERSION()", -1, &res, 0);
    sqlite3_finalize(res);
    
    sqlite3_exec(db, "create table if not exists user_prm_list(path text, id int, prms int);", NULL, NULL, &err);
    
    sqlite3_exec(db, "create unique index if not exists user_path_uid_index on user_prm_list(path, id);", NULL, NULL, &err);
    
    sqlite3_exec(db, "create table if not exists group_prm_list(path text, id int, prms int);", NULL, NULL, &err);
    
    sqlite3_exec(db, "create unique index if not exists group_path_uid_index on group_prm_list(path, id);", NULL, NULL, &err);
    
    sqlite3_exec(db, "create table if not exists other_prm_list(path text, id int, prms int);", NULL, NULL, &err);
    
    sqlite3_exec(db, "create unique index if not exists other_path_index on other_prm_list(path);", NULL, NULL, &err);
    
    sqlite3_exec(db, "create table if not exists mode_list(path text, mode int);", NULL, NULL, &err);
    
    sqlite3_exec(db, "create unique index if not exists mod_path_index on mode_list(path);", NULL, NULL, &err);
    
    sqlite3_exec(db, "create table if not exists own_list(path text, uid int);", NULL, NULL, &err);

    sqlite3_exec(db, "create unique index if not exists path_index on own_list(path);", NULL, NULL, &err);
    
    sqlite3_exec(db, "create table if not exists group_own_list(path text, gid int);", NULL, NULL, &err);

    sqlite3_exec(db, "create unique index if not exists path_index on group_own_list(path);", NULL, NULL, &err);
    
    sqlite3_exec(db, "create table if not exists dir_list(mount text, root text);", NULL, NULL, &err);
    
    sqlite3_exec(db, "create unique index if not exists mount_index on dir_list(mount);", NULL, NULL, &err);
    
    sqlite3_exec(db, "create unique index if not exists root_index on dir_list(root);", NULL, NULL, &err);
    
    //sqlite3_exec(db, "delete from dir_list;", NULL, NULL, &err);
    
    if (!check_prm_existence("/"))
    {
        struct stat *stat_buf = (struct stat *)malloc(sizeof(struct stat));
        if (lstat(rootdir, stat_buf) != 0)
            abort();
        put_file_prm("/", uid, gid, stat_buf->st_mode); 
        free(stat_buf);
    }
    
    char insert_dirs_query [QUERY_MAX];
    sprintf(insert_dirs_query, "insert or replace into dir_list (mount, root) values(\"%s\", \"%s\");", mountdir, rootdir);
    sqlite3_exec(db, insert_dirs_query, NULL, NULL, &err);
    
    return 0;
}