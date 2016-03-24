/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   db.c
 * Author: Roman
 *
 * Created on January 9, 2016, 3:03 PM
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

static int path_callback(void *notused, int coln, char **rows, char **colnm)
{
    char **temp = (char**)notused;
    sprintf(temp[quantity], "%s", rows[0]);
    
    quantity ++;
    
    return 0;
}

char * dir_path(const char * file)
{
    int len = 0;
    for (int i = strlen(file) - 1; i >= 0; i --){
        if (file[i] == '/'){
            len = i + 1;
            break;
        }
    }
    char * res = (char*)malloc(sizeof(char) * len);
    for (int i = 0; i < len; i ++){
        res[i] = file[i];
    }
    res[len] = '\0';
    
    return res;
}

int check_individual_prm_existence(char * file, uid_t * uid)
{
    char *query = 0,
         *res = 0,
         *err = NULL;
    int result = 1;
    
    query = (char*)malloc(sizeof(char) * 500);
    res = (char*)malloc(sizeof(char) * 500);
    
    sprintf(query, "select exists(select * from prm_list where path = \"%s\" and uid = %d);", file, (int)uid);
    if (sqlite3_exec(db, query, last_colomn_callback, res, &err) != SQLITE_OK)
    {
        free(res);
        free(query);
        return 0;
    }
    
    if (res[0] != '1')
    {
        result = 0;
    }
    
    free(res);
    free(query);
    
    return result;
}

int is_rootdir(const char * file)
{
    if (strcmp(file, root_path) == 0)
        return 1;

    return 0;
}


int check_prm_existence(char * file)
{
    char *query = 0,
         *res = 0,
         *err = NULL;
    int result = 1;
    
    query = (char*)malloc(sizeof(char) * 500);
    res = (char*)malloc(sizeof(char) * 500);
    
    sprintf(query, "select exists(select * from prm_list where path = \"%s\");", file);
    if (sqlite3_exec(db, query, last_colomn_callback, res, &err) != SQLITE_OK)
    {
        free(res);
        free(query);
        
        return 0;
    }
    
    if (res[0] != '1')
    {
        result = 0;
    }
    
    free(res);
    free(query);
    
    return result;
} 

int select_file_prm(char * file, int uid, int op_num)
{
    char *err = NULL, *res = NULL, *query = NULL;
    
/*
    if (is_hidden(file))
        return 1;
*/
    
    query = (char*)malloc(sizeof(char) * 500);
    res = (char*)malloc(sizeof(char) * 500);
    sprintf(query, "select prms from prm_list where path = \"%s\" and uid = %d;", file, uid);
    if (sqlite3_exec(db, query, last_colomn_callback, res, &err) != SQLITE_OK){

        free(res);
        free(query);
        
        return 0;
    }
    
    int result = atoi(res) & op_num != 0;

    free(res);
    free(query);
    
    return result;
}

int select_dir_prm(char * file, uid_t *uid, int op_num, int owning)
{
    int prm_uid = (int)uid;
/*
    if (is_hidden(file))
        return 1;
*/
    
    if (is_rootdir(file)) 
        return 1;
    
    char * temp_path = dir_path(file);
    
    if (!check_individual_prm_existence(temp_path, uid))
        prm_uid = owning ? OWNER_PRMS : NOT_OWNER_PRMS;
    
    int result = select_file_prm(temp_path, prm_uid, op_num);

    free(temp_path);

    return result;

}

int select_prm(const char * filename, uid_t *uid, int op_num)
{
    char *res = NULL, *file = NULL, *err = NULL;
    //struct stat *stat_buf = (struct stat *)malloc(sizeof(struct stat));
    int result = 1, owner = 0, prm_uid = 0;
    
/*
    if (is_hidden(filename))
    {
        free(stat_buf);
        return 1;
    }
*/
    
/*
    if (lstat(filename, stat_buf) != 0)
    {
        free(stat_buf);
        return 0;
    }
*/
    if (check_prm_existence(filename) == 0)
    {
        return 0;
    }
    
    res = (char *) malloc(sizeof(char) * 50);
    char *owner_select_query = (char *) malloc(sizeof(char) * 500);
    sprintf(owner_select_query, "select uid from own_list where path = \"%s\"", filename);
    sqlite3_exec(db, owner_select_query, last_colomn_callback, res, &err);
    owner = atoi(res);
    
/*
    if (check_individual_prm_existence(filename, uid))
        prm_uid = (int)uid;
    else
        prm_uid = owner == (int)uid ? OWNER_PRMS : NOT_OWNER_PRMS;
*/
    
//    file = get_slash(filename);
    
/*
    
*/
    
//    free(res);
//    free(file);
    
    result *= select_file_prm(filename, check_individual_prm_existence(filename, uid) ? (int)uid : owner == (int)uid ? OWNER_PRMS : NOT_OWNER_PRMS, op_num);
    
    result *= select_dir_prm(filename, uid, op_num, owner == (int)uid ? 1 : 0);
    
    //free(stat_buf);
    //free(file);
    
    return result;
}

