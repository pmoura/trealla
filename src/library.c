#include "library.h"

extern unsigned char library_builtins_pl[];
extern unsigned int library_builtins_pl_len;
extern unsigned char library_lists_pl[];
extern unsigned int library_lists_pl_len;
extern unsigned char library_apply_pl[];
extern unsigned int library_apply_pl_len;
extern unsigned char library_http_pl[];
extern unsigned int library_http_pl_len;
extern unsigned char library_atts_pl[];
extern unsigned int library_atts_pl_len;
extern unsigned char library_error_pl[];
extern unsigned int library_error_pl_len;
extern unsigned char library_dcgs_pl[];
extern unsigned int library_dcgs_pl_len;
extern unsigned char library_format_pl[];
extern unsigned int library_format_pl_len;
extern unsigned char library_charsio_pl[];
extern unsigned int library_charsio_pl_len;
extern unsigned char library_assoc_pl[];
extern unsigned int library_assoc_pl_len;
extern unsigned char library_ordsets_pl[];
extern unsigned int library_ordsets_pl_len;
extern unsigned char library_dict_pl[];
extern unsigned int library_dict_pl_len;
extern unsigned char library_freeze_pl[];
extern unsigned int library_freeze_pl_len;
extern unsigned char library_dif_pl[];
extern unsigned int library_dif_pl_len;

library g_libs[] = {
     {"builtins", library_builtins_pl, &library_builtins_pl_len},
     {"lists", library_lists_pl, &library_lists_pl_len},
     {"apply", library_apply_pl, &library_apply_pl_len},
     {"assoc", library_assoc_pl, &library_assoc_pl_len},
     {"ordsets", library_ordsets_pl, &library_ordsets_pl_len},
     {"dict", library_dict_pl, &library_dict_pl_len},
     {"http", library_http_pl, &library_http_pl_len},
     {"atts", library_atts_pl, &library_atts_pl_len},
     {"error", library_error_pl, &library_error_pl_len},
     {"dcgs", library_dcgs_pl, &library_dcgs_pl_len},
     {"format", library_format_pl, &library_format_pl_len},
     {"charsio", library_charsio_pl, &library_charsio_pl_len},
     {"freeze", library_freeze_pl, &library_freeze_pl_len},
     {"dif", library_dif_pl, &library_dif_pl_len},
     {0}
};
