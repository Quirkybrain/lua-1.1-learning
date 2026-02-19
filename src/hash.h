/**
 * @file hash.h
 * @brief 哈希表声明。
 */

#ifndef hash_h
#define hash_h

typedef struct node
{
 Object ref;
 Object val;
 struct node *next;
} Node;

typedef struct Hash
{
 char           mark;
 unsigned int   nhash;
 Node         **list;
} Hash;


Hash    *lua_createarray (int nhash);
void     lua_hashmark (Hash *h);
void     lua_hashcollector (void);
Object 	*lua_hashdefine (Hash *t, Object *ref);
void     lua_next (void);

#endif
