#pragma once

#define DUMP_ERRS 0

extern parser *create_parser(module *m);
extern void destroy_parser(parser *p);

extern unsigned tokenize(parser *p, bool args, bool consing);
extern void term_xref(parser *p, term *t, predicate *parent);
extern void reset(parser *p);
extern void consultall(parser *p, cell *l);
extern void term_to_body(parser *p);
extern cell *check_body_callable(parser *p, cell *c);
extern bool run(parser *p, const char *src, bool dump, bool is_init);
extern void xref_db(parser *p);

extern void clear_term(term *t);
extern void do_reduce(cell *n);
extern void make_end(cell *tmp);
extern void fix_list(cell *c);
extern bool check_rule(const cell *c);
extern cell *get_head(cell *c);
extern cell *get_body(cell *c);
extern cell *get_logical_body(cell *c);
