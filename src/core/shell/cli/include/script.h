#ifndef SH_SCRIPT_H
#define SH_SCRIPT_H

/* Single console session; caller owns line buffer of TTY_NBUF bytes */
void sh_script_init(void);
int sh_script_line(char *line);

#endif
