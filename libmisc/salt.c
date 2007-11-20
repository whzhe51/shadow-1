/*
 * salt.c - generate a random salt string for crypt()
 *
 * Written by Marek Michalkiewicz <marekm@i17linuxb.ists.pwr.wroc.pl>,
 * public domain.
 *
 * l64a was Written by J.T. Conklin <jtc@netbsd.org>. Public domain.
 */

#include <config.h>

#ident "$Id$"

#include <sys/time.h>
#include <stdlib.h>
#include "prototypes.h"
#include "defines.h"
#include "getdef.h"

#ifndef HAVE_L64A
char *l64a(long value)
{
	static char buf[8];
	char *s = buf;
	int digit;
	int i;

	if (value < 0) {
		errno = EINVAL;
		return(NULL);
	}

	for (i = 0; value != 0 && i < 6; i++) {
		digit = value & 0x3f;

		if (digit < 2) 
			*s = digit + '.';
		else if (digit < 12)
			*s = digit + '0' - 2;
		else if (digit < 38)
			*s = digit + 'A' - 12;
		else
			*s = digit + 'a' - 38;

		value >>= 6;
		s++;
	}

	*s = '\0';

	return(buf);
}
#endif /* !HAVE_L64A */

/*
 * Add the salt prefix.
 */
#define MAGNUM(array,ch)	(array)[0]= (array)[2] = '$',\
				(array)[1]=(ch),\
				(array)[2]='\0'

/*
 * Return the salt size.
 * The size of the salt string is between 8 and 16 bytes for the SHA crypt
 * methods.
 */
static unsigned int SHA_salt_size (void)
{
	return 8 + 8*rand ()/(RAND_MAX+1);
}

/* ! Arguments evaluated twice ! */
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

/* Default number of rounds if not explicitly specified.  */
#define ROUNDS_DEFAULT 5000
/* Minimum number of rounds.  */
#define ROUNDS_MIN 1000
/* Maximum number of rounds.  */
#define ROUNDS_MAX 999999999

/*
 * Return a salt prefix specifying the rounds number for the SHA crypt methods.
 */
static char *SHA_salt_rounds (void)
{
	static char *rounds_prefix[18];
	long min_rounds = getdef_long ("SHA_CRYPT_MIN_ROUNDS", -1);
	long max_rounds = getdef_long ("SHA_CRYPT_MAX_ROUNDS", -1);
	long rounds;

	if (-1 == min_rounds && -1 == max_rounds)
		return "";

	if (-1 == min_rounds)
		min_rounds = max_rounds;

	if (-1 == max_rounds)
		max_rounds = min_rounds;

	if (min_rounds > max_rounds)
		max_rounds = min_rounds;

	rounds = min_rounds + (max_rounds - min_rounds)*rand ()/(RAND_MAX+1);

	/* Sanity checks. The libc should also check this, but this
	 * protects against a rounds_prefix overflow. */
	if (rounds < ROUNDS_MIN)
		rounds = ROUNDS_MIN;

	if (rounds > ROUNDS_MAX)
		rounds = ROUNDS_MAX;

	snprintf (rounds_prefix, 18, "rounds=%ld$", rounds);

	/* Sanity checks. That should not be necessary. */
	rounds_prefix[17] = '\0';
	if ('$' != rounds_prefix[16])
		rounds_prefix[17] = '$';

	return rounds_prefix;
}

/*
 * Generate 8 base64 ASCII characters of random salt.  If MD5_CRYPT_ENAB
 * in /etc/login.defs is "yes", the salt string will be prefixed by "$1$"
 * (magic) and pw_encrypt() will execute the MD5-based FreeBSD-compatible
 * version of crypt() instead of the standard one.
 * Other methods can be set with ENCRYPT_METHOD
 */
char *crypt_make_salt (void)
{
	struct timeval tv;
	/* Max result size for the SHA methods:
	 *  +3		$5$
	 *  +17		rounds=999999999$
	 *  +16		salt
	 *  +1		\0
	 */
	static char result[40];
	int max_salt_len = 8;
	char *method;

	result[0] = '\0';

#ifndef USE_PAM
#ifdef ENCRYPTMETHOD_SELECT
	if ((method = getdef_str ("ENCRYPT_METHOD")) == NULL) {
#endif
		if (getdef_bool ("MD5_CRYPT_ENAB")) {
			MAGNUM(result,'1');
			max_salt_len = 11;
		}
#ifdef ENCRYPTMETHOD_SELECT
	} else {
		if (!strncmp (method, "MD5", 3)) {
			MAGNUM(result, '1');
			max_salt_len = 11;
		} else if (!strncmp (method, "SHA256", 6)) {
			MAGNUM(result, '5');
			strcat(result, SHA_salt_rounds());
			max_salt_len = strlen(result) + SHA_salt_size();
		} else if (!strncmp (method, "SHA512", 6)) {
			MAGNUM(result, '6');
			strcat(result, SHA_salt_rounds());
			max_salt_len = strlen(result) + SHA_salt_size();
		} else if (0 != strncmp (method, "DES", 3)) {
			fprintf (stderr,
				 _("Invalid ENCRYPT_METHOD value: '%s'.\n"
				   "Defaulting to DES.\n"),
				 method);
			result[0] = '\0';
		}
	}
#endif				/* ENCRYPTMETHOD_SELECT */
#endif				/* USE_PAM */

	/*
	 * Concatenate a pseudo random salt.
	 */
	gettimeofday (&tv, (struct timezone *) 0);
	strncat (result, sizeof(result), l64a (tv.tv_usec));
	strncat (result, sizeof(result),
		 l64a (tv.tv_sec + getpid () + clock ()));

	if (strlen (result) > max_salt_len)	/* magic+salt */
		result[max_salt_len] = '\0';

	return result;
}
