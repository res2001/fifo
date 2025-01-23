#ifndef _CONTAINEROF_H__
#define _CONTAINEROF_H__

#define container_of(ptr, type, node_member)     ((type*)((char*)(ptr) - offsetof(type, node_member)))

#endif  /* _CONTAINEROF_H__ */