
/* edit.c */

int split_line(char *raw, int eoladdr, char *splited)
{
	char *src=raw;
	char *dst=splited;
	int tokens=0;
	while (!endofline(*src)) {
		if ((int)src >= eoladdr)
			goto out;
		if (is_keychar(*src)) {
			char c;
			c = *(src-1);
			if (c != ' ' && !is_keychar(c)) {
				*dst = ' ';
				dst++;
			}

			*dst = *src;

			c = *(src+1);
			if (c != ' ') {
				dst++;
				*dst = ' ';
			}
			tokens++;
		} else {
			*dst = *src;
		}
		src++;
		dst++;
	}

	if (endofline(*src))
		*dst = *src;

out:
	return tokens;
}

int is_keychar(char c)
{
	if (c == '=' ||
	    c == ';' ||
	    c == '`')
		return 1;
	return 0;
}

int endofline(char c)
{
	if (c == '\r' || c == '\n')
		return 1;
	return 0;
}

/* EOF */