int put_file_prm(char *file, uid_t *uid)
{
    char *err = 0;
    char insert_query[500];
    char *res = (char *) malloc(sizeof(char) * PREMISSIONS_SIZE);

    sprintf(insert_query, "insert or ignore into prm_list (path, uid, prms) values (\"%s\", %d, 1073741823);", file, OWNER_PRMS);
    if(sqlite3_exec(db, insert_query, 0, 0, &err) == SQLITE_OK)
    {
        sprintf(insert_query, "insert or ignore into prm_list (path, uid, prms) values (\"%s\", %d, 1073741823);", file, NOT_OWNER_PRMS);
        sqlite3_exec(db, insert_query, 0, 0, &err);
        
        sprintf(insert_query, "insert or replace into own_list (path, uid) values (\"%s\", %d);", file, (int)uid);
        sqlite3_exec(db, insert_query, 0, 0, &err);
    }

    free(res);

    return 0;
}

int put_dir_prm(char *file, uid_t *uid)
{
    char *err = 0;
    char insert_query[500];
    put_file_prm(file, uid);
    
    sprintf(insert_query, "insert or ignore into prm_list (path, uid, prms) values (\"%s/\", %d, 1073741823);", file, OWNER_PRMS);
    if(sqlite3_exec(db, insert_query, 0, 0, &err) == SQLITE_OK)
    {
        sprintf(insert_query, "insert or ignore into prm_list (path, uid, prms) values (\"%s/\", %d, 1073741823);", file, NOT_OWNER_PRMS);
        sqlite3_exec(db, insert_query, 0, 0, &err);
    }
    
    return 0;
}

int rem_prm(char *file)
{
    char *err = 0;
    char delete_query[500];
    sprintf(delete_query, "delete from prm_list where path = \"%s\";", file);
    
    if(sqlite3_exec(db, delete_query, 0, 0, &err) != SQLITE_OK)
    {
        //что-то не так
    }
    sprintf(delete_query, "delete from own_list where path = \"%s\";", file);
    sqlite3_exec(db, delete_query, 0, 0, &err);
    
    return 0;
}


int upd_prm(char *path, char *newpath, char *table)
{
    quantity = 0;
    char **res = (char**)malloc(sizeof(char*) * 200);
    for (int i = 0; i < 200; i ++){
        res[i] = (char*)malloc(sizeof(char) * 200);
    }
    char *select_query, *err = 0;
    select_query = (char *)malloc(sizeof(char)*500);
    
    sprintf(select_query, "select * from %s where path like \"%s%%\";", table, path);
    if (sqlite3_exec(db, select_query, path_callback, res, &err) != SQLITE_OK)
    {
        //что-то не так
    }
    
    free(select_query);
    
    for (int i = 0; i < quantity; i++)
    {
        int dif_len = strlen(res[i]) - strlen(path);
        char *temp = (char*)malloc(sizeof(char)*(dif_len + 1));
        char update_query [500];
        
        for (int j = strlen(path); j < strlen(res[i]); j++)
            temp[j - strlen(path)] = res[i][j];
        temp[dif_len] = '\0';
        if (dif_len == 0)
            sprintf(update_query, "update %s set path = \"%s\" where path = \"%s\";", table, newpath, res[i]);
        else
            sprintf(update_query, "update %s set path = \"%s%s\" where path = \"%s\";", table, newpath, temp, res[i]);
        if (sqlite3_exec(db, update_query, 0, 0, &err) != SQLITE_OK)
        {
            rem_prm(res[i]);
        }
        
        free(temp);
        //free(update_query);
    }
    
    for (int i = 0; i < 200; i ++){
        free(res[i]);
    }
    free(res);
    
    return 0;
}


int init_db(char *rootdir, char *mountdir, uid_t *uid)
{
    char *err = NULL;
    
    int len = strlen(rootdir);
    char *db_name = "prm.dblite";
    root_path = "/";
    
    if(sqlite3_open(db_name, &db) != SQLITE_OK)
    {
        //Ошибка открытия/создания БД
        //abort();)
    }
    
    char *create_prm_table_query = "create table if not exists prm_list(path text, uid int, prms int);";
    sqlite3_exec(db, create_prm_table_query, 0, 0, &err);
    
    char *prm_unique_index_create_query = "create unique index if not exists path_uid_index on prm_list(path, uid);";  
    sqlite3_exec(db, prm_unique_index_create_query, 0, 0, &err);

    char *create_own_table_query = "create table if not exists own_list(path text, uid int);";
    sqlite3_exec(db, create_own_table_query, 0, 0, &err);

    char *own_unique_index_create_query = "create unique index if not exists path_index on own_list(path);";  
    sqlite3_exec(db, own_unique_index_create_query, 0, 0, &err);

    char exists_rootdir_prms_query [500];
    char *res = (char *) malloc(sizeof(char) * 2);
    sprintf(exists_rootdir_prms_query, "select exists(select * from prm_list where path = \"%s\");", root_path);
    sqlite3_exec(db, exists_rootdir_prms_query, last_colomn_callback, res, &err);

    if (res[0] == '0')
    {
        put_file_prm("/", uid); 
    }

    free(res);
    
    char *create_dir_table_query = "create table if not exists dir_list(mount text, root text);";
    sqlite3_exec(db, create_dir_table_query, 0, 0, &err);
    
    char *delete_query = "delete from dir_list;";  
    sqlite3_exec(db, delete_query, 0, 0, &err);
    
    char insert_dirs_query [500];
    sprintf(insert_dirs_query, "insert into dir_list values(\"%s\", \"%s\");", mountdir, rootdir);
    sqlite3_exec(db, insert_dirs_query, 0, 0, &err);
    
    return 0;
}