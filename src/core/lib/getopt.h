
/* getopt.h */

#ifndef __GETOPT_H__
#define __GETOPT_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


extern	int	opterr;	/* determine whether it prints error log or not */
extern	int	optind;	/* index of argv to be processed */
extern	int	optopt;	/* have a option character at detecting errors */
extern	char	*optarg;/* argument character associating with option */
extern	int	optdel;	/* delimiter on argument vector */

extern	int	getopt(int argc, char * const *argv, const char *opts);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
