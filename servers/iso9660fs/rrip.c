/*
 * Rock Ridge Extension to iso9660fs.
 *   
 * Changes:
 *   Aug 14, 2013  support PX, PN, SL and NM entries.  (LiQiong Lee)
 */

#include "inc.h"
#include "rrip.h"
#include "isonum.h"

#include <sys/stat.h>

#define SIG(a, b)  (u16_t)(a | b << 8)

#define	 MINIMUM_LEN_ENTRY	 4
#define	 NAME_TRUNK	 64

static u8_t found_sp = 0;
static u8_t entry_offset = 0;

static int	check_susp(struct susp_desc *s_desc);

static void do_entry_SL(struct dir_record *dir,
						struct susp_desc *s_desc);

static void check_room(char **str, int add, int size_trunk);


void create_rrip_attr(struct dir_record *dir, 
					  char *buffer, u32_t length_sysuse)
{
	int next_entry_offset = entry_offset;
	struct rrip_attr* rr_attr = DIR_RRIP_ATTR(dir);
	rr_attr->s_link = NULL;
	rr_attr->f_name = NULL;

	//printf("\n create rrip attr (%d->%x)\n", dir->d_ino_nr, dir->d_phy_addr);
	
	while(length_sysuse >= next_entry_offset + MINIMUM_LEN_ENTRY)
	{
		u16_t sig;
		struct susp_desc* s_entry;
		char *buf_entry;

		buf_entry = buffer + next_entry_offset;
		s_entry = (struct susp_desc *)(buf_entry);

		next_entry_offset += s_entry->len;
		sig = isonum_721(s_entry->sig);
			  
		if( !check_susp(s_entry) )
			return;

		if(SIG('P', 'X') == sig) {
			dir->d_mode = isonum_733(s_entry->d.PX.mode);
			dir->d_uid = isonum_733(s_entry->d.PX.uid);
			dir->d_gid = isonum_733(s_entry->d.PX.gid);
			continue;
		}

		if(SIG('P', 'N') == sig) {
			if( S_ISCHR(dir->d_mode) ||  
				S_ISBLK(dir->d_mode) ){
			  	u16_t h = isonum_733(s_entry->d.PN.h_devnd);
				u16_t l = isonum_733(s_entry->d.PN.l_devnd);
				
				if ((l & ~0xff) && h == 0) /* come from  linux */
					dir->d_rdev = makedev(l >> 8, l & 0xff);
				else
					dir->d_rdev = makedev(h, l);
			}
			continue;
		}

		if(SIG('S', 'L') == sig) {
			if( S_ISLNK(dir->d_mode) )
				do_entry_SL(dir, s_entry);

			continue;
		}

		if(SIG('N', 'M') == sig) {
			if( (s_entry->d.NM.flag & 0x02) == 0 
				&& (s_entry->d.NM.flag & 0x04) == 0 
				&& s_entry->len ) {

				check_room(&rr_attr->f_name, 
						   s_entry->len-5 + 1, 
						   NAME_TRUNK);

				strncat(rr_attr->f_name, 
						s_entry->d.NM.name,
						s_entry->len-5);
			}
			continue;
		}

		if(SIG('S', 'T') == sig)
			return;
	}
}

static void do_entry_SL(struct dir_record *dir, struct susp_desc *s_entry)
{
	struct rrip_attr* rr_attr = DIR_RRIP_ATTR(dir);
	u8_t *buf_com = s_entry->d.SL.com_area;
	char *slink;
	
	check_room(&(rr_attr->s_link), s_entry->len-5, NAME_TRUNK);
	slink = rr_attr->s_link;

	do{
		struct sl_com *com = (struct sl_com *)buf_com;
		int is_root = 0;

		if ( com->flag & 0x02 )
			strcat(slink, ".");
		else if( com->flag & 0x04 )
			strcat(slink, "..");
		else if( com->flag & 0x08 ) {
			strcat(slink, "/");
			is_root = 1;
		} else if(com->len)
			strncat(slink, com->path, com->len);
				 
		if(!(com->flag & 0x01) && !is_root)
			strcat(slink, "/");
					
		buf_com += com->len+2;
	}while(buf_com - s_entry->d.SL.com_area 
		   < s_entry->len-5-1); /*padding bytes*/

	if(!(s_entry->d.SL.flag & 0x01)) {
		*(strrchr(rr_attr->s_link, '/')) = '\0';
		dir->d_file_size = strlen(rr_attr->s_link);
	}
}

static int check_susp(struct susp_desc *s_entry)
{
	/* Only the first directory (".") of the 
	 *  root directory has the SP entry.
     */

	u16_t sig = isonum_721(s_entry->sig);

	#define SP_MAGIC  0xEFBE

	if(!found_sp)
	{
		if(SIG('S', 'P') == sig)
		{
			if (s_entry->d.SP.magic == SP_MAGIC) {
				entry_offset = s_entry->d.SP.offset;
				found_sp = 1;
				return 1;
			}
		}
		return 0;
	}
	return 1;
}

static void check_room(char **str, int add, int size_trunk) 
{
	if (*str == NULL) {
		*str = malloc(size_trunk);
		(*str)[0] = '\0';
	} else {
		int len = strlen(*str);
		int size = (len/size_trunk +1) * size_trunk;
		if((len + add) > size)
			realloc(*str, size + size_trunk);
	}
}

