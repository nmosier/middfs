/* middfs-util.h -- utility functions
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_UTIL_H
#define __MIDDFS_UTIL_H

//#include <string.h>
//#include <stdint.h>

size_t sizerem(size_t nbytes, size_t used);

#define MAX(i1, i2) ((i1) < (i2) ? (i2) : (i1))
#define MIN(i1, i2) ((i1) < (i2) ? (i1) : (i2))

#if 0

/* X MACRO UTILITIES */
/* sn -- struct name
 * type -- type of struct member
 * name -- name of struct member */
#define STRUCT_MEMB_BASIC(sn, type, suffix, name)	\
  type name;
#define STRUCT_MEMB_STRUCT(sn, type, suffix, name)	\
  struct type name;
#define STRUCT_MEMB_ENUM(sn, type, suffix, name)	\
  enum type name;
#define STRUCT_MEMB_UNION(sn, type, suffix, name)	\
  union type name;

#define UNION_MEMB_BASIC(sn, type, suffix, name)	\
  type name;
#define UNION_MEMB_STRUCT(sn, type, suffix, name)	\
  struct type name;
#define UNION_MEMB_ENUM(sn, type, suffix, name)	\
  enum type name;
#define UNION_MEMB_UNION(sn, type, suffix, name)	\
  union type name;

/* SN -- struct name
 * MEMBS -- struct members
 */
#define STRUCT(SN, MEMBS)			\
  struct SN {					\
    MEMBS(SN, STRUCT_MEMB_BASIC, STRUCT_MEMB_STRUCT,	\
	  STRUCT_MEMB_ENUM, STRUCT_MEMB_UNION)		\
  }

#define UNION(UN, MEMBS)			\
  union UN {					\
  MEMBS(UN, UNION_MEMB_BASIC, UNION_MEMB_STRUCT,	\
	UNION_MEMB_ENUM, UNION_MEMB_UNION)		\
  }

/* enum table? */

#define ENUM_MEMB(en, type, suffix, name) \
  

#define ENUM(EN, MEMBS)				\
  enum EN {					\


#define PRINT_MEMB_BASIC(sn, type, suffix, name) \
  print_##suffix(sn->name);
#define PRINT_MEMB_STRUCT(sn, type, suffix, name)	\
  print_##suffix(&sn->name);
#define PRINT_MEMB_ENUM(sn, type, suffix, name)	      	\
  print_##suffix(sn->name);
#define PRINT_MEMB_UNION(sn, type, suffix, name) /* not defined */

#define PRINT_STRUCT_PROT(SN, suffix)			\
  print_##suffix(struct SN *st)

#define PRINT_STRUCT(SN, suffix, MEMBS)			\
  PRINT_STRUCT_PROT(SN, suffix) {			\
  MEMBS(SN, PRINT_MEMB_BASIC, PRINT_MEMB_STRUCT,	\
	PRINT_MEMB_ENUM, PRINT_MEMB_UNION)		\
  }



/* end X MACRO UTILITIES */

/* SN -- struct name (minus the leading `struct')
 * FN -- function to apply to each struct member */
#define UINT64_SPLIT(SN, FN_BASIC, FN_STRUCT, FN_ENUM, FN_UNION) \
  FN_BASIC(SN, uint32_t, uint32, msh)				 \
  FN_BASIC(SN, uint32_t, uint32, lsh)

/* Table format:
 * enum value; function suffix for type; 
 * corresponding union member */
#define RSRC_ENUM(NM, FN_BASIC, FN_STRUCT, FN_ENUM, FN_UNION)	\
  FN_BASIC(MR_NETWORK, int, int, un_network)			\
  FN_BASIC(MR_LOCAL, int, int, un_local)			\
  FN_BASIC(MR_ROOT, int, int, un_root)





STRUCT(uint64_split, UINT64_SPLIT);
UNION(uint64_split, UINT64_SPLIT);
PRINT_STRUCT(uint64_split, uint64_split, UINT64_SPLIT)

#endif
  
#endif
