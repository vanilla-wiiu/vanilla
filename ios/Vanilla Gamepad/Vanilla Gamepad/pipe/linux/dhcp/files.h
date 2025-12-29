/* files.h */
#ifndef _FILES_H
#define _FILES_H

struct config_keyword {
	const char *keyword;
	int (* const handler)(const char *line, void *var);
	void *var;
	const char *def;
};


int read_config(const char *file);
void write_leases(void);
void read_leases(const char *file);

#endif
