#ifndef ESH_SCRIPT_H
#define ESH_SCRIPT_H

/* Single console session; caller owns line buffer of TTY_NBUF bytes */
void esh_script_init(void);
int esh_script_line(char *line);

#endif
