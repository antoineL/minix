
#ifndef _RRIP_H_
#define _RRIP_H_

struct px_entry_d {
    u8_t mode[8];
    u8_t links[8];
    u8_t uid[8];
    u8_t gid[8];
}__attribute__((aligned(1)));

struct sp_entry_d {
    u16_t magic;
    u8_t offset;
}__attribute__((aligned(1)));

struct sl_com {
    u8_t flag;
    u8_t len;
    char path[0];
}__attribute__((aligned(1)));

struct sl_entry_d {
    u8_t flag;
    u8_t com_area[0];
}__attribute__((aligned(1)));

struct pn_entry_d {
    u8_t h_devnd[8];
    u8_t l_devnd[8];
}__attribute__((aligned(1)));

struct nm_entry_d {
    u8_t flag;
    char name[0];
}__attribute__((aligned(1)));

struct susp_desc {
    u8_t sig[2];
    u8_t len;
    u8_t version;
    union{
        struct px_entry_d PX;
        struct sp_entry_d SP;
        struct sl_entry_d SL;
        struct pn_entry_d PN;
        struct nm_entry_d NM;
    }d;
};

struct rrip_attr {
    char* s_link;
    char* f_name;
};

#endif
