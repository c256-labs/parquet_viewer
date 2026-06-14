#ifndef PV_SQL_H
#define PV_SQL_H

/* SQL escaping helpers. Both return a malloc'd string the caller must free. */

/* Escape a value as a single-quoted SQL string literal (doubles '). */
char *sql_literal(const char *s);

/* Quote an identifier with double quotes (doubles "). */
char *quote_ident(const char *s);

#endif /* PV_SQL_H */
